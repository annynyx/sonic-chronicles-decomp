# Tools

The tools come in these groups:

- **`analise/`** (Python, Java/Ghidra, shell): the reverse-engineering pipeline. This is where
  the findings were made.
- **`decomp/`** (C): rewritten game functions and the tests that validate them, plus
  `decomp/tools/`, the build that rebuilds the identical ROM.
- **`src/`, `include/`, `config/`** (C++ and `dsd` configuration): the *matching*
  decompilation, which compiles to the game's exact bytes.

General requirements: Python 3.10+ (`pip install ndspy capstone pyelftools`), a C and C++
compiler, and the tools `decomp/tools/ferramentas.sh` downloads (dsd, wibo, mwccarm). Optional:
Ghidra 11.x (`GHIDRA_HOME`).

Command-line flags and output folder names are still in Portuguese; the translations are
given next to them.

---

## `analise/`: reverse-engineering pipeline

`./analise/run_all.sh rom.nds [--sem-ghidra]` runs everything in sequence inside `work/`.

| Step | Tool | What it does |
|---|---|---|
| 1 | `tools/extract_rom.py` | extracts ARM9, ARM7 and NitroFS (`ndspy`) |
| 2 | `dsd init` | finds the 6,820 functions and each one's ARM/Thumb mode |
| 3 | `tools/rtti.py` | reads the C++ RTTI: 290 classes, 351 vtables; names virtual methods, constructors and destructors by **behavior** (who writes the vtable into `this`) |
| 3b | `tools/apply_names.py` | applies `symbols_manual.txt` (names given by hand, with the evidence) |
| 4 | `dsd dis` | full assembly with the names |
| 5 | `tools/herf.py` | extracts HERF packs (`erf.dict` + dictionary attack) |
| 6 | `make -C decomp test` | validates the C functions against the game |
| 7 | Ghidra headless | `ghidra_scripts/SetupSonic.java` creates ITCM/DTCM, marks code read-only (so the decompiler shows constants) and creates the functions; `DecompileAll.java` writes one `.c` per class |

Other tools:

| Tool | Use |
|---|---|
| `tools/xref.py arm9.bin symbols.txt relocs.txt out.json` | which functions use which strings (through the literal pool relocations) |
| `tools/gff4.py file [out.json]` | any GFF4 → JSON (reference reader for the format) |
| `tools/ghidra_symbols.py` | converts dsd symbols for Ghidra |
| `tools/manifest_pairs.py` | lists hash/name pairs of a manifest (for the C tests) |

## `decomp/`: functions rewritten in C

| File | Game function | Validation |
|---|---|---|
| `src/resource_hash.c` | `HashResourceName` (0x02009b78) | recomputes the 8,690 names in `erf.dict`: 100% |
| `src/exo_string.c` | `CExoString::CStr` (0x020052f0) | used by the previous one |
| `src/language.c` | choice of TLK by language (part of 0x020148c8) | the 7 cases of the `switch` |

```
make -C decomp test HERF_DIR=$PWD/work/herf/test          # output of run_all.sh
```
These are *non-matching* decompilations (same behavior, not the same bytes), easy to test on
the PC. The *matching* versions (the game's exact bytes) live in `src/`.

## `decomp/tools/`: the matching build

| Script | What it does |
|---|---|
| `ferramentas.sh` | downloads `dsd`, `wibo` and the `mwccarm` compilers into `work/ferramentas/` (SHA-256 checked) |
| `baixar_rom.sh` | in the cloud: downloads the ROM from the link in `SONIC_ROM_URL` (accepts a Google Drive share link) to `work/rom/ywse.nds` and checks its SHA-1, never printing the link |
| `montar_rom.sh rom.nds` | rebuilds the ROM from the assembly and `src/` and compares the SHA-1 |
| `compilar.sh source.cpp out.o` | compiles with the game's compiler and flags |
| `comparar.py file.o symbol address` | compares a compiled function with the game's, instruction by instruction |
| `conferir_sem_rom.py` | what CI runs: the SHA-1 of each decompiled function against `decomp/compilador/esperado.txt` |
| `progresso.py` | the progress metric: share of code bytes that come from compiled source |
| `progress_report.py` | the same metric as an objdiff `report.json` (CI uploads it as `YWSE_report`) |
| `achar_funcoes.py lib.a ...` | finds in the game the functions of a compiled library (and writes the names with `--aplicar`) |
| `nitrosdk.sh` | downloads the decompiled NitroSDK 4.2, compiles it as Thumb and names its functions in the game |
| `nitrosystem.sh` | the same for NitroSystem 071126 (3D, 2D, sound); needs `nitrosdk.sh` first |
| `msl.sh` | takes the MSL `.o` files out of the CodeWarrior 2.0 sp2 `.a` archives, into `work/bibliotecas/msl/` |
| `ligar_bibliotecas.py` | splits a library's region into files and makes the build link its compiled `.o` files |
| `mapa_arquivos.py` | finds pieces of the game code that certainly belong to a single file (through the strings) and writes them to `delinks.txt` with `--aplicar` |
| `preparar_link.py` | (called by `montar_rom.sh`) adjusts the copies of the library `.o` files so the link comes out the same as Nintendo's |
| `conferir_chamadas.py file.o ...` | checks names through calls: does X's `bl` in the game land on the function Y that the source calls? |
| `versao_msl.sh` | which CodeWarrior version provided the game's MSL |
| `testar_compilador.sh` | matrix of versions × optimizations for the functions in `decomp/compilador/casos.txt` |
| `gerar_config.sh rom.nds` | generates `config/YWSE/` from scratch (wipes hand edits) |
| `banner_sem_perda.py`, `crc_area_segura.py` | the two adjustments that make the ROM identical |

The step by step and the reason for each stage: [`BUILD.md`](BUILD.md). The compiler:
[`COMPILADOR.md`](COMPILADOR.md).
