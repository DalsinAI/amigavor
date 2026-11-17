#!/bin/sh
# The release archive, for Cradle's stove_build (target package): the drawer
# built by amiga/Makefile.amigaos, then the drawer, its icon and the Installer
# script beside it packed as NAME-VERSION-68040.lha (tools/lha_archive.py),
# with its .sha256 and a short .readme. VERSION is the Installer script's
# $VER, so the archive and the Installer always name the same version.
# package/package.env names the drawer (NAME), the make target (TARGET) and
# where it leaves the drawer (DIST). The SDL kit, the NDK and the libraries
# are the stove's own (prefix/m68k-amigaos). MAKEVARS, if set, is passed to
# make as well (package.env says when a port needs it). Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
. "$HERE/package.env"
OUT=${OUT:-"$ROOT/build-amiga-package"}
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
cd "$ROOT"

# A stove's compiler is in its prefix/ (or STOVE is the prefix itself).
if [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then PREFIX="$STOVE/prefix"; else PREFIX="$STOVE"; fi
KIT="$PREFIX/m68k-amigaos"

make -f amiga/Makefile.amigaos STOVE="$PREFIX" NDK="$KIT/ndk-include" SDL2KIT="$KIT" SDLKIT="$KIT" \
    DEPS="$KIT/port-deps" -j2 $TARGET ${MAKEVARS:-} > "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }

python3 "$ROOT/tools/pack_dist.py" --dist "$DIST" --name "$NAME" --out "$OUT"
