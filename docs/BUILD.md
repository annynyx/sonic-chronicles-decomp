# "Matching" build: rebuilding the identical ROM

This is the result of **Phase 0** of the [plan](PLANO-DECOMPILACAO.md): the game's ROM is
taken apart into pieces, linked again with Metrowerks' original linker and comes out
**identical byte for byte** to yours (same SHA-1). From there on, every function decompiled
to C++ takes the place of the matching piece, and this same build proves that nothing
changed.

```
decomp/tools/ferramentas.sh                  # once: dsd, wibo and mwccarm in work/ferramentas/
decomp/tools/montar_rom.sh your_copy.nds     # rebuilds and checks: "IDÊNTICA" (identical)
```

Requirements: Linux x86-64 (or WSL), `python3` with `pip install capstone pyelftools pillow`,
`curl`, `unzip`. The build takes about 7 seconds. The expected ROM (USA, `YWSE`) has SHA-1
`f4ff8291b38bf9a33ab280fb9f7625cf4d549473`.

(The scripts still print in Portuguese: `IDÊNTICA` = identical, `DIFERENTE` = different.)

**In a cloud session**, the ROM arrives through a private link in the `SONIC_ROM_URL`
environment variable. `decomp/tools/baixar_rom.sh` downloads it to `work/rom/ywse.nds` and
checks the SHA-1. It accepts a Google Drive share link. For that, the file must be shared as
"Anyone with the link", and the environment's network must allow `drive.google.com` and
`drive.usercontent.google.com`. The variable and the network rule only apply to sessions
opened after the change.

> None of this goes into Git: `work/` is in `.gitignore`. The repository only holds the
> scripts, the decompiled C++ (`src/`, `include/`) and the configuration (`config/`), which
> has addresses, sizes and names, not a single byte of the game.

---

## What each step does and why

| Step | Tool | What happens |
|---|---|---|
| 1. extract | `dsd rom extract` | splits the ROM into code (`arm9.bin`, ITCM, DTCM, `arm7.bin`), header, banner and the 303 files |
| 2. delink | `dsd delink` | cuts the ARM9 into ELF object files (`.o`) |
| 2b. compile | `mwccarm` | compiles the files in `src/` marked as done |
| 3. lcf | `dsd lcf` | writes the linker script: the order of each `.o` in memory |
| 4. link | `mwldarm` | the original linker joins everything and resolves the addresses |
| 5. rom | `dsd rom config/build` | assembles the `.nds` |
| 6. check | `dsd check` + SHA-1 | compares ARM9/ITCM/DTCM and the whole ROM with the original |

### Delink: the opposite of linking

A compiler produces one `.o` per source file. Inside it, the code doesn't know where it is
going to live: where there is a `bl CExoString::CStr`, the `.o` only keeps a
**relocation**, a note saying "the linker puts the address of `CStr` here". The linker
decides the addresses and fills in the notes.

`dsd delink` goes the other way. It takes the finished binary and, using the lists
`config/YWSE/arm9/symbols.txt` (where each function and each piece of data starts) and
`relocs.txt` (every place in the code that points to another address, 60 thousand of them),
recreates `.o` files with the notes in place of the addresses. If a relocation is wrong or
missing, the addresses come out different after the link and step 6 reports it.

That is why the identical build is the **safety net**: it proves the symbol and relocation
map is right, before a single line of C++.

### The files in `config/YWSE/arm9/`

- `delinks.txt`: the ARM9's sections (`.text` code, `.rodata` constants, `.data`, `.bss`...)
  and, below, the **source files** already identified, with the piece of each section that
  belongs to them. What is not in any file becomes a `_dsd_gap` (a "hole" that is still the
  original code). A file marked `complete` has already been decompiled: the build uses the
  `.o` compiled from `src/` instead of the cut one. (Game files not yet decompiled are named
  `jogo/...`, *jogo* = game.)
- `symbols.txt`: every function and piece of data with address, kind (ARM/Thumb) and size.
  The names come from RTTI and from `analise/symbols_manual.txt`. A decompiled function uses
  the mangled name the compiler produces (`_ZNK10CExoString4CStrEv`), so that the rest of the
  code, which calls that name, finds it.
- `relocs.txt`: the relocations.

`decomp/tools/gerar_config.sh rom.nds` generates all of this from scratch (`dsd init` +
names). It **wipes** hand edits: after the first time, `config/` is edited and versioned.

### Two details outside the code

