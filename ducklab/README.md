# 🦆 DuckLab

**The 3DS game you're supposed to cheat in.**

DuckLab teaches you how to make your own [CTRPluginFramework](https://gitlab.com/thepixellizeross/ctrpluginframework) (CTRPF) cheat plugins. Every level has a goal you can't reach by playing normally, and each one hides its values in memory in a harder way. You practise on a game you own, so you don't need any commercial game to learn.

| Level | Name | What it teaches |
|---|---|---|
| 1 | Plain Values | Memory search, static addresses, `Process::Write32` |
| 2 | Moving Target | Heap memory, pointers and offsets, floats |
| 3 | Locked Vault | XOR-encoded values, checksums, beating a simple anti-cheat |

Each level has a ⭐ goal (9999 coins) and a ⭐ bonus. Stars save to `sdmc:/3ds/DuckLab/progress.txt`.

> Only use what you learn here in offline, single-player games.

## Get started

1. Install `release/DuckLab.cia` on a 3DS running Luma3DS v13+.
2. Follow **[Lesson 1: Your first cheat](lessons/01-your-first-cheat.md)**.

### Controls
| Button | Action |
|---|---|
| A | Quack (+1 coin) |
| B | Buy bread (heal 20 HP for 10 coins) |
| Y | Lab Panel: hints → spoilers → off |
| SELECT | Opens the CTRPF plugin menu (when a plugin is loaded) |
| START | Back to menu / quit |

## What's in here

```
ducklab/
├── game/       DuckLab itself (C, libctru)
├── plugin/     DuckLab Trainer, the example CTRPF plugin (C++)
├── lessons/    Step-by-step lessons
├── release/    Prebuilt DuckLab.cia, DuckLab.3dsx, DuckLabTrainer.3gx
└── tools/      make_assets.py (icon, banner and quack sound)
```

## Building

You need [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the 3DS packages.

**Game**
```sh
cd game
make        # DuckLab.3dsx
make cia    # DuckLab.cia (also needs makerom and bannertool on your PATH)
```

**Plugin** (also needs libctrpf and 3gxtool, see the CTRPF README)
```sh
cd plugin
make        # DuckLabTrainer-release.3gx and -debug.3gx
```

> **Addresses depend on the build.** If you build the game yourself with a different toolchain version, Level 1's addresses can move. The Lab Panel spoilers (press Y twice) always show the right ones for your build. The lessons use the addresses of the `release/` build.

## Details
- Title ID: `000400000DC1AB00`, so plugins go in `/luma/plugins/000400000DC1AB00/`.
- `plugin/Sources/libctru_compat.cpp` lets CTRPF run inside homebrew (libctru) games. CTRPF normally relies on code from Nintendo's SDK that only retail games have. `tools/allow_homebrew_load.py` sets the 3GX flag Luma3DS needs before it loads a plugin into a homebrew title.
- The plugin's `3gx.ld` linker script comes from CTRPF and keeps CTRPF's license (`plugin/LICENSE-CTRPF.txt`).

## Roadmap
- [x] DuckLab game: 3 levels
- [x] Lesson 1: Your first cheat
- [ ] Lesson 2: Moving Target (pointers)
- [ ] Lesson 3: Locked Vault (encoded values)
- [ ] PC app: one-click setup, no-code cheat builder, code converter
