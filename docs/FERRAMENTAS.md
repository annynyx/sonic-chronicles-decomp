# Tools

The tools come in these groups:

- **`engine/`** (Rust): what you use day to day. Fast and tested, with binaries for Windows and Linux.
- **`analise/`** (Python, Java/Ghidra, shell): the reverse-engineering pipeline. This is where
  the findings were made.
- **`decomp/`** (C): rewritten game functions and the tests that validate them, plus
  `decomp/tools/`, the build that rebuilds the identical ROM.
- **`src/`, `include/`, `config/`** (C++ and `dsd` configuration): the *matching*
  decompilation, which compiles to the game's exact bytes.

General requirements: Rust 1.80+ (`cargo`), Python 3.10+ (`pip install ndspy capstone pillow`),
a C compiler. Optional: Ghidra 11.x (`GHIDRA_HOME`), `py-desmume` (emulator).

Command-line flags and output folder names are still in Portuguese; the translations are
given next to them.

---

## `sonic-mod`: modding

```
sonic-mod unpack <rom.nds> <project>
sonic-mod pack   <rom.nds> <project> <output.nds>
```

**unpack** creates an editable project:

| Output | Source |
|---|---|
| `tabelas/<pack>/<Table>.csv` (tables) | every GDA in the HERF packs (229), with column names |
| `textos/<language>.csv` (`id,text`) (texts) | the 5 TLKs (`en`, `fr`, `de`, `es`, `it`) |
| `arquivos/<pack>/*` (files) | 2DA text tables (`.ITM` item effects, `.SPL` moves...) |
| `LEIA-ME.md` (read me) | the [modding guide](MODDING.md) |

**pack**, inside, in order:
1. Reads the original ROM and recovers the resource names (`erf.dict`).
2. For each spreadsheet: reads the original table to know **each column's type**, converts
   the cells (with a clear file/row/column error), builds the GDA and compares it with the
   original. The writers are exact, so "equal" means you didn't touch it.
3. Files in `arquivos/`: the same name replaces the original; a new name **adds** the
   resource to the pack (with the right hash, at the right place in the sorted index, and in
   `erf.dict`).
4. Texts: changes the existing ids and **inserts new ids into the TLK's hash table**, with
   the game's own hash function.
5. Repacks the changed HERFs, recompressing with LZ10 what was `.small`.
6. Writes the files into the ROM: in their original place if they fit, otherwise at the end
   of the cartridge; updates the FAT, the used size and the header CRC16.
7. **Reopens the generated ROM** and checks that every written file comes back identical.

With no changes, `pack` says "nothing changed" and generates nothing. That is also the test
that `unpack` loses no information.

## `sonic-dump`: asset extractor

```
sonic-dump <rom.nds> <output> [--sem-imagens] [--sem-cenarios] [--sem-json] [--sem-bruto]
```
(`--sem-X` = without X: images, backgrounds, JSON, raw files.)

| Output | Contents |
|---|---|
| `bruto/` (raw) | original NitroFS files + `arm9.bin`, `arm7.bin` |
| `herf/<pack>/` | 8,901 decompressed, named resources + `_manifesto.json` |
| `textos/` (texts) | `textos_5_idiomas.csv` and `roteiro_<language>.txt` (dialog script, with character and emotion) |
| `tabelas/gda/`, `tabelas/2da/` (tables) | 229 binary and 431 text tables, as CSV |
| `json/` | every GFF4 (dialogs, areas, screens, plots) as JSON |
| `imagens/sprites/` (images) | 3,444 NCGR as PNG; `imagens/_paletas.json` gives the palette and the method used |
| `imagens/montadas/` (assembled) | 342 portraits/icons (4 pieces of 64×64 assembled into 128×128) |
| `imagens/cenarios/` (backgrounds) | 62 complete areas + `_profundidade.png` (depth) |

Palette of each sprite, in order of confidence: `gui` (the screens say), `tabela` (a GDA row
cites it), `mesmo_nome` (same name), `prefixo` (prefix, a guess), `cinza` (gray, unknown).
Runs in parallel (`rayon`): about 7 seconds for everything.

## `sonic-formats`: the library

The base of both tools above and of a future executable. Does no disk I/O.

