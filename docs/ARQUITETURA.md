# Architecture

## The game

Sonic Chronicles runs on **BioWare's Aurora engine** (the same family as NWN, KotOR and
Dragon Age), adapted to the DS and written in C++ (CodeWarrior, with RTTI and exceptions).

```
 ┌────────────────────────── ARM9 (1.1 MB of code) ─────────────────────────────┐
 │ Game:  GameMode (16 modes) · GuiManager · CGameObject and subclasses · Puzzles│
 │ Aurora engine: CExoString · CDSFileSystem/CErfMan · CTlkTable · CRules · GFF4│
 │ NitroSystem (NNS: G3D, G2D, sound) · MSL C++ (Metrowerks) · NitroSDK 4.2     │
 └───────────────────────────────────────────────────────────────────────────────┘
 Data (NitroFS):   test.herf (8,691 resources) · test_[efgis].herf (per language)
                   strings*.tlk (texts) · <area>.cbgt/.pal/.2da/.cdpth (backgrounds)
                   *.vx (videos) · sound_data.sdat (sound)
```

The game is **data-driven**: items, creatures, shops, chapters, dialogs, areas and screens
are tables and GFF4 files inside `test.herf`. The engine code that reads them (Phase 3 of
the plan) is the first part of the game being decompiled.

How a resource is loaded: the code asks for a name (e.g. `"Items.GDA"`); `CErfMan` computes
`HashResourceName` (DJB2), looks up `name.small` and then `name` in the HERF's sorted index
(binary search), decompresses the `.small` (LZ10) if needed, and the format's reader
(GFF4/GDA/TLK) interprets it.

## This repository

```
analise/                      reverse-engineering pipeline
  run_all.sh                  ROM -> functions, names, assembly, pseudo-C
  tools/                      Python: RTTI, xrefs, HERF, GFF4
  ghidra_scripts/             Ghidra setup + bulk decompilation
  symbols_manual.txt          names given by hand, with the evidence
decomp/                       matching-decompilation tools, C rewrites + tests against the game
src/, include/                the game's matching C++
config/YWSE/                  dsd configuration (file splits, symbols, relocations)
docs/                         this documentation
work/                         (not versioned) everything generated from the ROM
```

### Principles
1. **Nothing from the game in the repository.** Everything is generated from the user's ROM.
2. **Matching code counts only when the rebuilt game is identical** (SHA-1
   `f4ff8291b38bf9a33ab280fb9f7625cf4d549473`, [BUILD.md](BUILD.md)).
3. **Every finding is validated against the game**, never only against the hypothesis:
   recomputed hashes, backgrounds measured by their edges, the engine's C++ run on the real
   game files.

### Tests
```
python3 decomp/tools/conferir_sem_rom.py           # decompiled functions match (no ROM needed)
decomp/tools/montar_rom.sh rom.nds                 # the rebuilt game is identical
make -C decomp test HERF_DIR=$PWD/work/herf        # C functions against the game (after run_all.sh)
make -C decomp arquivos                            # the engine's C++ on the game files (after montar_rom.sh)
```
