# The technologies and why we use them

A matching decompilation has one goal that decides every choice: C and C++ that the
**original compiler** turns into the **same bytes** as the game. A tool is only here if it
helps reach that goal, or helps prove it was reached. This page says what each one does in
the project and why it was chosen over the alternatives.

## What we are rebuilding

| | Fact | Why it matters |
|---|---|---|
| Console | Nintendo DS: ARM946E-S (ARMv5TE) runs the game, an ARM7 handles sound and Wi-Fi | the project decompiles the ARM9 code and its ITCM/DTCM, where the game lives |
| Game code | **Thumb** (16-bit instructions) for 6,309 of the 6,542 main ARM9 functions; ARM for the rest and for the fast ITCM routines | the compiler mode (`-thumb`) and the disassembly mode change per function |
| Engine | BioWare's **Aurora** engine (the family of Neverwinter Nights and Dragon Age), in C++ with RTTI | the class names come from the RTTI ([hierarquia_classes.md](hierarquia_classes.md)), and NWN's public knowledge helps name methods |
| Libraries | NitroSDK 4.2.30001, NitroSystem 071126, Metrowerks MSL and Runtime | they have public sources or original `.o` files, so they are linked instead of rewritten ([COMPILADOR.md](COMPILADOR.md)) |
| Proof | ROM SHA-1 `f4ff8291b38bf9a33ab280fb9f7625cf4d549473` | a file counts only when the rebuilt ROM has this SHA-1 |

## The build

