# Sonic Chronicles: The Dark Brotherhood decompilation

A matching decompilation of **Sonic Chronicles: The Dark Brotherhood** (Nintendo DS, BioWare,
2008, US version `YWSE`): C and C++ source that the original compiler turns back into the exact
same game, byte for byte. In the end, all of its code decompiled is also the base for a native
PC executable.

> **Nothing from the game is in this repository.** No ROM, no assets, no assembly, no
> generated pseudo-C. Every tool works from a copy of the game that you own.

## The spirit of the project

This is a **learning by doing** project, with one rule above all: *no finding counts until it
has been checked against the real game*. Every decompiled file is rebuilt and compared byte
by byte with the original; every rewritten engine class is also tested against thousands of
real records from the game files. When a hypothesis was wrong, and several were,
the mistake and its fix are written down, because that is how one learns. Read
[`docs/ESPIRITO.md`](docs/ESPIRITO.md) and the journal of findings in
[`docs/DIARIO.md`](docs/DIARIO.md).

**AI-assisted.** Much of the matching work is done by a Claude Code agent and reviewed by the
maintainer. Nothing is taken on its word: a file only counts as decompiled when the rebuilt
game is byte-identical to the original (SHA-1 `f4ff8291b38bf9a33ab280fb9f7625cf4d549473`).

## Status

| Front | State |
|---|---|
| **Formats** | ✅ ROM, HERF, LZ10, GFF4, TLK, GDA, 2DA, NCGR/NCLR and backgrounds documented ([`docs/FORMATOS.md`](docs/FORMATOS.md)) |
| **Code** | 🟡 6,820 functions mapped, 290 C++ classes recovered, ~2,230 functions named, pseudo-C of everything (generated locally, never committed) |
| **Matching decompilation** | 🟡 **14.91% of the code comes from source** (`decomp/tools/progresso.py`: libraries 87.71%, game 1.71%). Phases 0 and 1: the game is rebuilt byte-identical ([`docs/BUILD.md`](docs/BUILD.md)), compiler found (mwccarm 2.0 sp2, `-ipa file`, [`docs/COMPILADOR.md`](docs/COMPILADOR.md)). In C++ in the build: all of `CExoString` (33 functions), the allocator (`new`/`delete`, `NitroStartUp`), the file system (`CDSFileSystem`, HERF packs and `.small` reading, 45 functions), the GDA tables with GFF4 reading (52 functions) and the text 2DA tables (21 functions); 215 functions checked in CI, including 8 of the 9 in `CRules`; NitroSystem, 86 NitroSDK files and 62 MSL files linked from the libraries' source; 84% of the game code already split into files; ~1,100 library functions with their real names. Plan: [`docs/PLANO-DECOMPILACAO.md`](docs/PLANO-DECOMPILACAO.md) |
| **PC port** | ⏳ Phase 7 of the plan, after the decompilation |

## Roadmap: where we are

The full plan, with a "done when" criterion for each part, is in
[`docs/PLANO-DECOMPILACAO.md`](docs/PLANO-DECOMPILACAO.md). Phase 3 is the one being worked on
now.

| Phase | What | State |
|---|---|---|
| **0. Matching build** | rebuild the game from the assembly, byte-identical | ✅ done |
| **1. Compiler** | find the compiler and flags (mwccarm 2.0 sp2, `-O4,p`, Thumb, `-ipa file`) | ✅ done |
| **2. Libraries** | link the Nintendo and Metrowerks libraries from their source and split the game code into files | 🟡 almost done: NitroSystem 100%, NitroSDK 89%, MSL 74% (87.71% of the library code); 84% of the game code split into files |
| **3. Aurora engine core** | the engine classes everything else uses | 🟡 **in progress**: strings (`CExoString`) ✅, memory (allocator, `Singleton<T>`, `CExoArrayList<T>`) ✅, file system ✅ (7 template copies still in assembly), data tables (GDA/GFF4 ✅, 2DA ✅, `CTlkTable` 12 of 13 functions); now on **3.5, the game rules**: `CRules` 8 of 9 functions match, `LoadRules` is next |
| **4. Game objects** | `CGameObject`, items, creatures, the party | ⏳ next |
| **5. Game modes and interface** | exploration, combat, conversations, the `.gui` screens | ⏳ |
| **6. Graphics and sound** | backgrounds, models, effects, video | ⏳ |
| **7. PC port** | a native executable on top of the decompiled code | ⏳ can start during Phase 4 |