The first rebuild came out with **20 different bytes**, none of them code:

1. **The banner icon (16 bytes + CRC).** `dsd` saves the icon as a color PNG and, on the way
   back, each pixel is converted to the **first** palette index with that color. The game's
   palette has two equal entries (2 and 4 = `0x2083`), so every pixel with index 4 came back
   as 2. `decomp/tools/banner_sem_perda.py` rewrites the PNGs using the 3 low bits of each
   channel, which the DS ignores (a DS color has 5 bits, PNG has 8), so each index gets a
   unique PNG color.
2. **The secure area CRC (2 bytes + header CRC).** The first 16 KB of the ARM9 are encrypted
   on the cartridge, and the header keeps the CRC of that encrypted version. The key is in
   the ARM7 BIOS, which we don't have (and can't distribute), and without it `dsd` writes 0.
   `decomp/tools/crc_area_segura.py` checks that the rebuilt 16 KB are identical to the
   original's and only then copies the CRC. If someone changes the start of the ARM9, it
   refuses; then the BIOS is needed (`dsd rom build --arm7-bios bios7.bin`).

### When the build fails

```
DIFERENTE. Primeiros bytes que mudaram:
   offset 0xdba6 (ARM9 0x02009ba6): 2c -> 64
```
("DIFFERENT. First bytes that changed:")

The ARM9 address points to the function (look in `symbols.txt` for the one that contains the
address). Compile the file and compare only that function:

```
decomp/tools/compilar.sh src/Aurora/CDSFileSystem.cpp /tmp/x.o
python3 decomp/tools/comparar.py /tmp/x.o --listar
python3 decomp/tools/comparar.py /tmp/x.o _Z16HashResourceNameRK10CExoString 0x02009b78
```

`comparar.py` shows the game's assembly and yours side by side, with `!!` on the different
instructions and `R` on the bytes the linker will still fill in.

### A decompiled file enters the build

When every function of a piece matches in `comparar.py`, it enters the build like this (the
example is `CExoString`):

1. In `delinks.txt`, the file gets `complete` and the ranges of each section the `.o`
   produces: `.text` and, if any, `.data` (strings, vtable, typeinfo, the `2/π` marker),
   `.rodata`, `.bss`, `.init` (the `__sinit`) and `.ctor` (the 4 bytes that point to it). The
   `.text` goes up to the multiple of 4. `src/Aurora/Memory.cpp` has all of them.
