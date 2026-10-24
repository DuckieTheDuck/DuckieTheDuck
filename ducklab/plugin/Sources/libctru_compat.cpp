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
//  3. Hooks libctru's gspPresentBuffer (called every time the game shows a
//     frame) and runs CTRPF's frame callback there. CTRPF needs this to draw
//     notifications, to know a new frame happened (the menu loop waits for
//     that before checking the SELECT hotkey), and to pause the game while
//     the menu is open.

#include <3ds.h>
#include <CTRPluginFramework.hpp>
#include "libctru_compat.hpp"

// Part of CTRPF: tells its GPU code which thread slot, event and shared
// memory the game got from the GPU service.
extern "C" void __gsp__Update(u32 threadId, Handle eventHandle, Handle sharedMemHandle);

namespace CTRPluginFramework
{
    // Part of CTRPF (not in its public headers): runs once per presented
    // frame. This is what CTRPF's own screen hook calls in retail games.
    class OSDImpl
    {
    public:
        static void CallbackCommon(u32 isBottom, void *addr, void *addrB, int stride, int format, int swap);
    };
}

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
    static Hook      s_presentHook;
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

    // Runs in place of libctru's gspPresentBuffer().
    static u32 PresentBufferHook(u32 screen, u32 swap, void *fbA, void *fbB, u32 stride, u32 mode)
    {
        if (fbA)
            OSDImpl::CallbackCommon(screen, fbA, fbB, stride, mode, swap);

        return HookContext::GetCurrent().OriginalFunction<u32>(screen, swap, fbA, fbB, stride, mode);
    }

    // Instruction tests (any condition, any registers).
    static bool IsLdrex(u32 w)     { return (w & 0x0FF00FFF) == 0x01900F9F; }
    static bool IsStrex(u32 w)     { return (w & 0x0FF00FF0) == 0x01800F90; }
    static bool IsOrr0x100(u32 w)  { return (w & 0x0FF00FFF) == 0x03800C01; }
    static bool IsAdd0x200(u32 w)  { return (w & 0x0FF00FFF) == 0x02800C02; }
    static bool IsPushLr(u32 w)    { return (w & 0xFFFF4000) == 0xE92D4000; }

    // Finds libctru's gspPresentBuffer: a short function that adds 0x200 to
    // reach the framebuffer info, then sets the "update" flag (| 0x100) with
    // an ldrex/strex loop. Returns 0 unless exactly one match is found.
    static u32 FindPresentBuffer(void)
    {
        const vu32 *text = reinterpret_cast<vu32 *>(0x00100000);
        const u32 words = Process::GetTextSize() / 4;
        u32 found = 0;

        for (u32 i = 48; i + 8 < words; ++i)
        {
            if (!IsLdrex(text[i]))
                continue;

            bool strex = false, orr = false, add = false;
            for (u32 j = i + 1; j <= i + 8; ++j)
            {
                strex |= IsStrex(text[j]);
                orr |= IsOrr0x100(text[j]);
            }
            for (u32 j = i - 40; j < i; ++j)
                add |= IsAdd0x200(text[j]);
            if (!strex || !orr || !add)
                continue;

            u32 start = 0;
            for (u32 j = i; j > i - 48; --j)
            {
                if (IsPushLr(text[j]))
                {
                    start = 0x00100000 + j * 4;
                    break;
                }
            }
            if (!start)
                continue;
            if (found && found != start)
                return 0; // ambiguous, better not to guess
            found = start;
        }
        return found;
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
        if (s_sendSyncHook.Enable() != HookResult::Success)
            return false;

        u32 present = FindPresentBuffer();
        if (!present)
            return false;

        s_presentHook.InitializeForMitm(present, (u32)PresentBufferHook);
        return s_presentHook.Enable() == HookResult::Success;
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
