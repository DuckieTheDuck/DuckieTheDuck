"""Set the "allowHomebrewLoad" flag in a .3gx plugin file.

Luma3DS skips plugins for games built with libctru (like DuckLab) unless
this flag is set, and it does so without showing an error. 3gxtool has no
setting for the flag yet, so the plugin Makefile runs this after 3gxtool.

Usage: python3 allow_homebrew_load.py plugin.3gx
"""
import struct
import sys

MAGIC_PREFIX = b"3GX$"
FLAGS_OFFSET = 0x30           # magic(8) + version(4) + reserved(4) + 8 info words(32)
ALLOW_HOMEBREW_LOAD = 1 << 9  # bit 9 of the info flags, see Luma3DS plugin/3gx.h


def main(path):
    with open(path, "r+b") as f:
        header = f.read(FLAGS_OFFSET + 4)
        if not header.startswith(MAGIC_PREFIX):
            sys.exit(f"{path}: not a 3GX file")
        (flags,) = struct.unpack_from("<I", header, FLAGS_OFFSET)
        f.seek(FLAGS_OFFSET)
        f.write(struct.pack("<I", flags | ALLOW_HOMEBREW_LOAD))
    print(f"{path}: homebrew loading allowed (flags 0x{flags:08X} -> 0x{flags | ALLOW_HOMEBREW_LOAD:08X})")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