2. `python3 decomp/tools/nomes_do_fonte.py src/Aurora/CExoString.cpp --aplicar` compiles the
   source, fits each function and each piece of data of the `.o` into the range, comparing
   the bytes, and writes into `symbols.txt` the names the compiler produces
   (`_ZN10CExoStringC1EPKc`...), which is how the rest of the game will call them. A
   relocation into the middle of a symbol (the vtable + 8) becomes "symbol + offset" in
   `relocs.txt`. When the `.o` has two equal copies (the `C1` and the `C2` of a constructor)
   and the game only one, the name is that of the copy the `.o` itself uses, or `C1`; the
   linker discards the other one. That also holds when the compiler puts a multidef copy
   between the two (the base class's inline destructor, in `C2DA`: DIARIO 30). If anything
   doesn't fit, the script stops.
   The "multidef" copies (the methods of `CExoArrayList<T>`, inline constructors that were
   not expanded, typeinfos, the object of a `Singleton<T>`) come out in every `.o` that uses
   them, and the game kept the one from the first file linked. Beforehand, give them in
   `symbols.txt` the name the compiler produces, at the address where the game kept them
   (`main.cpp` has many): the script skips the ones named outside the range, and
   `preparar_link.py` makes the linker use the game's. A copy that doesn't match at the end
   of the file (the file system's `DelIndex`) stays out of the range and remains in
   assembly; ours gives way all the same. Alignment-1 data (strings, class names) counts as
   aligned to 4, the way the linker places it.
3. The functions go into `decomp/compilador/casos.txt`, and
   `python3 decomp/tools/conferir_sem_rom.py --gerar work/extract/arm9/arm9.bin` rewrites the
   hashes, so CI can check without the ROM.
4. `montar_rom.sh` has to say IDÊNTICA, and `progresso.py` shows the new number.
5. If the file belongs to the engine (Phase 3), it goes into the real-data test:
   `decomp/tests/test_arquivos.cpp` compiles the C++ of `src/Aurora` on the PC, without
   changing anything, with the NitroSDK and MSL functions made for the PC in
   `decomp/tests/nitro_pc.cpp`, and runs it on the files `montar_rom.sh` extracted:

   ```
   make -C decomp arquivos            # uses work/extract/files
   ```

   The game's source works for both sides thanks to `include/msl.h`, which has one branch
   for `mwccarm` (which defines `__MWERKS__`) and another for the PC.

When a function in the middle of the file doesn't match, the file doesn't enter the build
yet: the `.o` takes the whole range, and splitting the source into two `.cpp` files doesn't
work (each would get its own `__sinit`). The functions that match stay in the source and go
into `casos.txt` (step 3), and the one that doesn't stays out, with the attempt in the
DIARIO. That is the case of `src/Aurora/CTlkTable.cpp` (DIARIO 28). A template copy at the
**end** of the file doesn't block anything: it stays out of the range, as in step 2.

---

## Nintendo's libraries, linked from source

All of NitroSystem (from `0x020c8278` to `0x020d4394`, 59 files), 86 of the 87 NitroSDK
files and 62 of the 68 files of the MSL (Metrowerks' C/C++ library) that the game uses no
longer come from the cut assembly. The build compiles the `.c` files of the public SDK and
NitroSystem source (see
[COMPILADOR.md](COMPILADOR.md#nitrosystem-100-of-the-region-recognized)), takes the MSL
`.o` files out of CodeWarrior 2.0 sp2's `.a` archives, links everything, and the ROM stays
identical. That is 132 KB, **13.47% of the ARM9 code** (with the ITCM; the count is
[below](#the-progress-metric)). The first time, `montar_rom.sh` prepares the libraries by
itself (`nitrosdk.sh`, `nitrosystem.sh` and `msl.sh`, into `work/bibliotecas/`).

From the NitroSDK, `gx_vramcnt.c` is missing: the function `GX_SetBankForSubBG`
(`0x020d6930`) does the same as the source, but its `switch` came out with another tree of
comparisons. It is not the compiler (the ten 2.0 versions give the same result), nor the
order of the `case`s, nor the optimization options tested: the version of the file Nintendo
compiled must be slightly different from the public one.

From the MSL, six files are missing. Two define a C++ template (`shared_ptr<char>`,
`move_ptr`) that the game also has, in a single copy: linked, they would give "defined
twice". In `CPP_locale` and `C_math_arm` the functions appear in the game in another order,
and in `CPP_iostream` and `C_ansi_fp` a function in the middle doesn't match. The list comes
from `ligar_bibliotecas.py MSL ...` (the "não liga", "doesn't link", lines).

Five things in the link that only showed up with the SDK:

- **Functions nobody calls.** The game has `OS_DisableProtectionUnit`, which nothing calls.
  The original linker kept it; ours, with `-dead`, throws it away. `montar_rom.sh` puts a
  `FORCE_ACTIVE` block in the `.lcf` with the global functions of the linked files. (The
  command-line `-force_active` option aborts, without a message, when the list goes over
  ~256 characters.)
- **"Weak" functions.** `OS_Terminate` and `OS_Halt` are declared weak in the SDK (the game
  could have its own version). `mwcc` records that with its own code (14), and `symbols.txt`
  must say `weak`.
- **Symbols the linker computed.** `os_arena.c`, `os_thread.c` and others use
  `SDK_SYS_STACKSIZE`, `SDK_MAIN_ARENA_LO`, `SDK_AUTOLOAD_DTCM_START`... They are not in any
  `.c`: Nintendo's `.lcf` defined them. They are in `config/YWSE/arm9/simbolos_linker.lcf`,
  which `montar_rom.sh` appends to the `.lcf` generated by `dsd`, with values checked
  against the ones the game has written in its code.
- **One file in three modules.** `os_irqHandler.c` has `OS_WaitIrq` in the ARM9,
  `OS_IrqHandler` in the ITCM (the fast memory, for the interrupt) and a variable in the
  DTCM. It is a single `.o`, but `dsd` cuts each module separately and doesn't accept the
  same name in two. So the ITCM part appears in `itcm/delinks.txt` as `os_irqHandler.itcm.c`
  (and the DTCM one as `.dtcm.c`), and `montar_rom.sh` replaces `os_irqHandler.itcm.o(.text)`
  in the `.lcf` with `os_irqHandler.o(.itcm)`. The `static`s one part uses from the other
  (`OSi_DoResetSystem`) become global in `symbols.txt`, otherwise `dsd` refuses; `dsd check
  symbols` points out that difference and `montar_rom.sh` lets only it through.
- **The `.version` signature.** Files like `card_backup.c` have a string
  `[SDK+NINTENDO:BACKUP]` in a `.version` section, which Nintendo's `.lcf` placed right after
  the crt0 (at `0x02000bd4`). It goes in through the same trick, as `card_backup.version.c`,
  and the `.lcf` asks for `card_backup.o(.version)`.

The MSL brought other problems, because Nintendo's linker received the whole `.a` archives
and kept only what the game uses, while ours receives one `.o` per file next to the
assembly. `decomp/tools/preparar_link.py` adjusts the copies of the `.o` files in
`work/build` before the link (the ones in `work/bibliotecas` stay intact):

- **Names only discarded code cites.** `powf` (from `math.o`) calls `pow`, and neither is in
  the game. `mwldarm` requires every cited name to exist, even in code it will throw away
  (and linking the whole `.a` archives gives "Internal linker error"). Those names get an
  empty function in `work/build/mortos.c`; `-dead` discards it.
- **The exception table.** `dsd`'s `.lcf` keeps every `.exceptix` section (`KEEP_SECTION`),
  because the one from the assembly is a block nobody cites. But in the `.o` each function
  has its own, which points to it: kept, it would keep alive a function the game discarded.
  The `.exceptix` of those functions is left out of the link.
- **"Multidef" data and functions.** The typeinfo of `std::exception` goes into every `.o`
  that uses it, and the linker keeps one copy. The game kept that of a game file, which for
  us is in the assembly. `mwldarm` accepts several multidef copies, but not one multidef and
  one global; the `.o`'s copy gives way. The same goes for the `.o` files of our C++
  (`src/`), whose templates the game kept in other files; since the compiler puts each
  function in its own section, the section of the copy that gave way is left unused and
  `-dead` discards it.

And three things `ligar_bibliotecas.py` now gets right in the configuration:

- **Twin functions.** `strtod` and `strtold` have the same code (on the DS, `double` and
  `long double` are the same), and the game only kept one. The neighbor that calls it says
  which: the game's `atof` jumps to it, and the `.o`'s calls `strtod`. The RTTI destructors
  (`~__class_type_info`, `~__fundamental_type_info`) are equal even in their calls; there the
  vtable decides, looked up in the game's data and checked byte by byte up to the class name.
- **`__sinit__`**, the start of the `.ctor` table, is data. Recorded as a code label, the
  linker thought the table's pointers were ARM code calling Thumb and created a detour
  ("veneer") for each one. It is in `simbolos_linker.lcf`.
- **Each function's `.exception` starts at a multiple of 4**, even though the `.o` says
  alignment 1.

The one who writes those entries into `delinks.txt` is `decomp/tools/ligar_bibliotecas.py`:

1. **Which file each function is in.** The linker puts a file's functions together and in
   the `.o`'s order. The script walks the region and cuts where the source file changes.
   When two functions have the same code (the `NNS_G3dFree*` ones, which only call
   `NNS_FndFreeToAllocator`), the call targets and the order in the `.o` break the tie.
2. **Where each file's data is.** If function X of the `.o` reads `.bss+8` and in the game
   the pointer is `0x021b0d68`, that file's `.bss` starts at `0x021b0d60`. The bytes of
   `.data`/`.rodata` are checked, pointers included.
3. **The names the `.o` expects.** The `.o` calls `_s32_div_f`, `FS_ReadFile`... The address
   of each one comes from the game's instruction, and the name goes into `symbols.txt`.
   `static` functions stay `local`, and the data get the size they have in the `.o`.
4. **Who points into the middle of a file.** The game uses `NNS_G3dGlb + 0x80` (a field of
   the structure). The `.o` only has the `NNS_G3dGlb` symbol, so the relocation becomes
   `to:NNS_G3dGlb add:0x80`.

```
python3 decomp/tools/ligar_bibliotecas.py NitroSystem work/NitroSystem \
    work/bibliotecas/nitrosystem 0x020c8278 0x020d4394            # only shows
python3 decomp/tools/ligar_bibliotecas.py ... --aplicar           # writes
```

The versioned configuration comes from the three, in this order, starting from the one
before Phase 2.2 (commit `af53d17`):

```
python3 decomp/tools/ligar_bibliotecas.py NitroSystem work/NitroSystem work/bibliotecas/nitrosystem 0x020c8278 0x020d4394 --aplicar
python3 decomp/tools/ligar_bibliotecas.py NitroSDK work/NitroSDK work/bibliotecas/nitrosdk 0x020d4394 0x020e09d0 --aplicar
python3 decomp/tools/ligar_bibliotecas.py MSL - work/bibliotecas/msl 0x020e09d0 0x020eccb4 --aplicar
python3 decomp/tools/mapa_arquivos.py --aplicar                  # the game code (below)
```

## The game's file map

BioWare compiled the game file by file, and the linker placed each `.o`'s whole `.text`, one
after the other, in the same order in `.text` and in `.data`. The ROM doesn't say where a
file ends. `decomp/tools/mapa_arquivos.py` recovers part of that with a rule that can be
proven:

- **A string literal belongs to the file that uses it.** The compiler keeps a single copy of
  each string repeated within a file, but doesn't merge across files: `"BackButton"` appears
  9 times in `.data`. So two functions that read the same copy are in the same file, and so
  is everything between them in `.text`.
- **The order of the data is the order of the code.** If a piece comes earlier in `.text`
  but has strings after those of another, the two are the same file. It happens with inline
  functions (`GameModeInventory` has one at the end of `.text`, which reads a string from the
  start of the file's `.data`).
- **The check:** functions that read different copies of the same string must fall in
  different pieces. There are 337 pairs, and all of them do. If one failed, the script would
  stop.

The second rule (DIARIO 25) is that of the 12-byte variables the compiler creates for
`__register_global_object`: they are local to the file, even when they come from an inline
function (`Singleton<T>::Instance()`), so two functions that pass the same variable are in
the same file. The order of those variables in `.bss` also joins pieces, and two more checks
stop the script if they fail: no `2/π` marker in the middle of a piece's strings, and no
`.bss` range of a piece over that of an already linked file.

The result is **161 "cores"**, with 2,446 of the game's 4,869 functions and 84% of its code
bytes (with the strings alone there were 175 cores and 38%). Each one is in `delinks.txt` as
`jogo/<Class>_<address>.cpp` (the class most cited in the function names, when there is
one), with the `.text` and, when possible, the strings in `.data` (93) and the variables in
`.bss` (125). They are not `complete`: the build still uses the game's code, only cut into
one `.o` per file, and the ROM comes out identical.

A core is a piece **of a single file**, but it may be smaller than the real file: the
functions at the edges that use neither strings nor those variables stay out, in the
`_dsd_gap`s. Two details `dsd` requires: `.text` goes up to the multiple of 4 (the 2 padding
bytes after a Thumb function belong to its `.o`) and `.data` and `.bss` end at the start of
a symbol (`dsd` gives a piece of data without a size the bytes up to the next one).

## The progress metric

```
python3 decomp/tools/progresso.py
```

Measures how many **code** bytes the build takes from a `.o` compiled from source. Code is
the `kind:code` sections of `delinks.txt`: the `.text` of the ARM9 and the ITCM (functions
and the constants that come with them) and the ARM9's `.init` (the `__sinit_*` functions,
which the compiler produces in each `.cpp` with constructed global variables). "From source"
is the same criterion as `montar_rom.sh`: the files marked `complete`. The split between
libraries and game uses the regions proven in Phase 2 (the crt0 and the secure area,
NitroSystem, NitroSDK, MSL and the end of the ITCM); the rest is the game, where there may
still be some unidentified library.

```
                                       de fonte       total
     NitroSystem                         49.436      49.436  100,00%
     NitroSDK                            49.084      55.132   89,03%
     MSL                                 36.952      49.892   74,06%
   bibliotecas (213 arquivos)           135.472     154.460   87,71%
   jogo (3 arquivos)                      2.932     851.320    0,34%
   TOTAL                                138.404   1.005.780   13,76%
```
(*de fonte* = from source, *bibliotecas* = libraries, *jogo* = game, *arquivos* = files;
the output uses Portuguese number formatting. The numbers above are from an earlier stage.)

Until Phase 2.4, the count was done by hand and only with `.text` (13.58% at that time, the
earlier "13.6%"; before `CExoString` the total was 13.48%). `.init` came in because it is
also game code that will have to come from source: without it, it would be possible to reach
"100%" with 7,760 bytes still coming from the assembly.

After a `montar_rom.sh`, the script also checks the link: every symbol the linker map
(`work/build/arm9.o.xMAP`) shows inside a `complete` file must come from that file's `.o`,
and that `.o` must be the compiled one, not the one cut in `work/build/delinks/`. Without the
ROM (in CI), the count comes from the configuration alone. CI publishes the table in the job
summary.

**The objdiff report.** `python3 decomp/tools/progress_report.py` writes the same count to
`build/report.json` in [objdiff](https://github.com/encounter/objdiff)'s progress report
format (`report.proto`, version 2): code and data, per category (game, libraries, and each
library) and per file in `delinks.txt`, plus the function count from `symbols.txt`. CI
uploads it as the `YWSE_report` artifact, which is what progress trackers such as
[recomp.fyi](https://recomp.fyi) read from the default branch. Since only `complete` files
count, "matched" and "fuzzy" are the same number: there is no partial credit.

## Comparing with objdiff

`comparar.py` answers "does this function match?" from the command line.
[objdiff](https://github.com/encounter/objdiff) is the graphical tool most decompilations
use for the same question: it shows each function of the game next to what our C++ compiles
to, marks the instructions that differ, and recompiles the file every time it is saved.

```
decomp/tools/montar_rom.sh rom.nds            # extracts and delinks the game (work/build/)
python3 decomp/tools/objdiff_config.py        # writes objdiff.json
```

Then choose the repository folder as the project directory in objdiff.
`objdiff.json` is generated from `delinks.txt` and stays out of Git: run the script again
after changing `delinks.txt`.

What the script sets up, on top of `dsd objdiff`:

- Each file in `delinks.txt` is a unit. The game's side is the `.o` that `dsd delink` cut
  (`work/build/delinks/`); ours is compiled by `decomp/tools/objdiff_build.sh`, which calls
  `compilar.sh`, so objdiff uses the same compiler and flags as the real build.
- A game file not decompiled yet (`jogo/...`) has only the game's side: it is there to read,
  and to start a decomp.me scratch from.
- Scratches use decomp.me's `mwcc_30_136`, which is `mwccarm` 2.0 sp2 (its `mwccarm.exe`
  reports "FileVersion 3.0 build 136"), the flags `compilar.sh --flags` prints, and the
  preprocessed headers as context. The libraries get no scratch: they are already linked
  from source, with other flags.
- objdiff pairs functions by name. A file still in progress shows both sides when its entry
  in `delinks.txt` is named after its source (`src/Aurora/CRules.cpp` instead of
  `jogo/0201ef68.cpp`) and `symbols.txt` has the names the compiler gives its functions.
  Without `complete`, the build still links the game's code, so the ROM stays identical.

The check that it works: with objdiff-cli 3.4.0, `objdiff-cli report generate` on this
`objdiff.json` gives 100% for each of the 5 decompiled game files. A few library files
(MSL's floating-point routines, 4 others at 99.99%) come out below 100% in objdiff although
the ROM is identical; the SHA-1 is what decides.

## The tools and their versions

| Tool | Version | Where from |
|---|---|---|
| `dsd` | 0.12.1 (release binary) | [ds-decomp](https://github.com/AetiasHax/ds-decomp/releases/tag/v0.12.1) |
| `wibo` | 0.6.16 | [decompals/wibo](https://github.com/decompals/wibo): runs Metrowerks' 32-bit `.exe` files on Linux, much smaller than Wine |
| `mwccarm`, `mwldarm` | 2.0/sp2 (see [COMPILADOR.md](COMPILADOR.md)) | the `mwccarm.zip` package from [decomp.me](https://github.com/decompme/compilers) |

All are downloaded with their SHA-256 checked. Why the `dsd` release and not the code from
`main`: built from `main` (commit `c408063`), `dsd lcf` reports "duplicate file name" on
every `_dsd_gap` as soon as the ARM9 is split into more than one file. The 0.12.1 release
binary doesn't have the problem.

## What is left of Phase 0

**0.4, CI with the ROM.** The full build needs the ROM, and it can't go to GitHub (not even
as a secret: the limit of a secret is 48 KB). Run `montar_rom.sh` before every PR that
touches `src/` or `config/`.

Meanwhile, CI already checks the part that breaks most, **without the ROM**: the
`decomp-matching` job compiles each function in `decomp/compilador/casos.txt` and compares
the SHA-1 of its bytes (with the relocations zeroed) with `decomp/compilador/esperado.txt`.
Only the hash is in the repository. If a function's C++ stops matching, the PR turns red.
When you match a new function, add it to `casos.txt` and run
`python3 decomp/tools/conferir_sem_rom.py --gerar work/extract/arm9/arm9.bin`.
