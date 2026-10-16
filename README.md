# Sonic Chronicles: The Dark Brotherhood: decomp and modding

Reverse engineering of **Sonic Chronicles: The Dark Brotherhood** (Nintendo DS, BioWare, 2008).
The goal is to understand the game from the inside, to be able to **modify it** and, in the
end, to have all of its code decompiled, the base for a native PC executable.

> **Nothing from the game is in this repository.** No ROM, no assets, no game code.
> Every tool works from a copy of the game that you own.

## The spirit of the project

This is a **learning by doing** project, with one rule above all: *no finding counts until it
has been checked against the real game*. Every decoded format is rewritten and compared byte
by byte with the original; every rewritten function is tested against thousands of real
records; every mod is opened in the emulator. When a hypothesis was wrong, and several were,
the mistake and its fix are written down, because that is how one learns. Read
[`docs/ESPIRITO.md`](docs/ESPIRITO.md) and the journal of findings in
[`docs/DIARIO.md`](docs/DIARIO.md).

**AI-assisted.** Much of the matching work is done by a Claude Code agent and reviewed by the
maintainer. Nothing is taken on its word: a file only counts as decompiled when the rebuilt
game is byte-identical to the original (SHA-1 `f4ff8291b38bf9a33ab280fb9f7625cf4d549473`).

## Status

| Front | State |
|---|---|
| **Modding** | ✅ `sonic-mod`: edit items, creatures, shops and texts in spreadsheets and build the modified game. Tested in the emulator. |
| **Assets** | ✅ `sonic-dump`: 62 backgrounds, 3,444 sprites, 342 portraits, texts in 5 languages, 660 tables, in ~7 s |
| **Formats** | ✅ ROM, HERF, LZ10, GFF4, TLK, GDA, 2DA, NCGR/NCLR and backgrounds: read **and written** byte for byte |
| **Code** | 🟡 6,820 functions mapped, 290 C++ classes recovered, ~2,230 functions named, pseudo-C of everything (generated locally, never committed) |
| **Matching decompilation** | 🟡 **14.91% of the code comes from source** (`decomp/tools/progresso.py`: libraries 87.71%, game 1.71%). Phases 0 and 1: the game is rebuilt byte-identical ([`docs/BUILD.md`](docs/BUILD.md)), compiler found (mwccarm 2.0 sp2, `-ipa file`, [`docs/COMPILADOR.md`](docs/COMPILADOR.md)). In C++ in the build: all of `CExoString` (33 functions), the allocator (`new`/`delete`, `NitroStartUp`), the file system (`CDSFileSystem`, HERF packs and `.small` reading, 45 functions), the GDA tables with GFF4 reading (52 functions) and the text 2DA tables (21 functions); NitroSystem, 86 NitroSDK files and 62 MSL files linked from the libraries' source; 84% of the game code already split into files; ~1,100 library functions with their real names. Plan: [`docs/PLANO-DECOMPILACAO.md`](docs/PLANO-DECOMPILACAO.md) |
| **PC port** | ⏳ Phase 7 of the plan; today the (modified) game runs on PC through an emulator |

### How progress is measured