Want to help? The parts that help most right now are matching C++ for the engine classes of
Phase 3 and 4, and the GFF4 and map script formats. Read [`CONTRIBUTING.md`](CONTRIBUTING.md)
first.

### How progress is measured

`decomp/tools/progresso.py` counts the code bytes (ARM9 `.text` and `.init`, ITCM `.text`)
of the files marked `complete` in `config/YWSE/arm9/delinks.txt`. A file is only marked
complete when `decomp/tools/montar_rom.sh` rebuilds an identical game, so there is no partial
credit. CI also writes the same numbers as an [objdiff](https://github.com/encounter/objdiff)
progress report (`decomp/tools/progress_report.py`) and uploads it as the `YWSE_report`
artifact, which progress trackers such as [recomp.fyi](https://recomp.fyi) read. The project's
row there comes from [`.recomp.json`](.recomp.json).

## Getting started

**Rebuild the game from source:** see [`docs/BUILD.md`](docs/BUILD.md). In short:
```bash
decomp/tools/ferramentas.sh                 # downloads dsd, wibo and mwccarm into work/
decomp/tools/montar_rom.sh /path/rom.nds    # rebuilds the game and checks that it is identical
python3 decomp/tools/progresso.py           # how much of the code comes from source
```

**Study the code:** `./analise/run_all.sh /path/rom.nds` runs the reverse-engineering pipeline
(function map, C++ classes from the RTTI, names, assembly and, with Ghidra, pseudo-C). Its
output stays in `work/`, never in Git.

## Repository map

```
analise/             reverse-engineering pipeline (Python, Ghidra)
decomp/              build and checking tools for the matching decompilation, C tests
src/, include/       the game's matching C++ (Aurora engine classes)
config/YWSE/         dsd configuration: file splits (delinks), symbols, relocations
docs/                all documentation (below)
```

## Documentation

| Document | What for |
|---|---|
| [`docs/ESPIRITO.md`](docs/ESPIRITO.md) | the principles: how and why we work this way |
| [`docs/BUILD.md`](docs/BUILD.md) | rebuilding the game from source, step by step |
| [`docs/COMPILADOR.md`](docs/COMPILADOR.md) | the compiler, its flags, and what makes code match |
| [`docs/FERRAMENTAS.md`](docs/FERRAMENTAS.md) | **every tool**: what it does, how to use it, how it works inside |
| [`docs/ARQUITETURA.md`](docs/ARQUITETURA.md) | how the game and this repository are organized |
| [`docs/FORMATOS.md`](docs/FORMATOS.md) | reference for every decoded format |
| [`docs/DIARIO.md`](docs/DIARIO.md) | the story of the findings, mistakes and fixes included |
| [`docs/PLANO-DECOMPILACAO.md`](docs/PLANO-DECOMPILACAO.md) | the plan for the real decompilation, phase by phase |
| [`docs/hierarquia_classes.md`](docs/hierarquia_classes.md) | the game's 290 C++ classes |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | rules for contributing |

File names, script names and some code identifiers are still in Portuguese (the project
started in Portuguese); the documentation is in English.

## Credits

- [ds-decomp (`dsd`)](https://github.com/AetiasHax/ds-decomp): function analysis, delinking and disassembly
- [objdiff](https://github.com/encounter/objdiff): the progress report format
- [xoreos](https://github.com/xoreos/xoreos): documentation of the Aurora formats
- [NitroSDK](https://github.com/ntrtwl/NitroSDK) and [NitroSystem](https://github.com/ntrtwl/NitroSystem)
  sources from ntrtwl
- [ndspy](https://github.com/RoadrunnerWMC/ndspy), [Capstone](https://www.capstone-engine.org/),
  [Ghidra](https://ghidra-sre.org/)
- Inspiration: [pret/pokeplatinum](https://github.com/pret/pokeplatinum) and its
  [PC port](https://github.com/cybervisi0n/pokeplatinum)

## License and legal notice

Code under **GPL-3.0** ([`LICENSE`](LICENSE)). Sonic Chronicles: The Dark Brotherhood is ©
SEGA / BioWare. This is a fan project, not affiliated with them and non-commercial. Use it
only with a copy you own, and do not distribute ROMs, assets or extracted material.
