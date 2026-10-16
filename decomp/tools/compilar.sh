#!/usr/bin/env bash
# Compila um arquivo C++ do jogo com o compilador e as flags originais.
# É o único lugar com as flags: montar_rom.sh e testar_compilador.sh usam este.
# Como cada uma foi descoberta: docs/COMPILADOR.md.
#
# Uso: decomp/tools/compilar.sh fonte.cpp saida.o [flags extras]
#      MWCC_VERSAO=2.0/sp2 decomp/tools/compilar.sh ...   (testar outra versão)
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
F="${FERRAMENTAS:-$REPO/work/ferramentas}"
# 2.0/sp2: o MSL e o Runtime ligados no jogo são os da 2.0 sp2 (docs/COMPILADOR.md)
VERSAO="${MWCC_VERSAO:-2.0/sp2}"
# The flags that decide the generated code. `compilar.sh --flags` prints them, for
# decomp.me scratches (objdiff_config.py), which add -c and -proc arm946e on their own.
CODE_FLAGS=(
    -O4,p               # otimização máxima, priorizando velocidade
    -thumb              # o código do jogo é Thumb (o NitroSDK é ARM)
    -interworking       # chamadas entre ARM e Thumb
    -enum int -char signed
    -lang=c++
    -Cpp_exceptions off # o .exceptix do jogo só tem entradas da biblioteca (MSL)
    -RTTI on            # as classes do jogo têm typeinfo (rtti.py)
    -ipa file           # só assim o nome de cada classe (_ZTS) vem antes das strings
                        # do arquivo na .data, como no jogo (conferir_ipa.py)
    -w off
)
if [ "${1:-}" = --flags ]; then echo "${CODE_FLAGS[*]}"; exit 0; fi
SRC="$1"; OUT="$2"; shift 2
FLAGS=(
    -c
    -proc arm946e       # a CPU principal do DS
    "${CODE_FLAGS[@]}"
    -gccinc -i "$REPO/include"
)
mkdir -p "$(dirname "$OUT")"
exec "$F/bin/wibo" "$F/mwccarm/$VERSAO/mwccarm.exe" "${FLAGS[@]}" "$@" "$SRC" -o "$OUT"
