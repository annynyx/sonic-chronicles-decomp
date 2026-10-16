#!/usr/bin/env bash
# The build command objdiff runs (objdiff.json, written by objdiff_config.py). objdiff
# passes the file it wants, relative to the repository:
#
#   work/build/<source>.o        compiles <source>.cpp from src/ with compilar.sh, the same
#                                compiler and flags as the real build
#   work/build/<source>.ctx.cpp  the context of a decomp.me scratch: the headers <source>.cpp
#                                includes, preprocessed. A game file not decompiled yet
#                                (jogo/...) has no source, so it gets every header in include/
#
# The libraries' objects (NitroSDK/, NitroSystem/, MSL/) are not built here: they use their
# own compilers and flags, and decomp/tools/montar_rom.sh leaves them in work/build/.
#
# Usage: decomp/tools/objdiff_build.sh work/build/src/Aurora/CRules.o
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO"
OUT="${1:?usage: objdiff_build.sh work/build/<source>.o}"
REL="${OUT#work/build/}"

case "$REL" in
    NitroSDK/*|NitroSystem/*|MSL/*)
        [ -f "$OUT" ] && exit 0
        echo "$OUT: library objects come from the full build, run decomp/tools/montar_rom.sh rom.nds" >&2
        exit 1 ;;
    *.ctx.*)
        SRC="${REL%.ctx.*}.${REL##*.}"
        TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
        if [ -f "$SRC" ]; then
            grep -E '^[[:space:]]*#[[:space:]]*include' "$SRC" > "$TMP/ctx.cpp"
        else
            { echo '#include "msl.h"'
              for h in include/Aurora/*.h; do echo "#include \"${h#include/}\""; done; } > "$TMP/ctx.cpp"
        fi
        decomp/tools/compilar.sh "$TMP/ctx.cpp" "$TMP/out" -EP
        mkdir -p "$(dirname "$OUT")"
        # -EP still writes the #line of the files; decomp.me only needs the declarations
        grep -v '^#line' "$TMP/out" | cat -s > "$OUT" ;;
    *.o)
        STEM="${REL%.o}"
        for SRC in "$STEM.cpp" "$STEM.c"; do
            [ -f "$SRC" ] && exec decomp/tools/compilar.sh "$SRC" "$OUT"
        done
        echo "$OUT: no source for it yet ($STEM.cpp)" >&2
        exit 1 ;;
    *)
        echo "objdiff_build.sh: don't know how to build $OUT" >&2
        exit 1 ;;
esac