| Tool | What it does here | Why this one |
|---|---|---|
| [**dsd** (ds-decomp)](https://github.com/AetiasHax/ds-decomp) v0.12.1 | extracts the ROM, finds the 6,820 functions, splits the ARM9 into `.o` files (*delinking*), writes the linker script and rebuilds the ROM | made for the DS: it understands the ARM9, the ITCM/DTCM, the autoloads and the ROM layout, so a whole game can be rebuilt from day one and replaced file by file. The pinned release avoids a bug of its `main` branch ([BUILD.md](BUILD.md)) |
| **mwccarm 2.0 sp2** and **mwldarm** (Metrowerks CodeWarrior for DS) | compile our C/C++ and link the game | it is the compiler the game was built with (Phase 1, [COMPILADOR.md](COMPILADOR.md)). A different compiler, even a better one, gives different bytes, and then nothing can be proven. sp2 is the version whose MSL and Runtime match the game's |
| [**wibo**](https://github.com/decompals/wibo) 0.6.16 | runs the 32-bit Windows compilers on Linux | much smaller and faster than Wine, made by the decomp community for exactly this; it lets CI run the compiler on a stock Ubuntu machine |
| [decompme/compilers](https://github.com/decompme/compilers) | where the compiler package comes from | the same package [decomp.me](https://decomp.me) uses, so a function tried there behaves the same here |
| [**metroskrew**](https://github.com/mid-kid/metroskrew) 0.1.3 | provides the MSL and Runtime libraries of each CodeWarrior version | to find which version's libraries are in the game, and to link them from their original `.o` files |
| [ntrtwl/NitroSDK](https://github.com/ntrtwl/NitroSDK) and [ntrtwl/NitroSystem](https://github.com/ntrtwl/NitroSystem) | the community decompilations of Nintendo's libraries | the game uses those exact versions, so 86 NitroSDK files and all of NitroSystem link from source instead of being decompiled again |

All of them are downloaded by `decomp/tools/ferramentas.sh`, with **fixed versions checked by
SHA-256**: the same build today and in a year. None of them is stored in Git.

## Checking that it matches

| Tool | What it does here | Why |
|---|---|---|
| `decomp/tools/montar_rom.sh` | rebuilds the whole ROM and compares its SHA-1 | the final proof: if one byte differs, the file is not marked `complete` |
| `decomp/tools/comparar.py` ([Capstone](https://www.capstone-engine.org/)) | compares a compiled function with the game's, instruction by instruction | while writing a function, a byte diff says *that* it differs; the disassembly says *where* and *why* (a register, a stack slot) |
| [objdiff](https://github.com/encounter/objdiff), set up by `decomp/tools/objdiff_config.py` | the same comparison in a window: every function of a file next to the game's, recompiled on each save, and a decomp.me scratch from any function | what most decompilations use, so contributors already know it; `dsd` writes its project file, and our build script makes it use `compilar.sh`, the compiler and flags of the real build ([BUILD.md](BUILD.md#comparing-with-objdiff)) |
| `decomp/tools/conferir_sem_rom.py` ([pyelftools](https://github.com/eliben/pyelftools)) | CI's check: the SHA-1 of each decompiled function against `decomp/compilador/esperado.txt` | CI has no ROM (it can't, legally), but a hash of each function's bytes is not the game's code, so CI can still say whether the C++ still compiles to the game |
| `decomp/tools/progresso.py` and `progress_report.py` | the progress metric, and the same numbers as an [objdiff](https://github.com/encounter/objdiff) report | objdiff's `report.json` is the format decomp trackers read ([recomp.fyi](https://recomp.fyi)); the metric itself counts only files the rebuild accepts, so there is no partial credit |
| `decomp/tests/test_arquivos.cpp` (any C++ compiler) | compiles the engine's C++ from `src/` for the PC and runs it on the game's real files | a byte match proves the code is the game's, not that we understand it; this test checks behavior on every table and file, and caught mistakes a match can't |
| [clang-format](https://clang.llvm.org/docs/ClangFormat.html) 18 (`.clang-format`) | one style for the C and C++ | written to fit the code that already exists, and checked only on the lines a pull request changes, so old files are not rewritten (which would get in the way of work in progress); include order and `> >` are left alone because the compiler is C++98 and the order can change its output |
| GitHub Actions | runs all the checks above that need no ROM on every push | a pull request that breaks a matching function turns red before anyone merges it |

## Understanding the code

| Tool | What it does here | Why |
|---|---|---|
| `analise/tools/rtti.py`, `apply_names.py`, `xref.py` (Python, Capstone) | recover the 290 C++ classes and their vtables from the RTTI, name ~2,230 functions, find which functions use which strings | the RTTI is the only place where the game keeps real class names; our own scripts, because they read the Metrowerks RTTI layout, which generic tools don't |
| [ndspy](https://github.com/RoadrunnerWMC/ndspy) | reads the ROM and its file system, decompresses LZ10 | a mature, tested DS library; no point in rewriting it |
| [Ghidra](https://ghidra-sre.org/) (headless) | pseudo-C of every function, for reading | the best free decompiler with ARMv5T and Thumb support; `analise/ghidra_scripts/SetupSonic.java` maps the ITCM/DTCM and loads our names. Its output is only a reading aid: it stays in `work/` and never goes into Git |
| `docs/DIARIO.md` | the journal: every hypothesis, how it was tested, every mistake | a decompilation is a long chain of small proofs; writing down why a choice was made, and when it was wrong, keeps the chain checkable |

## The people and the AI

Most of the matching work is done by a **Claude Code agent** (Anthropic) and reviewed by the
maintainer. That is disclosed in the README and in `.recomp.json`. It is safe for the same
reason the rest of the project is: nothing is taken on anyone's word. A function the agent
writes counts only when CI says its bytes match and the rebuilt ROM is identical.

## What we chose not to use

| Not used | Why |
|---|---|
| Wine | wibo does the same job for these compilers, smaller and faster |
| A modern compiler (GCC, Clang) for the game code | it can't produce the original bytes; it is used only for the PC test |
| splat / decomp-toolkit | made for other consoles (N64, PS2, GameCube/Wii); dsd is the DS equivalent, also used by other DS decomps such as Phantom Hourglass, Spirit Tracks and Advance Wars: Dual Strike |
| Committing assembly, pseudo-C or game files | they are the game's code and data; everything derived from the ROM is generated locally from your own copy |
| AI or decompiler output as proof | both are guesses; only the compiler and the SHA-1 decide |
