// DuckLab Trainer - the example plugin from Lesson 1.
//
// Build:   make            (needs devkitARM, libctru, libctrpf and 3gxtool)
// Install: copy DuckLabTrainer-release.3gx to
//          /luma/plugins/000400000DC1AB00/DuckLabTrainer.3gx on your SD card
// Use:     start DuckLab, wait for "Plugin ready!", press SELECT

#include <3ds.h>
#include <CTRPluginFramework.hpp>

namespace CTRPluginFramework
{
    // Level 1 addresses for the DuckLab v0.1.0 release build.
    // If you build DuckLab yourself these can move: check the Lab Panel
    // spoilers (press Y twice in Level 1) and update them here.
    static const u32 COINS_ADDRESS = 0x0012CDE0;
    static const u32 HP_ADDRESS    = 0x0012B004;
    static const u32 TIMER_ADDRESS = 0x0012B008;

    // A "game function" runs once per frame while its entry is ticked.
    // Writing the same value every frame is how a "freeze" cheat works.

    static void MaxCoins(MenuEntry *entry)
    {
        Process::Write32(COINS_ADDRESS, 9999);
    }

    static void SuperHp(MenuEntry *entry)
    {
        Process::Write32(HP_ADDRESS, 999);
    }

    static void FreezeTimer(MenuEntry *entry)
    {
        Process::Write32(TIMER_ADDRESS, 60);
    }

    // Example of reading memory: show the current coin count once.
    static void ShowCoins(MenuEntry *entry)
    {
        u32 coins = 0;
        if (Process::Read32(COINS_ADDRESS, coins))
            MessageBox(Utils::Format("You have %lu coins.", coins))();
        else
            MessageBox("Couldn't read that address. Is DuckLab running?")();
    }

    // Runs before the game starts.
    void PatchProcess(FwkSettings &settings)
    {
        // Retail games read buttons with Nintendo's SDK code, which CTRPF can
        // hook. DuckLab is built with libctru, so CTRPF must read the buttons
        // itself or it aborts on boot ("Fatal plugin error ... abort()").
        settings.UseGameHidMemory = false;
    }

    int main(void)
    {
        PluginMenu *menu = new PluginMenu("DuckLab Trainer", 0, 1, 0,
            "Lesson 1 cheats for DuckLab.\nOnly cheat in offline games!");

        // Run cheats in sync with the game's frames.
        menu->SynchronizeWithFrame(true);

        MenuFolder *level1 = new MenuFolder("Level 1: Plain Values");
        *level1 += new MenuEntry("Max coins (9999)", MaxCoins,
            "Writes 9999 to the coins address every frame.");
        *level1 += new MenuEntry("Super HP (999)", SuperHp,
            "Bonus star: HP above the normal max of 100.");
        *level1 += new MenuEntry("Freeze timer", FreezeTimer,
            "Keeps the timer at 60 seconds.");
        *level1 += new MenuEntry("Show my coins", nullptr, ShowCoins,
            "Reads memory instead of writing it.");
        *menu += level1;

        menu->Run();

        delete menu;
        return 0;
    }
}
