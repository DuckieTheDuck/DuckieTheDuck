# Lesson 1: Your first cheat

**Goal:** get 9999 coins in DuckLab Level 1, a number you can't reach by playing.
**You'll learn:** what a plugin is, how to find a value in memory, and how to write it from a CTRPF plugin.
**Time:** about 30 minutes, plus setup.

> Only cheat in offline, single-player games. Cheating online ruins games for other people and gets accounts banned.

---

## 1. What is a plugin?

A game keeps everything it knows (your coins, your HP, the timer) as numbers in **memory** (RAM). Each number lives at an **address**, like a house number on a street.

A **CTRPF plugin** is a small program that Luma3DS loads *inside* a running game. Because it runs inside the game, it can read and change those numbers. Change the coins number, and the game believes you have that many coins.

The plugin is a `.3gx` file. Luma3DS loads it when the game starts if it finds it in:

```
/luma/plugins/<the game's title ID>/
```

DuckLab's title ID is `000400000DC1AB00`.

---

## 2. Setup

### On your 3DS
1. You need a 3DS with **Luma3DS v13 or newer**, which has the plugin loader built in.
2. Install `release/DuckLab.cia` with FBI (or any CIA installer).
   - Use the **.cia**, not the .3dsx. Plugins attach by title ID, and a .3dsx runs under the Homebrew Launcher's ID instead. DuckLab warns you on the menu if you run it as a .3dsx.
3. Turn on the plugin loader: open the Rosalina menu (**L + Down + SELECT**), go to **Plugin Loader** and enable it.

### On your PC (to build your own plugins)
1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the **3DS** packages (devkitARM + libctru).
2. Install CTRPluginFramework and 3gxtool by following the steps in the [CTRPF README](https://gitlab.com/thepixellizeross/ctrpluginframework). This adds a package source, then runs `pacman -S libctrpf`.
3. Check it works: open a terminal in `ducklab/plugin/` and run `make`. You should get `DuckLabTrainer-release.3gx`.

> **No PC setup yet?** You can still do Part A with the prebuilt `release/DuckLabTrainer.3gx`.

---

## 3. Part A: Find the coins yourself

Every CTRPF plugin comes with a built-in **memory search** tool. You'll use it to find where Level 1 keeps your coins.

1. Copy `release/DuckLabTrainer.3gx` to `/luma/plugins/000400000DC1AB00/` on your SD card. It should be the only `.3gx` in that folder.
2. Start DuckLab. After a few seconds the plugin shows **Plugin ready!**.
3. Start **Level 1** and press **A** a few times to get some coins. Say you have **4**.
4. Press **SELECT** to open the plugin menu and go to the **Search** tab (the magnifying glass).
5. Start a new search:
   - Value type: **32-bit** (u32)
   - Search for: **exact value** `4`
6. You'll get thousands of results. Lots of memory happens to hold a 4.
7. Close the menu, quack a few more times (say coins are now **9**), open the menu and search again for exact value `9` *in the previous results*.
8. Repeat until only one or two results are left. That's your coins address. 🎉

**Check your answer:** press **Y** twice in Level 1 to show the Lab Panel spoilers. They list the real addresses. For the v0.1.0 release build, coins are at `0x0012CDE0`.

> **Why does this work?** Each search keeps only the addresses whose value changed exactly like your coins did. After a few rounds, only the real one is left.

> CTRPF's menu labels can differ slightly between versions. If a name doesn't match, look for the closest option.

### No "Plugin ready!" and SELECT does nothing?
- **Plugin Loader is off.** Turn it on in the Rosalina menu (L + Down + SELECT).
- **Wrong folder.** The file must be in `/luma/plugins/000400000DC1AB00/`. Check the title ID: it's all zeros except `0DC1AB`.
- **Missing the homebrew flag.** Luma silently skips plugins for homebrew games like DuckLab unless the plugin allows it. The plugin `Makefile` sets this flag for you with `tools/allow_homebrew_load.py`. If you make a plugin with a different Makefile for a homebrew game, run that script on your `.3gx`.
- **"Fatal plugin error ... abort()" on boot.** CTRPF expects the button and graphics code that retail games use, and homebrew games built with libctru don't have it. The example plugin handles this: `settings.UseGameHidMemory = false;` for buttons, and `Sources/libctru_compat.cpp` for graphics. Copy both into any plugin you make for a homebrew game. Retail games don't need them.
- **"Outdated plugin loader" message.** Update Luma3DS.
- **"Couldn't set memblock size."** Memory from an earlier plugin run is still in use, usually after a game crashed or closed badly. Turn the 3DS fully off and on again.

---

## 4. Part B: Write the cheat

Open `ducklab/plugin/Sources/main.cpp`. The key part is:

```cpp
static const u32 COINS_ADDRESS = 0x0012CDE0;

static void MaxCoins(MenuEntry *entry)
{
    Process::Write32(COINS_ADDRESS, 9999);
}
```

- `Process::Write32(address, value)` writes a 32-bit number into the game's memory.
- `MaxCoins` is a **game function**: while its entry is ticked in the menu, CTRPF calls it **every frame**. Writing every frame means the value stays put, which is how "freeze" cheats work.

The menu entry that connects it:

```cpp
*level1 += new MenuEntry("Max coins (9999)", MaxCoins,
    "Writes 9999 to the coins address every frame.");
```

### Try it
1. Build: `make` in `ducklab/plugin/`.
2. Copy `DuckLabTrainer-release.3gx` to `/luma/plugins/000400000DC1AB00/` (replace the old one).
3. Start DuckLab, then Level 1. Press **SELECT** and tick **Max coins (9999)**.
4. Close the menu. **LEVEL 1 HACKED!** ⭐

---

## 5. Challenges

Do these yourself before checking the example plugin, which already solves them:

1. **Bonus star:** get **999 HP**. Bread only heals up to 100, so you need a cheat. *(Hint: find HP the same way you found coins. Get hit to change it.)*
2. **Freeze the timer** at 60 seconds.
3. **Read instead of write:** make a menu entry that shows your coins in a message box using `Process::Read32`. *(The example's "Show my coins" does this.)*
4. **Your own idea:** add an entry that sets coins to 9998, so one quack wins.

---

## What you learned
- Games store values at memory addresses.
- You find an address by searching, changing the value, and searching again.
- `Process::Write32` changes a value. Doing it every frame freezes it.
- A plugin goes in `/luma/plugins/<title ID>/`.

**Next: Lesson 2, Moving Target.** In Level 2 the duck moves to a new address every time the level starts, so a fixed address won't work. You'll learn **pointers**.