| Module | Read | Write | Highlight |
|---|---|---|---|
| `nds` | `Rom::parse`, NitroFS | `replace_file`, `header_crc16` | swaps files with the fewest changes |
| `herf` | `Herf`, `parse_name_dict`, `recover_names` | `write`, `write_name_dict` | dictionary attack + `erf.dict` |
| `compression` | `lz10_decompress`, `open_small` | `lz10_compress`, `make_small` | no distance 1 (VRAM-safe) |
| `gff4` | `Gff4::root()` → JSON | (planned) | types 18 and 20, unique to Sonic |
| `tlk` | `Tlk::parse`, `get`, `texts` | `set`, `remove`, `to_bytes` | the game's hash table (`slot_hash`) |
| `gda` | `Gda::parse`, `cell_to_text` | `to_bytes`, `cell_from_text` | typed per column |
| `twoda` | `read` | (plain text) | |
| `nitro` | `Palette`, `Tiles::render` | | NCLR/NCGR |
| `background` | `render`, `render_depth` | | `.cbgt` backgrounds |
| `image`, `csv` | utilities | | PNG, CSV with `,` or `;` |

Tests: `cargo test` (unit) and `SONIC_ROM=rom.nds cargo test` (round trip with the real ROM:
5 TLKs, 6 HERFs, 229 GDAs identical; 400 files recompressed with LZ10 without loss).
API documentation: `cd engine && cargo doc --open`.

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
| 6 | `tools/dump_dialogs.py`, `tools/gda.py` | readable script and tables |
| 7 | `make -C decomp test` | validates the C functions against the game |
| 8 | `cargo test` (engine) | library tests |
| 9 | Ghidra headless | `ghidra_scripts/SetupSonic.java` creates ITCM/DTCM, marks code read-only (so the decompiler shows constants) and creates the functions; `DecompileAll.java` writes one `.c` per class |

Other tools:

| Tool | Use |
|---|---|
| `tools/xref.py arm9.bin symbols.txt relocs.txt out.json` | which functions use which strings (through the literal pool relocations) |
| `tools/gff4.py file [out.json]` | any GFF4 → JSON (Python reference of the Rust reader) |
| `tools/emu_run.py rom.nds folder "script"` | runs the ROM in DeSmuME without a window and performs actions: `w N` wait, `p KEY` press, `t X Y` touch the screen, `s name` screenshot, `save`/`load` state. Needs `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` |
| `tools/ghidra_symbols.py` | converts dsd symbols for Ghidra |
| `tools/manifest_pairs.py` | lists hash/name pairs of a manifest (for the C tests) |
| `tools/combate_tabelas.py project [out]` | reads a `sonic-mod unpack` project and writes the moves, effects (`.SPL`), items (`.ITM`), creatures and combat rules, already decoded, as Markdown (see [COMBATE.md](COMBATE.md)) |

Example of a mod test in the emulator (reaches the opening quote screen):
```
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy python3 analise/tools/emu_run.py rom_mod.nds shots \
  "w 400; t 128 96; w 200; t 128 96; w 200; t 128 96; w 240; t 128 96; w 240; t 128 140; w 240; t 128 96; w 300; s citacao"
```

## `decomp/`: functions rewritten in C

| File | Game function | Validation |
|---|---|---|
| `src/resource_hash.c` | `HashResourceName` (0x02009b78) | recomputes the 8,690 names in `erf.dict`: 100% |
| `src/exo_string.c` | `CExoString::CStr` (0x020052f0) | used by the previous one |
| `src/language.c` | choice of TLK by language (part of 0x020148c8) | the 7 cases of the `switch` |

```
make -C decomp test HERF_DIR=$PWD/work/herf/test          # output of run_all.sh
make -C decomp test MANIFEST=$PWD/saida/herf/_manifesto.json   # output of sonic-dump
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

## `exemplos/rust-minimo/`

A ~150-line crate with the essentials (hash, HERF, LZ10) and a `herf list/get` CLI. It is the
best starting point for someone learning Rust who wants to see the whole format at once,
without the structure of the full library.

## Publishing a version

```
git tag v0.3.0 && git push origin v0.3.0
```
or, without a command line: **Actions → release → Run workflow** tab, giving the version. The
`.github/workflows/release.yml` workflow builds on Windows and Linux (GitHub machines), runs
the tests and publishes the release with `sonic-tools-windows.zip` and
`sonic-tools-linux.tar.gz`. The normal CI (`ci.yml`) runs on every push.

## Building for Windows from Linux

```
pip install ziglang cargo-zigbuild
rustup target add x86_64-pc-windows-gnu
cd engine && cargo zigbuild --release --target x86_64-pc-windows-gnu -p sonic-mod -p sonic-dump
```
The `.exe` files only depend on system DLLs (Windows 10/11).
