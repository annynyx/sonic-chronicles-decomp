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
are tables and GFF4 files inside `test.herf`. That is what makes data modding (`sonic-mod`)
possible before the decompilation is finished.

How a resource is loaded: the code asks for a name (e.g. `"Items.GDA"`); `CErfMan` computes
`HashResourceName` (DJB2), looks up `name.small` and then `name` in the HERF's sorted index
(binary search), decompresses the `.small` (LZ10) if needed, and the format's reader
(GFF4/GDA/TLK) interprets it.

## This repository

```
engine/                       Rust (workspace): what you use day to day
  crates/sonic-formats/       library: every format, reading AND writing
  crates/sonic-dump/          CLI: extracts assets to PNG/CSV/JSON
  crates/sonic-mod/           CLI: unpack (editable project) / pack (modified game)
analise/                      reverse-engineering pipeline
  run_all.sh                  ROM -> functions, names, assembly, data, pseudo-C
  tools/                      Python: RTTI, xrefs, HERF, GFF4, GDA, dialogs, emulator
  ghidra_scripts/             Ghidra setup + bulk decompilation
  symbols_manual.txt          names given by hand, with the evidence
decomp/                       matching-decompilation tools, C rewrites + tests against the game
src/, include/                the game's matching C++
config/YWSE/                  dsd configuration (file splits, symbols, relocations)
exemplos/rust-minimo/         minimal didactic version (HERF + LZ10)
docs/                         this documentation
work/                         (not versioned) everything generated from the ROM
```

### Principles
1. **Nothing from the game in the repository.** Everything is generated from the user's ROM.
2. **Every writer is proven by a round trip.** Reading and writing without changing anything
   must give the original file byte for byte (`engine/crates/sonic-formats/tests`).
3. **Every finding is validated against the game**, never only against the hypothesis:
   recomputed hashes, backgrounds measured by their edges, mods tested in the emulator.
4. **The library knows nothing about disks or CLIs.** That way a future executable (viewer,
   port) reuses the same readers.

### Tests
```
cd engine && cargo test --release                                 # unit tests
SONIC_ROM=rom.nds cargo test --release -- --nocapture             # round trip with the real ROM
make -C decomp test MANIFEST=$PWD/saida/herf/_manifesto.json        # C functions against the game
SDL_VIDEODRIVER=dummy python3 analise/tools/emu_run.py rom.nds out "w 600; s tela"   # in the emulator
```
