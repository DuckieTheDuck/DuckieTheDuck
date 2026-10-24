# DuckLab handoff notes

Context for a new Claude Code session picking up this project.

## Status (working on real hardware)
- `game/`: DuckLab, a libctru 3DS game meant to be cheated in. 3 levels: static values, heap pointer + offsets, XOR-encoded vault with checksum. Installs as CIA, title ID `000400000DC1AB00`.
- `plugin/`: DuckLab Trainer, a CTRPF plugin. Level 1 cheats work; menu opens with SELECT.
- `lessons/01-your-first-cheat.md`: Lesson 1, including a troubleshooting list from real errors.
- `release/`: prebuilt `DuckLab.cia`, `DuckLab.3dsx`, `DuckLabTrainer.3gx`.

## Fixes that made it work (don't undo)
1. `game/DuckLab.rsf` needs `MemoryType: Application`. Without it Luma's loader panics (0xD8E00402).
2. Luma silently skips plugins for libctru titles unless the 3GX `allowHomebrewLoad` flag (bit 9 of info flags at file offset 0x30) is set. 3gxtool can't set it, so the plugin Makefile runs `tools/allow_homebrew_load.py`.
3. `settings.UseGameHidMemory = false` in `PatchProcess`. CTRPF otherwise aborts looking for SDK HID code.
4. `plugin/Sources/libctru_compat.cpp` makes CTRPF work in libctru games:
   - writes CTRPF's searched-for SDK GSP pattern into zero padding at the end of the game's text pages (never executed), so `GSP::Initialize` succeeds;
   - hooks libctru `svcSendSyncRequest`; on `RegisterInterruptRelayQueue` (0x00130042) it forwards thread index, event and shared memory to CTRPF's `__gsp__Update` once `main()` runs;
   - MITM-hooks libctru `gspPresentBuffer` (found by heuristic: ldrex/strex + `|0x100` + `+0x200`) to call `OSDImpl::CallbackCommon`, so frames, notifications, SELECT and pausing work.
5. "Couldn't set memblock size" (0xD8A077F9) = memory left over from a crashed plugin run. Reboot the 3DS.

## Build notes
- devkitARM + libctru; CTRPF from https://gitlab.com/thepixellizeross/ctrpluginframework (needs `-Wno-error=unused-but-set-variable` with GCC 16, build single-threaded, and copy `Library/include/*.h` into the install if built from source); 3gxtool; makerom + bannertool for the CIA.
- Level 1 addresses (release build): coins `0x0012CDE0`, hp `0x0012B004`, timer `0x0012B008`.

## Next ideas (pick with the user)
- **DuckBridge:** a PC tool that talks to Luma Rosalina's GDB stub over Wi-Fi so Claude can read/write/search memory, diff snapshots, set watchpoints and screenshot, then generate CTRPF cheats. Validate on DuckLab first (known addresses). Offline, single-player games the user owns only.
- Lesson 2 (pointers), Lesson 3 (encoded vault), PC teaching app, move DuckLab to its own repo.

## Rules
Teach and build cheats for offline, single-player games only, in games the user owns. No online cheating, no piracy.