`decomp/tools/progresso.py` counts the code bytes (ARM9 `.text` and `.init`, ITCM `.text`)
of the files marked `complete` in `config/YWSE/arm9/delinks.txt`. A file is only marked
complete when `decomp/tools/montar_rom.sh` rebuilds an identical game, so there is no partial
credit. CI also writes the same numbers as an [objdiff](https://github.com/encounter/objdiff)
progress report (`decomp/tools/progress_report.py`) and uploads it as the `YWSE_report`
artifact, which progress trackers such as [recomp.fyi](https://recomp.fyi) read. The project's
row there comes from [`.recomp.json`](.recomp.json).

## Getting started

**I only want to mod the game (Windows):** download `sonic-tools-windows.zip` from the
[Releases](../../releases), then:
```powershell
.\sonic-mod.exe unpack "Sonic Chronicles.nds" my_mod
# edit my_mod\tabelas\test\Items.csv, my_mod\textos\en.csv ...
.\sonic-mod.exe pack "Sonic Chronicles.nds" my_mod modded.nds
```
Full guide: [`docs/MODDING.md`](docs/MODDING.md). To play, use melonDS or DeSmuME.

**I want the assets:** `sonic-dump "Sonic Chronicles.nds" output`

**I want to build and study:**
```bash
cd engine && cargo build --release && cargo test --release
SONIC_ROM=/path/rom.nds cargo test --release           # round-trip proof with the real game
./analise/run_all.sh /path/rom.nds                     # full reverse-engineering pipeline
```

**I want to rebuild the game from source** (the matching decompilation): see
[`docs/BUILD.md`](docs/BUILD.md). In short, `decomp/tools/ferramentas.sh` downloads the tools
(dsd, wibo, mwccarm) and `decomp/tools/montar_rom.sh rom.nds` rebuilds the game and checks
that it is identical.

## Repository map

```
engine/              Rust: sonic-formats (library), sonic-mod, sonic-dump
analise/             reverse-engineering pipeline (Python, Ghidra, emulator)
decomp/              build and checking tools for the matching decompilation, C tests
src/, include/       the game's matching C++ (Aurora engine classes)
config/YWSE/         dsd configuration: file splits (delinks), symbols, relocations
exemplos/            a minimal, didactic Rust crate (HERF + LZ10)
docs/                all documentation (below)
```

## Documentation

| Document | What for |
|---|---|
| [`docs/ESPIRITO.md`](docs/ESPIRITO.md) | the principles: how and why we work this way |
| [`docs/BUILD.md`](docs/BUILD.md) | rebuilding the game from source, step by step |
| [`docs/COMPILADOR.md`](docs/COMPILADOR.md) | the compiler, its flags, and what makes code match |
| [`docs/FERRAMENTAS.md`](docs/FERRAMENTAS.md) | **every tool**: what it does, how to use it, how it works inside |
| [`docs/MODDING.md`](docs/MODDING.md) | modding guide with recipes (shipped with every `sonic-mod` project) |
| [`docs/ARQUITETURA.md`](docs/ARQUITETURA.md) | how the game and this repository are organized |
| [`docs/FORMATOS.md`](docs/FORMATOS.md) | reference for every decoded format |
| [`docs/COMBATE.md`](docs/COMBATE.md) | **combat from the inside**: formulas, moves, status effects, items, Chao, difficulty |
| [`docs/DIARIO.md`](docs/DIARIO.md) | the story of the findings, mistakes and fixes included |
| [`docs/PLANO-DECOMPILACAO.md`](docs/PLANO-DECOMPILACAO.md) | the plan for the real decompilation, phase by phase |
| [`docs/hierarquia_classes.md`](docs/hierarquia_classes.md) | the game's 290 C++ classes |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | rules for contributing |

File names, script names and some code identifiers are still in Portuguese (the project
started in Portuguese); the documentation is in English.

## Credits

- [ds-decomp (`dsd`)](https://github.com/AetiasHax/ds-decomp): function analysis, delinking and disassembly
- [objdiff](https://github.com/encounter/objdiff): the progress report format
- [xoreos](https://github.com/xoreos/xoreos): documentation of the Aurora formats and the GDA
  column name table (`engine/crates/sonic-formats/data/gda_columns_xoreos.json`, GPLv3)
- [NitroSDK](https://github.com/ntrtwl/NitroSDK) and [NitroSystem](https://github.com/ntrtwl/NitroSystem)
  sources from ntrtwl
- [ndspy](https://github.com/RoadrunnerWMC/ndspy), [Capstone](https://www.capstone-engine.org/),
  [Ghidra](https://ghidra-sre.org/), [py-desmume](https://github.com/SkyTemple/py-desmume)
- Inspiration: [pret/pokeplatinum](https://github.com/pret/pokeplatinum) and its
  [PC port](https://github.com/cybervisi0n/pokeplatinum)

## License and legal notice

Code under **GPL-3.0** ([`LICENSE`](LICENSE)). Sonic Chronicles: The Dark Brotherhood is ©
SEGA / BioWare. This is a fan project, not affiliated with them and non-commercial. Use it
only with a copy you own, and do not distribute ROMs, assets or extracted material.
