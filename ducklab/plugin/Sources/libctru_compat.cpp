// Lets CTRPluginFramework run inside games built with libctru (homebrew).
//
// You don't need this file for retail games. CTRPF finds a retail game's
// graphics setup by searching for code from Nintendo's SDK, then hooks it.
// libctru does the same setup differently, so the search fails and CTRPF
// aborts with "Fatal plugin error". This file fixes that in two steps:
//
//  1. Writes a copy of the SDK code CTRPF searches for into unused padding at
//     the end of the game's code, so CTRPF's search succeeds. That copy is
//     never run.
//  2. Hooks libctru's svcSendSyncRequest (every service call goes through it)
//     and, when the game registers with the GPU service, hands CTRPF the same
//     details its own hook would have collected.

#include <3ds.h>
#include <CTRPluginFramework.hpp>
#include "libctru_compat.hpp"

// Part of CTRPF: tells its GPU code which thread slot, event and shared
// memory the game got from the GPU service.
extern "C" void __gsp__Update(u32 threadId, Handle eventHandle, Handle sharedMemHandle);

namespace CTRPluginFramework
{
namespace LibctruCompat
{
    // GSPGPU::RegisterInterruptRelayQueue request header.
    static const u32 REGISTER_RELAY_QUEUE = 0x00130042;

    // The Nintendo SDK function CTRPF's GSP::Initialize() searches for.
    static const u32 SDK_PATTERN[] =
    {
        0xE92D4070, 0xE1A06003, 0xE59D5010, 0xEE1D3F70, 0xE2834080,
        0xE59F3034, 0xE3A0C000, 0xE584100C, 0xE9841004, 0xE5843000,
        0xE5900000, 0xEF000032, 0xE2101102, 0x4A000004, 0xE5940008,
        0xE5850000, 0xE5940010, 0xE5860000, 0xE5940004, 0xE8BD8070,
        REGISTER_RELAY_QUEUE,
    };
    static const u32 SDK_PATTERN_WORDS = sizeof(SDK_PATTERN) / sizeof(u32);

    static Hook      s_sendSyncHook;
    static LightLock s_lock;
    static bool      s_ctrpfReady = false; // CTRPF's GPU code is set up
    static bool      s_haveInfo = false;   // the game has registered
    static u32       s_threadId = 0;
    static Handle    s_event = 0;
    static Handle    s_sharedMem = 0;

    static void ForwardToCtrpf(void)
    {
        __gsp__Update(s_threadId, s_event, s_sharedMem);
    }

    // Runs in place of the game's `svc 0x32` (send a service request).
    extern "C" Result DuckLab_SendSyncRequestHook(Handle session)
    {
        u32 *cmd = getThreadCommandBuffer();

        if (cmd[0] != REGISTER_RELAY_QUEUE)
            return svcSendSyncRequest(session);

        Handle event = cmd[3]; // read before the reply overwrites it
        Result res = svcSendSyncRequest(session);

        if (R_SUCCEEDED(res) && R_SUCCEEDED((Result)cmd[1]))
        {
            LightLock_Lock(&s_lock);
            s_threadId = cmd[2];
            s_event = event;
            s_sharedMem = cmd[4];
            s_haveInfo = true;
            bool ready = s_ctrpfReady;
            LightLock_Unlock(&s_lock);

            if (ready)
                ForwardToCtrpf();
        }
        return res;
    }

    // Finds SDK_PATTERN_WORDS zero words at the end of the game's code pages.
    static u32 FindPadding(void)
    {
        const u32 textStart = 0x00100000;
        const u32 textEnd = textStart + Process::GetTextSize();
        u32 zeros = 0;

        for (u32 addr = textEnd - 4; addr >= textStart; addr -= 4)
        {
            if (*reinterpret_cast<vu32 *>(addr) != 0)
                break;
            if (++zeros == SDK_PATTERN_WORDS + 1) // +1 so we don't touch real code's last word
                return addr + 4;
        }
        return 0;
    }

    bool Install(void)
    {
        LightLock_Init(&s_lock);

        // Retail game? Then CTRPF can do everything itself.
        std::vector<u32> pattern(SDK_PATTERN, SDK_PATTERN + SDK_PATTERN_WORDS);
        if (Utils::Search<u32>(0x00100000, Process::GetTextSize(), pattern))
            return true;

        u32 sendSync = Utils::Search<u32>(0x00100000, Process::GetTextSize(), { 0xEF000032, 0xE12FFF1E });
        u32 padding = FindPadding();
        if (!sendSync || !padding)
            return false;

        if (!Process::Patch(padding, (void *)SDK_PATTERN, sizeof(SDK_PATTERN)))
            return false;

        s_sendSyncHook.Initialize(sendSync, (u32)DuckLab_SendSyncRequestHook)
                      .SetFlags(USE_LR_TO_RETURN);
        return s_sendSyncHook.Enable() == HookResult::Success;
    }

    void OnCtrpfReady(void)
    {
        LightLock_Lock(&s_lock);
        s_ctrpfReady = true;
        bool have = s_haveInfo;
        LightLock_Unlock(&s_lock);

        if (have)
            ForwardToCtrpf();
    }
}
}
