# Journal of findings

The story of the project, in order, with the hypotheses that worked and the ones that didn't.
Each entry says **how** the thing was found and **how** it was confirmed.

(*Diário* is Portuguese for journal; the file keeps its original name because other
documents and tools point to it. Mistakes are numbered "Erro nº N", mistake no. N.)

---

## 1. Opening the ROM
- Game code `YWSE`, a 1.1 MB ARM9 **without compression and without overlays** (rare: it
  helps a lot), NitroSDK 4.2 (`0x04027531`, found in the module's parameter block, signature
  `0xDEC00621`).
- 303 files at the root of NitroFS. The `2DA`, `GFF`, `TLK` extensions gave away the engine:
  **BioWare's Aurora**, the same as NWN/KotOR/Dragon Age, adapted to the DS.
- xoreos (a free reimplementation of the BioWare engines) had already studied the game, but
  only as a viewer.

## 2. Mapping the code
- `dsd` (ds-decomp) found the functions by following the calls. It got stuck at
  `0x020ecc3c`: a function **written by hand in assembly** that saves every register
  (`setjmp` style), right after another that ends in an unusual `bx r2`. Solution: allow
  calls to unknown functions. Result: **6,820 functions**.
- An attempt that led nowhere: a table with **98 profiling tags** (`"Creat:UpdtGamepl"`,
  `"BG:SetFocusPoint"`...). I looked for the profiler's `Begin` function statistically and
  didn't find it: the profiler was **turned off** in the final version (the report function,
  `0x020660c4`, has no caller). Still, the names reveal the subsystems.

## 3. RTTI: the real class names
- Strings like `13CGameCreature` are "mangled" C++ names, kept for `dynamic_cast` and
  exceptions. Following `name → typeinfo → vtable → functions`: **290 classes with
  inheritance, 351 vtables**.
- **Erro nº 1:** I marked as a constructor every function that read a vtable's address. The
  assembly showed functions that only *registered a singleton* (a local `static` with the
  constructor inlined). Fix: a constructor is whoever **writes** the vtable at offset 0, and
  whoever does that inside another task becomes `constructs_<Class>`.
- **Erro nº 2:** I assumed the Itanium ABI layout (destructors in vtable slots 0 and 1).
  Ghidra's pseudo-C showed slots 0 and 1 were ordinary methods. In CodeWarrior, the
  destructor sits in the order it was declared. Fix: a destructor is **the virtual method
  that writes its own class's vtable into `this`**. 151 found.

## 4. One function from start to end: the name hash
The `test.herf` pack only keeps hashes of the names. I tested candidates (DJB2, CRC32, FNV)
and the DJB2 of the lowercase name matched. Then I found **the game's function** that
computes it, through the constant 5381 (`0x1505`):

```asm
ldr   r4, =0x1505          ; hash = 5381          <- DJB2's signature
bl    CExoString__CStr     ; s = name.CStr()
ldrsb r5, [r0, r1]         ; c = (signed char)*s   <- SIGNED!
ldrb  r5, [r0, r5]         ; if 0<=c<0x80: c = lowercase_table[c]
lsl   r7, r4, #5 ; add r4, r7 ; add r4, r5, r4      ; hash = hash*33 + c
```
Rewritten in C (`decomp/src/resource_hash.c`) and in Rust. A detail only the assembly
reveals: the character is read **signed**. The C test recomputes every name in the game.

**Erro nº 3:** the C test caught **4 false names** the dictionary attack had accepted: hash
collisions with "words" taken from random bytes.

## 5. The data formats
- **`.small`**: at first I only found LZ10 (byte `0x10`). 185 files starting with
  `00 28 02 00` were left. The header is `type | size << 8`, and **type 0 is uncompressed**.
- **GFF4**: the same as Dragon Age's, but the TLK uses **8-bit** text (on PC it is UTF-16),
  while dialogs use UTF-16. There are two field types unique to Sonic: 18 (DS 20.12 fixed
  point) and 20 (inline ASCII).
- **GDA**: column names are `CRC32(name.lower() in UTF-16)`. 534 of 845 are known (the same
  63% as xoreos).
- **Dialogs**: the "who speaks" field is always `PLAYER`. Who really speaks, and with what
  **emotion**, is in the portrait's name: `tailssca` = Tails scared.

## 6. The backgrounds (a format nobody had decoded)
1. `.pal` with 136,192 bytes = 1,064 × 128? First hypothesis: a 64-color palette per tile.
   The indices went up to 215, so it was wrong.
2. The tiles are 4,096-byte LZ10 (64×64). Drawn row by row, they came out striped; in **8×8
   blocks**, leaves and rocks appeared.
3. There was still noise: 136,192 = 266 × 512 = **266 palettes of 256 colors**, and 38 × 28
   tiles / 4 = 266. In other words, one palette per 2×2 block of tiles.
4. The tile positions were still wrong. Instead of guessing, I **measured** the color
   difference at the edges between neighbors for each ordering hypothesis. The best gave 35,
   still bad. The area's `.2da` was a **palette map** (palettes numbered column by column).
   With it: **4.8**. Green Hill appeared whole.
5. Depth (`.cdpth`): the same index, but 16-bit values in **linear** order (unlike the
   color!), with `0x7FFF` = empty. It shows the pillars and statues that stand in front of
   the characters.

## 7. The official dictionary
Inside the pack itself there was an `erf.dict` with **every resource name**. It took the
recovery to 100% and revealed **12 more false names** from the dictionary attack (e.g.
`1_c.emit`, which is really `BTN_PUZZ_ON.NCGR`). Lesson: a clever method that gets 91% right
still gets things wrong; the official source, when there is one, is worth more.

## 8. Sprite palettes
**Erro nº 4:** the rule "most common palette of the same prefix" painted **555 portraits with
Tails' palette**. Fix: the tables give each character's palette (`creatures.gda`:
`PRTL_TAILS` → `PRTL_Tal.nclr`), and the `.gui` screens give each element's. 1,747 images now
have their palette confirmed by the game.

## 9. Writing back: the road to modding
To modify, you need to **write** the formats. The criterion was to read and rewrite without
changes and get the original file byte for byte:
- GDA: 137 of 229 on the first try. The padding between texts is `0xFF`, not zero. After
  that, 229 of 229.
- HERF: the end of the file had to be aligned too. After that, 6 of 6 (including the 49 MB
  one).
- **TLK: it is a hash table.** Of the 18,845 entries, 4,711 were empty (id `0xFFFFFFFF`) and
  the ids had no order. Common hashes didn't explain the positions. I went to **the game's
  code**: function `0x020989fc` (the text lookup) showed the formula, a variant of Thomas
  Wang's integer hash, with linear probing. Checked: 14,134 of 14,134 positions right.
  Without it, a new text would land in a position where the game would never look for it.
- LZ10 compressor **without distance-1 references**: the BIOS routine that writes to VRAM
  writes 16 bits at a time and would break with them.

## 10. In the emulator
- DeSmuME runs without a window (`py-desmume` with `SDL_VIDEODRIVER=dummy`) at ~110 fps. A
  script of touches reaches the title screen and the opening quote.
- Test 1: change the quote's text (TLK). It worked.
- Test 2, through `sonic-mod`: edit the `Chapter0` table to point to a **new text** (id
  990001) that didn't exist. The screen showed the new text. That proves the table edit, the
  insertion into the TLK hash table and writing a larger file at the end of the cartridge,
  all at once.

## 11. Combat
The result is in [`COMBATE.md`](COMBATE.md). The road:

- **From the rules to the code.** `combatrules.gda` has 74 unnamed numbers. The function
  that loads it (0x0201f270) copies each row into a global variable; cross-referencing each
  global with the functions that read it, each rule got an "owner" (e.g. rules 7, 8 and 11
  only appear in `Defend`). That is how the formulas were found: through the numbers they
  use, not through names.
- **Small functions actually executed.** The damage uses software floating point
  (`0x020ea840`, `0x020ea8d0`...). Instead of guessing by name, each one was run in Unicorn
  with known inputs (`3 → 3.0`, `6.0 / 4.0 → 1.5`). The same for `RollDice` (0x02008550).
- **Erro nº 5:** Ghidra showed `func_020d5cfc(x)` with **one** argument and I treated it as a
  one-input function (square root? inverse?). In Unicorn it hung. The assembly showed it is
  the DS's **hardware divider** (registers 0x4000280) and that the second argument (`r1 =
  0x64000`, or 100.0 in fixed point) exists, Ghidra just lost it. Lesson: when the pseudo-C
  looks odd, the assembly decides.
- **Column names lie sometimes.** The `GUITypeAggressive` column of `combo.gda` (a name from
  xoreos' dictionary) holds 0.3/0.5/0.8: the status chance per level.
- **The game's own text as proof.** Special abilities only have a numeric code. The
  descriptions of the Chao and accessories ("Increases the team's chances of Ambushing...")
  gave each code's meaning, and the ambush code confirmed numbers 21 and 22. The game's rules
  journal (`journalrules.gda`) confirmed the link between Armor/Grit, Power/damage and the
  pairs of statuses and cures.
- **A finding that contradicts the text.** The game's journal says Clover Juice cures
  Cursed; its `.ITM` has no removal row. It was recorded as it is.
- The generator `analise/tools/combate_tabelas.py` turns the tables and the effect files into
  readable Markdown, so you can check all of this on your copy.

## 14. The rebuilt ROM (Phase 0)
The guide is in [`BUILD.md`](BUILD.md). The road:

- **Tools without Windows.** Metrowerks' compilers are 32-bit `.exe` files. `wibo`, a
  minimal loader made by the decompilation community, runs them on Linux without Wine. The
  compiler package is the same one decomp.me uses.
- **Taking it apart and putting it back together.** `dsd delink` cuts the ARM9 into `.o`
  files with relocations in place of the addresses; `mwldarm` (the original linker) joins
  everything. The ARM9, ITCM and DTCM came out identical on the first try: the 60 thousand
  relocations found by `dsd init` are right.
- **The 20 stubborn bytes.** The whole ROM still had 20 different bytes, none of them code.
  16 were the icon's: the palette has two equal colors and `dsd`'s intermediate PNG doesn't
  keep which index was which. The others were the secure area CRC, which depends on the ARM7
  BIOS key. Both were solved without the BIOS, and the SHA-1 matched: `f4ff8291...`.
- **Erro nº 7:** at first I read the icon differences as "index 8 became 6" (looking at
  `cmp`'s bytes in octal). Comparing nibble by nibble, they were all 4 → 2: the two entries
  with the same color. The real cause only showed up when I read the `ds-rom` code that
  converts the PNG.
- **A tool bug.** Built from the newest code, `dsd` refused any split of the ARM9 into files
  ("duplicate name" in the holes without source). The 0.12.1 release binary doesn't have the
  problem; we stuck with it.

## 15. The compiler (Phase 1)
The result is in [`COMPILADOR.md`](COMPILADOR.md): **mwccarm 2.0, `-O4,p`, Thumb**.

- **Comparing without the linker.** `decomp/tools/comparar.py` puts the game's function and
  the `.o`'s side by side, ignoring the bytes only the linker fills in (a `bl`'s target, the
  constant pool pointers).
- **CExoString::CStr** matched on the first try with every 2.0 version: it is too small to
  tell which. **HashResourceName** needed 3 rounds: the inline `tolower` with `||` explained
  the flag register, and a temporary variable got the order of the last addition's operands
  right.
- **Looking for a function that tells the versions apart.** I compiled ~30 C++ constructs
  with every version: only three families produce different code (1.2, 2.0 and DSi). One DSi
  difference is allocating `r4-r7` where 2.0 uses `r3-r6`; searching the game for functions
  with that pattern, the list item removal (`0x0202d428`) matched every 2.0 and no DSi.
- **Exceptions off.** With exceptions on, the hash got an `.exceptix` entry; the game's are
  all from the MSL library. The game was compiled without them.
- **Erro nº 8:** `comparar.py` measured relocations against the `.o`'s first `.text`
  section. With a template function (which the compiler puts in its own section), the wrong
  relocations were masked and an identical function showed up as different. The right
  section is the one the relocation section's `sh_info` field points to.
- **The service pack from the libraries.** The game's code didn't tell the 2.0 versions
  apart, but the MSL that comes prebuilt with each CodeWarrior did. Searching the game for the
  functions of the libraries of 7 versions, only 2.0 sp2 found all 283 (the others, from 274
  to 280). As a bonus, 171 MSL functions got their real names (`memcpy`, `fwrite`...).
- **From the test to the game.** CStr and the hash became `src/Aurora/*.cpp`, marked
  `complete` in `delinks.txt`. The build started using our `.o` in place of the original
  code, and the SHA-1 stayed the same. To prove the check works, I undid the hash's
  temporary variable: the build failed pointing at byte `0x02009ba6`, the addition with the
  operands swapped.

## 16. The NitroSDK from source (start of Phase 2)
- **The same version, decompiled.** The game's SDK is 4.2.30001, the same one the community
  decompiled for Pokémon Platinum. Compiling that source, each SDK function should appear in
  the game with the same bytes.
- **Erro nº 9:** I compiled as ARM, as Platinum does, and nothing matched. The assembly of
  `OS_GetOwnerInfo` in the game is Thumb: BioWare used the Thumb version of the libraries.
  Recompiled with `-thumb -DSDK_CODE_THUMB`, 562 functions got names.
- **How to know it is not a coincidence.** Two small functions can have the same bytes. So
  the names were checked through the calls: if `X` calls `Y` in the source, function `X`'s
  `bl` in the game must land on function `Y`. All 1,140 references matched. The only one that
  looked wrong was a pointer *into* the function itself (the return address of
  `OSi_DisplayExContext`), and the mistake was the checker's, which ignored the relocation's
  offset.
- **The SDK was not compiled with the game's compiler.** Nintendo's libraries match better
  with 2.0 sp1 (652 functions) than with the sp2 (649) that compiled the game: Nintendo ships
  the SDK already compiled.

## 17. NitroSystem
- **The same road as the SDK.** I compiled the decompiled NitroSystem (version 071126) as
  Thumb, already knowing about Erro nº 9, and 360 functions got names: the 3D engine
  (`NNS_G3d*`), the sound one (`NNS_Snd*`), the 2D and the memory and VRAM managers.
- **A whole region explained.** All 464 functions between `0x020c8278` and `0x020d4394`
  match NitroSystem functions. None is left: it is exactly the version the game uses. That
  also fixed the start of the SDK, which I had put at `0x020d4000` from a quick look.
- **Who compiled what:** NitroSystem matches better with 2.0 sp2 (519 functions) than with
  sp1p2 (517). The SDK is sp1, NitroSystem and the MSL are sp2, as in Pokémon Platinum.
- **The call checker was wrong again, and on the safe side.** It reported 3 errors in
  `AlarmCallback`. It wasn't the name: there are two `static` functions with that name
  (`stream.c` and `capture.c`), and the checker compared one's calls against the other's
  address. Now it only uses the `.o` function with the same size as the game's: 708 of 708.
- **Linked from source.** The next step was for the build to use the compiled `.o` files in
  place of the assembly. The first attempt linked, but changed 19 thousand bytes. The
  causes, one by one:
  - a file's `.rodata` ended at an odd address, and the linker aligns the next one to 4:
    everything after it moved 2 bytes. The padding belongs to the file;
  - the game points to fields in the middle of structures (`NNS_G3dGlb + 0x80`), and the
    `.o` only has the start; the relocations became "symbol + offset";
  - functions with identical code (the `NNS_G3dFree*` ones) had their names swapped. The
    bytes matched, but the linker places functions in the `.o`'s order, and the order came
    out wrong.
  With that, NitroSystem's 59 files go in compiled and the ROM comes out identical.
- **A "relocation" that wasn't.** `dsd init` marked the number `0x021b0fdc`, in a constant
  table, as a pointer to the sound's `static` variable `sDriverInfo`. A `static` variable
  can't be used from another file, so it was just a number that looked like an address. It
  left the relocation list.
- **The NitroSDK too.** 73 of the SDK's 87 files started coming from source, and the ROM
  stays identical. With NitroSystem, that is 90 KB of the ARM9 (9.3%) coming from C code. The
  last obstacle was the linker throwing away `OS_DisableProtectionUnit`, which nobody calls
  but the game has. The `-force_active` option worked with one name and failed with the whole
  list. Testing lists of different sizes, the limit appeared: about 256 characters. The way
  out was the same request inside the linker file (`FORCE_ACTIVE` in the `.lcf`).
- **Symbols no `.c` defines.** Five files (`os_arena.c`, `os_thread.c`...) use
  `SDK_SYS_STACKSIZE`, `SDK_MAIN_ARENA_LO`... which Nintendo's `.lcf` computed. The values
  were read from the constants the game has written (e.g. `SDK_SECTION_ARENA_EX_START` =
  `0x023e0000`) and went into `simbolos_linker.lcf`. The proof they are right is the ROM: a
  wrong value would change those bytes. Now there are 78 SDK files, 92 KB (9.7% of the ARM9)
  of C, and the ROM identical. The configuration generated from scratch comes out the same as
  the versioned one.
- **ITCM and DTCM.** Six SDK files have parts outside the main ARM9. First a hand test with
  `os_irqHandler.c` (`dsd` refused the same name in two modules; with the alias
  `os_irqHandler.itcm.c` it accepted, and the linker map showed `OS_IrqHandler` coming from
  the compiled `.o`). Then `ligar_bibliotecas.py` learned to split the ITCM and to find the
  DTCM data as it does with the rest. Two files (`os_cache.c`, `mi_dma_gxcommand.c`) didn't
  even need it: their ITCM functions are not in the game, the linker discarded them. Up to 84.
- **A counting mistake, and a badly broken tie.** `os_china.c` showed up as "doesn't link",
  but none of its functions is in the game: the check looked at every compiled `.o`. And
  `card_backup.c` "didn't match" by mistake: `PXI_Init` (8 bytes, a "jump to X") is identical
  to `CARD_WaitBackupAsync`, the tie went to the wrong file, and when `card_backup` showed up
  in its place it looked "repeated". The tie-break now looks at where the function jumps, and
  `pxi_init.c` showed up. The right total is 87 (without `os_china.c`, with `pxi_init.c`),
  not the earlier 87 by coincidence.
- **`.version`.** `card_backup.c`'s `[SDK+NINTENDO:BACKUP]` string is at `0x02000bd4`, right
  after the crt0, as Nintendo's `.lcf` says. It went in through the same alias as the ITCM
  (`card_backup.version.c`). 86 of 87.
- **What's missing: `gx_vramcnt.c`.** `GX_SetBankForSubBG` does the same as the source, but
  the `switch` came out with another comparison tree. Tested: the ten 2.0 compiler versions,
  the 24 orders of the `case`s, more or fewer `case`s, other optimizations. None matched; the
  file Nintendo compiled must be different from the public one.

## 18. The MSL linked
- **Where the `.o` files come from.** The MSL has no public source: it comes prebuilt in
  CodeWarrior's `.a` archives. `msl.sh` takes the 204 `.o` files out of them (C, C++, the
  Runtime and the floating-point math, which is assembly). 68 files are in the game and 62
  link.
- **The first link didn't even close.** `mwldarm` wants every cited name to exist, even in
  code it will discard: `powf` calls `pow`, and neither is in the game. Linking the whole
  `.a` archives gave "Internal linker error", and so did defining the names as 0 in the
  `.lcf`. The way out was a `.o` of empty functions for those names, generated in the build.
- **Then it closed, and the ROM came out different.** Nine causes, found by comparing the
  linker map with the game's addresses, from the first difference onwards:
  - an automatic label (`.L_020ead44`) the `.o` doesn't define: `mwldarm` resolved it as 0,
    **without an error**, and put an 8-byte detour in the middle of the code. Now the build
    stops if the game's code cites a name nobody defines;
  - twin functions with swapped names: `strtold` in place of `strtod`, the 8-byte
    `exp`/`pow` stubs with `math.o`'s names, the RTTI destructors. Each swap kept one more
    function alive;
  - the exception table kept every function's entry, and with it the function the game
    discarded;
  - each function's `.exception` starts at a multiple of 4 (Erro nº 10, below);
  - `__sinit__` recorded as a code label: the linker created a detour for each pointer of
    the `.ctor` table;
  - the typeinfo of `std::exception` is "multidef": the game kept the copy of one of its
    files, and the `.o`'s had to give way;
  - the `stdin`/`stdout` buffers and the vtable of `__si_class_type_info` only appear through
    pointers inside data, which the tool didn't follow: the linker left them out and put 0
    in the pointers;
  - a 2-byte `b .` at the end of another function looked like the whole `__rt_div0`.
  With that the ROM came out identical: 132 KB (13.6% of the ARM9 code) come from linked
  libraries. The configuration generated from scratch comes out the same as the versioned
  one, and the NitroSDK and NitroSystem come out the same as before (except for
  `.L_020ead44`).
- **Erro nº 10:** the tool didn't round a file's `.exception` end up to a multiple of 4,
  because the `.o` says alignment 1 and I believed it. The linker aligns to 4 anyway, and the
  next piece of the game started 3 bytes earlier than it should.

## 19. The game's files (Phase 2.4)
- **What the ROM doesn't say.** The linker puts each `.cpp`'s code together, but doesn't mark
  where one ends. There are no file names (not even in `assert` strings), and all 4,942 game
  functions start at a multiple of 4, so alignment doesn't help either.
- **What the compiler showed.** I compiled test files with 2.0 and looked at the `.o`: each
  string literal goes into its own `.data` section, right after the function that uses it
  first, and a string repeated in the same file becomes a single copy. In the game there are
  105 strings with several copies (`"BackButton"` has 9): one per file. Hence the rule:
  whoever reads the same copy is in the same file. And inline functions go to the end of the
  file's `.text`.
- **The linker's order is one and the same.** The 149 functions of the `.ctor` table (the
  constructors of global variables, one per file that has them) are in increasing address
  order, and so is the data each one writes: the order of the files is the same in the code
  and in the data.
- **The result:** 175 pieces certainly from a single file, 38% of the game's code bytes, in
  `delinks.txt` with the ROM identical. 338 pairs of copies of the same string check the
  rule, with no failure. `dsd` still caught a case I hadn't foreseen: two pieces of
  `GameModeInventory` with the code in one order and the strings in the other ("cycle in the
  link order"). It was an inline function at the end of the file, which reads a string from
  its start: the two pieces are the same file, and the tool now joins pieces like that.
- **Erro nº 11:** in my test, each file had a single block of variables, before the strings,
  and I was going to cut `.data` at every variable that came after a string. In the game,
  typeinfos and vtables are interleaved with the class names, and 18 functions read strings
  on both sides of one of those "cuts". I dropped that cut; the string-copy rule doesn't
  depend on it.
- **Where it stopped (paused on 2026-10-08).** The 175 pieces are in `delinks.txt` and the
  ROM comes out identical. The next step would be finding where each file really starts and
  ends, joining the functions without strings to the edges of each piece. The clues for that
  are the `.bss`/`.data` variables each function uses, who calls whom and the runs of methods
  of the same class.

## 20. The progress metric
- **A number someone can hold us to.** The "13.6%" was a count made by hand. Now
  `decomp/tools/progresso.py` reads `config/YWSE/` and says how many code bytes the build
  takes from a `.o` compiled from source: **13.48%** (135,556 of 1,005,780 bytes), 87.71% of
  the libraries and 0.01% of the game. The count is in
  [BUILD.md](BUILD.md#the-progress-metric).
- **Why the number fell 0.1 point.** Redoing the old count, only the ARM9 and ITCM `.text`
  gave the same 13.58%. The `.init` was left out: 7,760 bytes of `__sinit_*`, the functions
  the compiler produces in each `.cpp` that has global variables with constructors. They are
  game code and will have to come from source along with their file, so they count.
- **A check that can fail.** The script reads the linker map and checks that everything that
  fell inside a `complete` file came from its `.o`. The map only shows the `.o`'s name
  without the folder, and the piece cut from `jogo/X.cpp` is also called `X.o`; so the list
  of `.o` files the linker received goes into the count. Tested the other way around: a core
  marked `complete` without being compiled and a file with its end stretched over its
  neighbor were both refused.
- **ARM code in the middle of the game.** Looking for what separates library from game, I
  found two blocks of ARM functions (the game is Thumb) inside the game's region. They are
  not another library: they are the game's own methods, of `CMoviePlayer` (`0x0208ab44`, 24
  functions) and `Timer` (`0x020c70e4`, 46 functions). BioWare compiled some files as ARM,
  probably the ones that need speed. To decompile them, `compilar.sh` will need an ARM mode.
  The start of the ITCM (`0x01ff8000`-`0x01ffe89c`, ARM, unnamed) still has no owner and is
  counted as game.

## 21. Looking for the file boundaries, and the `-ipa file` option
The next step of Phase 2.4 was to extend the 175 cores to the functions at the edges, which
don't use strings. I tested each clue against what is already known (the cores) before using
it:

- **The padding between functions.** If the linker padded with another value between files,
  the boundary would show. No: the 2 bytes after a Thumb function are always `0000` (1,091 in
  the holes, 43 inside the cores, 5 at the end of cores).
- **Erro nº 12: the hidden variable of `__register_global_object`.** In a test `.o`, each
  global object with a destructor gets a 12-byte variable, local to the file, which goes as
  the 3rd argument to `__register_global_object`. I thought this would tie each function to
  its file's `.bss`. In the game there are 3,119 calls, 3,076 of them in ordinary functions
  and not in the `__sinit`s, and only 481 different variables: `main` uses the same one three
  times. They are `static`s inside inline functions (the `Singleton<T>` pattern), copied into
  every place that calls them, and therefore shared between files. It is no proof.
- **"One class, one file".** It fails for 9 classes: methods of `GuiTextInterface` and
  `GuiAnimatedImage` are inside the `GameModeCombat` core, for example (inline methods
  emitted in the file that uses them).
- **Who calls and who uses the data.** Checked on the 604 functions of the cores, where the
  file is known: "only called by functions of core X" is wrong 51 of 249 times, and "uses
  data only core X uses" is wrong 11 of 226. Clues, not rules.
- **`.rodata`.** In C++, a `const` constant outside a function is local to the file, and I
  thought `.rodata` would work like the strings. But its start is global constants (`extern
  const`): the word `0xffffffff` at `0x020ef844` is read by 150 functions across the whole
  game, and `0x020ef814` is the pointer to `CStr`'s empty string.
- **What worked: `-ipa file`.** To understand `.data`, I compiled test `.o` files and
  compared them with the game. In the game, a class's name (`"4Time"`) comes before its
  typeinfo and before the file's strings; with our flags, it went to the end. Testing
  options, only `-ipa file` reproduces the order. The three functions stay identical with
  it, and it went into `compilar.sh`. The check (`conferir_ipa.py`, in
  [COMPILADOR.md](COMPILADOR.md#-ipa-file-the-order-of-the-data)) measures two things the
  default option wouldn't produce: 51 names before the typeinfo (49 of derived classes, as the
  order predicts) and 266 of 270 typeinfos with no strings in between. The 4 exceptions are
  a string of an inline function, which with `-ipa file` goes into the variables block.
- **Erro nº 13:** in the first comparison I used `-ipa off` as if it were the default. It is
  not: it even reverses the order of the functions in the `.o`. The default behaves like
  `-ipa function`.
- **What `-ipa file` says about a file.** `.text` has the functions in source order and, at
  the end, the multidef ones (inline and template methods). `.data` has first the variables,
  vtables, typeinfos and names, and last the code's strings. So a transition from "multidef"
  to "ordinary" in `.text`, or from "string" to "variable" in `.data`, is the start of
  another file. The first only has 8 recognizable functions (those of `Singleton<T>` and
  `ResourcePool<T>`, by name). The second splits `.data`, but a variable can be read from any
  file, so it doesn't say where a function goes.
- **The conclusion.** The binary doesn't keep enough to prove which file each function
  without strings falls in. A file's boundary is proven when it is decompiled whole: the `.o`
  must reproduce together the `.text`, the variables, the strings, the `.bss` and the
  `__sinit`. One function more or less at the edge, if it uses strings or variables of its
  file, changes that order and the build reports it. If it only uses global data, the wrong
  boundary changes no byte: the mistake stays in the organization of the source only, not in
  the ROM.

## 22. The libraries' leftovers (attempt)
- **The two files with C++ templates.** `CPP_ios.c` and `CPP_msl_thread.c` were left out
  because they define template functions (`shared_ptr<char>`, `move_ptr`) that the game has
  elsewhere. They are "multidef" in the `.o`, and the idea was to let the `.o`'s copy give
  way to the game's, as is already done with data. With the safety check off, the
  configuration regenerated from scratch (from `af53d17`) came out the same as the versioned
  one plus the two files, but the build grew 160 bytes: `CPP_ios.o` brought its own copy of
  the functions, and the game has another one in the same place, which doesn't give way
  because in the assembly they have automatic names. Worse, the two copies are different: in
  the game, the sequence after `CPP_ios` has sizes 0x1a, 0x2, 0x4, 0x12..., and in the `.o`
  0x28, 0x14, 0xc, 0x2. The copies Nintendo's linker kept came from another compilation. The
  safety check was right; it came back.
- **The rest** stays as it was in
  [BUILD.md](BUILD.md#nintendos-libraries-linked-from-source): `CPP_locale` and `C_math_arm`
  (functions in another order), `CPP_iostream` and `C_ansi_fp` (a function in the middle
  doesn't match) and the SDK's `gx_vramcnt.c`. Together they are about 1.9% of the code; they
  are left for later, because the goal now is the game's code.

## 23. All of `CExoString` (Phase 3.1)
- **The class.** Twelve bytes: the vtable (only the destructor is virtual), `m_sString` and
  `m_nBufferLength`, with the names of Neverwinter Nights' `CExoString`, from the same
  engine. They are 33 consecutive functions, from `0x02004e00` to `0x020055b8`, and the
  game's order is the source's: the two memory helpers, the constructors, the destructor, the
  operators, and `AsINT`, `CStr`, `Find`, `Format`, `Left`, `SubString`, `UpperCase`,
  `CompareNoCase`... 28 matched on the first compilation. The other 5 taught the lessons in
  [COMPILADOR.md](COMPILADOR.md#what-we-learned-matching-functions): calling a function
  inside the arguments changes the order of the loads, and a value kept in a variable is read
  only once.
- **A library that was missing.** `LowerCase`, `UpperCase` and `CompareNoCase` call three
  unnamed functions in the middle of the MSL. They weren't in MSL C: they are in
  **MSL_Extras** (`extras_string.h`), which Phase 2.3 hadn't searched. They are `strlwr`,
  `strupr` and `stricmp`. The first two have the same code and only differ in the table they
  read (`__lower_map` or `__upper_mapC`), and that is how they got their names.
- **The data come from source too.** The virtual destructor is the class's first virtual
  function, so the `.o` brings the vtable, the typeinfo and the name `"10CExoString"`, plus
  the `"%i"` string of the `int` constructor. In the `.o`'s order they fall exactly on the
  game's addresses (`0x020f5510` to `0x020f553c`).
- **What is left for later.** The real file has more: the `""` that `g_emptyString` points to
  (`0x020f5508`), a variable at `0x020f550c` and its `__sinit` (`0x020edb7c`), which also
  constructs two C++ library objects at `0x021609e8`. As with `CStr` before, the proven part
  went in alone; the rest stays in assembly.
- **A marker in every file.** The variable at `0x020f550c` is `FX_Div(2.0, π)`, computed in
  the `__sinit`. And the game's 149 `__sinit`s do the same calculation, each for its own
  variable: it is a constant declared in a header that almost every file includes, and each
  file has its own copy, like the strings. That is 149 markers, one per file, in `.data` and
  in `.init`. They don't say where each function goes, but they tie each core to its
  `__sinit` and show when there is no file at all between two cores. It is Phase 2.4's next
  step.
- **A tool to put files into the build.** `nomes_do_fonte.py` fits the `.o` into the file's
  range, comparing the bytes, and replaces the names in `symbols.txt` with the compiler's
  (step by step in [BUILD.md](BUILD.md#a-decompiled-file-enters-the-build)).
- **Erro nº 14:** the tool turned every relocation into the middle of a symbol into "symbol +
  offset", including pointers to Thumb functions, which have bit 0 set (`0x02004f71` is the
  function at `0x02004f70`). The linker would add 1 and still set the bit: the pointer would
  move 1 byte. The build reported it; now those stay as they were.
- **Erro nº 15:** the default constructor exists twice in the `.o` (`C2` and `C1`, same
  bytes) and once in the game. The tool gave the address the name of the first one, `C2`, but
  the `.o` itself calls `C1` (`CExoString result;` in `operator+`). With both cited, the linker
  kept both, and everything after moved 16 bytes. Now it prefers the copy the `.o` uses, and
  then `C1`.
- **Result:** ROM identical, 35 functions checked in CI, and progress went from 13.48% to
  **13.67%** (the game's code, from 84 to 2,044 bytes).

## 24. The markers and the allocator (Phases 2.4 and 3.2)
- **The markers checked.** Each file's `2/π` constant (DIARIO 23) was found in the 149
  `__sinit`s, one of them in ARM. The 149 variables are different and sit, in `.data`, in the
  same order as the `__sinit`s in `.init`: the check could have failed and didn't. And none
  falls in the middle of a core's strings, as the `[variables][strings]` model requires. From
  that comes a new proof: **if there is a marker between the strings of two cores, they are
  different files**, because the marker is a variable and can't come after its own file's
  strings. Of the 170 cores with strings, 77 have markers between them and the previous one,
  and 34 have two or more, which means there is at least one whole file without strings in
  between.
- **One of those files is the allocator.** Between core `02002918` and the sound one there
  are two markers. The first, `0x020f5424`, belongs to `__sinit_020eda24`, which sets up 5
  small-block "pools" and registers each one's destructor. Its `.bss` closes exactly: 4 bytes
  of a flag, 5 × 12 of the `__register_global_object` variables and 5 × 0x18 of the pools,
  from `0x02110f34` to `0x02110fec`. The code is the game's `new`/`delete` (which takes a
  block from a pool or, failing that, from the NitroSDK heap) and a function the SDK's crt0
  calls before `main`: `NitroStartUp`, which sets up the heaps.
- **The first game file with everything.** `src/Aurora/Memory.cpp` brings the code (14
  functions), the `__sinit`, the `.ctor` entry, the marker in `.data` and the `.bss`, and the
  ROM comes out identical. 9 functions matched on the first try; the lessons from the others
  are in [COMPILADOR.md](COMPILADOR.md#what-we-learned-matching-functions). The marker's
  lesson: it only goes into `.data` when declared `const` (`const fx32 X = FX_Div(...)`),
  which confirms it is a header constant, local to each file.
- **A bug in the game.** Whoever returns a block to the pool writes its index into the list,
  but doesn't decrease the count of used blocks. The block is never reused, and when a pool
  fills up, the requests go to the heap. It stayed in the C++ as it is, with a comment. *(The
  sentence "the block is never reused" is wrong: Erro nº 18, section 27.)*
- **The edges stay open.** The two empty functions after `NitroStartUp` (`0x020034ec` and
  `0x020034f0`) and the ones before the destructor may belong to this file or to its
  neighbors; they stayed in assembly.
- **Result:** ROM identical, 50 functions checked in CI, and progress went from 13.67% to
  **13.76%**.

## 25. The hidden variables, again: 84% of the code split into files (Phase 2.4)
- **Erro nº 16: the conclusion of Erro nº 12 was wrong.** Decompiling the file system, I
  found `Singleton<CDSFileSystem>::Instance()` copied (inline) inside a function: the guard
  and the object are in `main.cpp`'s `.bss`, but the 12-byte variable passed to
  `__register_global_object` is in this file's `.bss`. In Erro nº 12 I had seen `main` use the
  same variable three times and concluded it was shared between files. Three uses in the same
  file say nothing about different files. A test `.o` settles it: in an inline function with
  a `static`, the object and the guard come out "multidef" (one copy per program), and the
  variable comes out **local**, one per file, the same for every function of the file.
- **The new rule.** Two functions that pass the same variable to `__register_global_object`
  are in the same file, like two functions that read the same copy of a string. Since
  `Instance()` is copied into more than 3 thousand places, it reaches almost all the code.
  `mapa_arquivos.py` now joins functions by both rules (990 functions pass 438 variables) and
  also by the order of `.bss`: if a piece above has variables after those of a piece below,
  they are the same file (inside a file the compiler orders those variables its own way, as
  in the allocator's pools).
- **The checks, all of which could have failed:** the 337 pairs of copies of the same string
  stay in different pieces; none of the 149 markers falls in the middle of a piece's strings;
  and, by `.bss` contiguity, 8 pieces contain variables of a `__sinit`, each of a single one,
  in `__sinit` order.
- **Files without code.** The first piece goes from `main` to `0x02002d0c`, and there are
  three markers before its strings. Two are files without any function in the game. It makes
  sense: the linker discards functions nobody calls, but a file's `__sinit` always stays (the
  `.ctor` table points to it), and with it the data it writes. And the order inside a file's
  variables block mixes variables and typeinfos, so the marker doesn't mark the start of the
  file, only that the file exists.
- **Result:** 161 pieces cover **84%** of the game's code bytes (before, 175 pieces and 38%),
  125 of them with the `.bss` range; the ROM stays identical. A piece that crosses an already
  decompiled file (`HashResourceName` in the middle of the file system) stays in two parts.
  What is left (16%) are the edges and the files that use neither strings nor `Singleton`.

## 26. The file system in C++ (Phase 3.3)
- **What the file does.** `src/Aurora/CDSFileSystem.cpp` brings together four classes:
  `CDSFileSystem` opens the cartridge files (NitroFS), counts how many times each was opened,
  reads at once or in the background (a request queue, with state and a 60-frame timeout)
  and decompresses LZ, RL and Huffman; `CErfTable` is a HERF pack (the 12-byte index per
  resource, found by binary search on the hash); `CErfMan` opens `test.herf` and the one for
  the console's language (French, Italian, German, Spanish; the rest use English); and
  `CErfResource` is a resource requested by name, which looks up `name.small` (compressed)
  when asked. They are 52 functions, 0x02008bb0-0x0200a308.
- **Bugs in the game, kept as they are:** `CErfTable`'s destructor only deletes the index
  when it is NULL (`==` instead of `!=`); `CErfTable::Open` doesn't delete the 8-byte header
  it read; the 12 paths `Init` stores are never used.
- **The templates.** The game has the copies of `CExoArrayList<T>` (the engine's list) and of
  the inline constructors spread at the end of several files: each `.o` produces its own
  "multidef" copy and the linker keeps the first one it sees. Of the 72 functions compared, 20
  are copies the game kept in `main.cpp` and in `CExoString.cpp`, and all of them match. The
  compiler puts **each function in its own section**, so our copy can give way to the game's
  without a byte left over: `preparar_link.py`, which already did this with the libraries,
  now does it with `src/` too, looking at the names of all the game's cut code (not only the
  `_dsd_gap`s: `CDSFileSystem`'s destructor is in a `jogo/` piece, and the linker reported
  "Multiply-defined" until that was fixed).
- **`Singleton<T>` has an empty virtual destructor.** Three independent things show it, and
  all three only match with it: (1) in the copied `Instance()`, only the first field's address
  comes out as a ready-made constant (without a destructor in the base, the first two come
  out); (2) in `CErfMan`'s constructor, the object's address is loaded before the vtable; (3)
  in `.data`, typeinfos and vtables come out sorted by size, and the ties
  (`CErfMan`/`CDSFileSystem`, `CDSFileSystem`'s name/`CErfTable`'s vtable) only fall in the
  game's order with it. The `Singleton<T>` vtable this creates is used by nobody, and the
  linker discards it, in the game and here.
- **The NitroSDK's `FSFile`.** The game copies an `FSFile` word by word for the first 0x20
  bytes and in two blocks after that (16 and 24 bytes): it is the SDK's layout, with `prop`
  and `arg` as `union`s. `include/nitro.h` now declares it that way.
- **The game's `memcpy` is the compiler's intrinsic.** With a small constant size it becomes
  in-place byte copies (the HERF index fields, the 3-byte `"./"`); otherwise, a call. The game
  has both forms, so `include/msl.h` sends `memcpy` to `__memcpy`. The 50 earlier functions
  stay the same.
- **`DelIndex` didn't match, and stayed in assembly.** `CExoArrayList<T>::DelIndex` (removes
  an element and pulls the others back) exists in the game for several `T`s. Our template
  reproduces byte for byte the copy for a 16-byte type (`0x0202d788`), but not those for the
  0x18, 0x1c and 0x60-byte types: in them the game computes the next element's address as
  "current offset + size" (strength reduction), and ours redoes the multiplication. With 16
  bytes it is a bit shift, and not even the game reduces it. I tested 960 equivalent forms of
  the loop (decrement, loop kind, increment, element expression), compiler versions 1.2, 2.0
  and DSi, optimization options and an inline helper function: none gave all four cases. This
  file's two copies (`0x0200a1b0` and `0x0200a258`) are at the end of `.text`, among five
  other template copies that match; the stretch `0x0200a1b0-0x0200a308` (7 functions) still
  comes from assembly, and our `.o` covers the rest. It waits until another clue shows up.
- **Erro nº 17: two "matches" that weren't.** Testing options, `-O4,s` and `-ipa program`
  gave "0 differences" in `DelIndex`. That was because, with them, the function didn't even
  come out in the `.o`, and my loop counted the lines marked `!!` in an error output. The test
  scripts now write `ERR` when the comparison doesn't run.
- **`.data` alignment.** Class names and strings come out of the `.o` with alignment 1, but
  in the game each one starts at a multiple of 4: the linker aligns them. `nomes_do_fonte.py`
  now counts that way, and skips the multidef copies that stayed in another file and the ones
  nobody uses.
- **Result:** ROM identical on the first link. The file comes from C++ with 45 functions, the
  `__sinit`, the `.ctor`, the `.data` (typeinfos, vtables and 21 strings) and the `.bss`;
  `HashResourceName`, which was alone, went into it. 119 functions checked in CI (it was 50)
  and progress went from 13.76% to **14.32%**.

## 27. The game's C++ running on the ROM's data (Phase 3)
- **The test.** Phase 3 asks, for each part, for a test that reads real data. The strongest
  is to run the very C++ that produces the game's bytes, without rewriting anything:
  `decomp/tests/test_arquivos.cpp` compiles `src/Aurora/Memory.cpp`, `CExoString.cpp` and
  `CDSFileSystem.cpp` with `g++`, together with `decomp/tests/nitro_pc.cpp`, which implements
  for the PC the NitroSDK and MSL functions they call (the cartridge becomes the
  `work/extract/files` folder, case-insensitive; the BIOS's three decompressions; the heap).
  `include/msl.h` got a PC branch, separated by `__MWERKS__` (which `mwccarm` defines); the DS
  bytes don't change (119 of 119 in `conferir_sem_rom.py`).
- **What it checks:** `erf.dict` comes out through the game's whole chain (`CErfResource` →
  `CErfMan` → `CErfTable` → `CDSFileSystem`); for each of the names, the hash checks, the
  resource is found and read, and the bytes match a separate reading of the index; `.small`
  files come out decompressed with the header's size; reading a piece, the background queue
  (`ReadFileAsync`, `Update` and the callback) and each console language's pack. All with the
  game's allocator serving every `new` and `delete`.
- **Result:** the **8,689 resources** of `test.herf` (5,111 LZ `.small`, 200 uncompressed)
  come out right; languages 0 to 6 open the right pack (English for Japanese, English and the
  rest). `erf.dict` claims 8,691 names: the last 2 are zeroed entries.
- **Tested the other way around:** three errors planted in the source (the position inside
  the index, the compression type test, the binary search) were all caught.
- **Erro nº 18: the allocator's comment was wrong.** I had written that a block returned to
  the pool "is never used again". Running the game's code: `Free` writes the block into the
  next slot of the free list, so it is the **next** to come out; what is lost is the block
  that occupied that slot, and the counter only goes up, so the pool serves at most
  `m_nCount` requests in the game's lifetime. The test now checks this (request, return and
  request again: the same block comes out; the neighbor above never comes out), and the
  comment was fixed.
- **A finding about the game.** With the pool exhausted, returning a block writes 2 bytes
  right after the end of the free list. On the PC that corrupted the heap at program exit. On
  the DS it is harmless by luck: the NitroSDK heap rounds each block up to 32 bytes, and the 5
  pools' lists have 8 to 24 bytes of slack. The PC `OS_AllocFromHeap` now rounds the same way.
- **How to run it:** `make -C decomp arquivos` after `montar_rom.sh`. CI compiles the test (so
  it catches the game's C++ no longer compiling outside the DS), but doesn't run it: it has no
  ROM.

## 28. The text table (`CTlkTable`, Phase 3.4): 12 of 13 functions
- **What the file does.** `CTlkTable` keeps the list of open `.tlk` files (`strings.tlk` and
  the language one) and, on each request, looks the id up in each, in the order they were
  added. Each `.tlk` is a 0x20-byte `CTlkFile`: it reads the header on the first lookup,
  computes the Wang hash of the id and reads the table from the cartridge 64 entries (0x200
  bytes) at a time, with linear probing. The game file runs from `0x02098930` to
  `0x02098f24`: constructor, two destructors, `Reset`, `GetString`, `AddFile`, the
  `CTlkFile` constructor, `Unknown2` and 5 copies of `CExoArrayList<T>`.
- **12 of the 13 functions match** (and the `__sinit`), in `src/Aurora/CTlkTable.cpp`, and
  go into `casos.txt`: CI now checks 132 functions (it was 119). Two findings:
  - **Template copies come out in the order the methods are declared in the class** (with a
    second pass for the ones only another template method asks for). Here `Allocate` comes
    before `Add`, so the declaration in `include/Aurora/CExoArrayList.h` moved. CDSFileSystem
    still matches and the ROM is still identical.
  - **The list element is a `struct` holding the pointer**, not the bare pointer. Of the 39
    copies of `Allocate` for 4-byte elements in the game, 38 copy pointers (the loop sets up
    the source before the destination) and only this one copies a struct (the other way
    round).
- **The lookup, checked against the real data.** In a separate script (Python, outside the
  repository), I did the lookup the way the game does: slot = `wang(id) % n`, linear probing
  from there, stop at id `0xFFFFFFFF`, text at `data + position`. In `strings.tlk` (data at
  `0x60`, a table of 18,845 slots), it finds all **14,134** ids, in up to 75 steps, and id
  `0x7FFFFFFF`, which doesn't exist, stops at the first step. In French, the same. One new
  detail, now in FORMATOS.md: **6,579 ids have text position `0xFFFFFFFF`** (no text in this
  language), and the game code doesn't handle that case (it would add the position to the
  data just the same).
- **`GetString` didn't match and stays in assembly.** It is 0x394 bytes, and the
  instructions are the same as the game's; what changes is the registers and the stack
  slots (148 different lines in the best version). In the game, `data` (the `m_dataOffset`
  read on every iteration) and `position` (the text position) go to the stack (`[sp+0x28]`
  and `[sp+0x2c]`), and the `CDSFileSystem` address stays fixed in `r7` during the loop. In
  ours, one of the two stays in `r7`, and the address is reloaded from the constant pool on
  every use. The best version of the loop (before it, the function reads the header if
  needed, computes the hash and reads the first chunk of the table):

  ```cpp
  do {
      u32 data = m_dataOffset;
      CTlkEntry entry = *(CTlkEntry*)(m_buffer + i * 8);   // struct { u32 id, position; }
      if (entry.id == id) {
          CDSFileSystem::Instance().ReadFileAt(&m_fileId, m_buffer, data + entry.position, m_bufferSize);
          u32 length = *(u32*)m_buffer;
          if (length * 2 < m_bufferSize - 4) {
              if (length >= outSize)
                  length = outSize - 1;
              memcpy(out, m_buffer + 4, length);
              out[length] = 0;
              return TRUE;
          }
          if (length >= outSize)
              length = outSize - 1;
          u8* text = new u8[(length + 1) * 2];
          CDSFileSystem::Instance().ReadFileAt(&m_fileId, text, data + entry.position, length * 2 + 4);
          memcpy(out, text + 4, length);
          out[length] = 0;
          delete[] text;
          return TRUE;
      }
      if (entry.id == 0xffffffff)
          return FALSE;
      slot = (slot + 1) % count;
      i++;
      if (slot == 0 || m_bufferSize == i * 8) {
          CDSFileSystem::Instance().ReadFileAt(&m_fileId, m_buffer, m_dataOffset + 8 + slot * 8, m_bufferSize);
          i = 0;
      }
  } while (slot != start);
  ```

  I tried about 300 versions, most by automated search: the entry reads in six forms
  (direct, copied `struct`, pointer, inline accessors...) and in every order; the place and
  type of the variables (`data` before, after, at the top; `i` and `slot` as `int` or
  `u32`); the hash in an inline function; `count` before or after the hash; the "found" path
  as another inline function; the lookup written straight into the `CTlkTable` loop; and
  `CTlkFile::GetString` without `inline`, inside the class or after its caller. None put
  `data` and `position` where the game puts them.
- **What I learned trying: how the compiler picks what goes to the stack.** In Thumb, only
  `r4`-`r7` survive a call. With small test functions (only calls and variables, nothing
  from the game): when registers run out, the values with the fewest uses go to the stack
  and, on a tie, the newest ones; and **stack slots follow an internal numbering, not the
  source order**. In an inlined function, that numbering puts the inline's variables first,
  then its parameters (last to first), then the outer function's variables (last declared
  to first). The game follows that pattern in `GetString` (`start`, the part of the hash
  hoisted out of the loop, `count` | `outSize`, `out` | the outer `i` | `data`,
  `position`). What I don't understand yet is where a variable defined inside the inline's
  loop goes: holding the result of a call, it goes to the end; holding a memory read, it
  goes to the start. In the game, `data` and `position` (memory reads) are at the end. The
  lessons are in COMPILADOR.md.
- **Why the file isn't linked.** `GetString` is in the middle of `.text`, and the build
  links a whole file (the `.o` covers the whole range). Splitting the source into two
  `.cpp` files, with `GetString` in assembly between them, doesn't work: each `.cpp` gets
  its own `__sinit` (the `Comum.h` constant, DIARIO 24), and the game file has only one. So
  the whole file stays in assembly, and the source is checked function by function by CI.
  When `GetString` matches, it goes between `Reset` and `AddFile`, and the file is linked
  (with the `.data`, `.bss`, `.init` and `.ctor`, which are left for that day). Progress
  doesn't change: **14.32%**.
- **Another plan item:** `FileReference`, which was missing in Phase 3.3, derives from
  `CGameBaseComponent` (the RTTI tree in `hierarquia_classes.md`): it is a game object
  component, and it moves to Phase 4.

## 29. GDA tables and GFF4 reading (Phase 3.4)
- **What the file has.** `src/Aurora/G2DA.cpp` (`0x0201ae10-0x0201b918`) is `G2DA`, the GDA
  table (the name comes from the RTTI, like the base's, `C2DASource`): it opens a `.gda` by
  name, checks the header and reads cells by position (row, column) or by key (the row id,
  which is the value of column 0, and the hash of the column name), as text, integer,
  float, boolean or fixed point. At the end of the file are the 13 functions that read GFF4
  (`include/Aurora/GFF4.h`, our names): the file, a struct's template (with the binary
  search for the field), the struct, the list. **The whole game uses these**: the field
  search has 50 callers, 41 outside `G2DA`. That is 52 functions, plus the `__sinit`, the
  `.ctor` and the `.data` (typeinfos, a 21-entry vtable and a `""`).
- **Identical ROM on the first link**, and progress went from 14.32% to **14.60%** (game
  code from 1.00% to 1.34%). CI checks 185 functions (it was 132), among them 4 copies the
  game keeps in other files: `IsLoaded` and `GetRowCount` (inline in the class,
  `0x0200f800` and `0x0200c458`), `Get<int>` and the integer read (`0x02019348`).
- **What made it match** (COMPILADOR.md has the lessons, which hold for the rest of the
  game):
  - every new struct or list gets the file **built again**, `GFF4File(data, size)`: the game
    reads the size before the data, because arguments are evaluated right to left; a struct
    copy reads in the other order;
  - the template index (16 bits of the field type) is **truncated inside the constructor**,
    when storing it; truncated in the argument, the truncation comes out earlier;
  - the value readers `Get<T>` are function templates, and the compiler places each
    instance **right after the first function that uses it**, not at the end of the file
    like a class template's methods;
  - the `""` returned when a text is missing lives in an **inline function**: that way the
    compiler gives it a name, it goes into the `.data` variables block (before the 2/π
    marker, as in the game) and the linker keeps one copy; the 16-bit one is from another
    file (`0x020f585c`) and is used by three;
  - the fixed-point default value is passed **by pointer**: the class has a copy
    constructor;
  - the label accessors (`cells.GetInt(10005 + column)`) are inline and change the order of
    the temporaries on the stack; with them, 51 of the 52 matched. `GetColumnType` only
    matched with the calls written out directly and with the `flag_found` pattern from the
    MSL's `parse_format` (a variable set to 1 before the `switch` and to 0 in `default`). I
    found the pattern by searching the game for the same instruction sequence: it only
    shows up in this function and in `parse_format`, whose source is known.
- **Erro nº 19: a label read wrong.** I wrote 11000 for the column type label; the game's
  constant is `0x2af7`, 10999. The comparison showed the difference in the constant pool
  (`0x2af8` versus `0x2af7`), and the number was fixed before the commit.
- **Test with real data** (part 5 of `test_arquivos.cpp`): each of the **229 `.gda`** files
  in `erf.dict` is opened by name with our C++, and the rows, columns, the hash and type of
  each column, each row's id and the value of each of the **122,741 cells** (9,560 rows)
  match an independent GFF4 read done in the test (which looks the fields up one by one,
  without the binary search). Four planted bugs were caught. The first, in a float read,
  wasn't: **no game table has a float column** (only integer, byte, fixed point and ASCII
  text by reference), and the test now checks that read on the 32-bit cells.
- **Two findings about the game.** `GetColumnType` returns FALSE for the 17,556 text
  cells: the `switch` compares the type including the flags (`0x20000014`, text by
  reference), and the cases are 14 and 20, without them. And the "V0.2" tables path never
  runs: it checks the version in the same place where `Load` requires "V4.0".

## 30. Text 2DA tables (`C2DA`, Phase 3.4)
- **What the file has.** `src/Aurora/C2DA.cpp` (`0x02003fec-0x02004c1c`) is `C2DA`, the
  text 2DA table, BioWare's classic format (the names `C2DA` and `I2DAProducer` come from
  the RTTI; the method names follow Neverwinter Nights' C2DA, from the same engine). `Load`
  reads the resource by name; `Parse` builds, in a temporary C2DA, one `CExoString` per
  cell; and `CopyFrom` moves everything to the "compact" form: a single block, with the
  name pointers, the position (u16) of each cell and the texts. That is 21 functions, plus
  the `__sinit`, the `.ctor` and the `.data` (typeinfos, a 9-entry vtable and the strings).
- **Identical ROM**, and progress went from 14.60% to **14.91%** (game code from 1.34% to
  1.71%). CI checks 207 functions (it was 185). With this, Phase 3.4 is complete, except
  `CTlkTable::GetString` (section 28).
- **What made it match** (COMPILADOR.md has the lessons):
  - the size of the `CExoString` block is `rows * columns + columns + rows` (cells, column
    names, row names); the compiler factors it into `(rows + 1) * columns + rows`, but
    writing the factored form gives a different operand order;
  - `CopyFrom` and `Parse` were written **in C style**: variables at the top of the function
    and the same counters (`i`, `j`, `column`) reused across loops. That changes the stack
    slot numbering and keeps alive a counter that is only incremented (the `column++` of the
    column-names loop). In `CopyFrom`, the expression `labelText + position` appears three
    times **without a variable**: the compiler keeps it as its own temporary, in another
    stack slot. With a `label` variable, the instructions were the same and only the stack
    changed;
  - in `Parse`, the variables whose address is passed (`&line`, `&lineLength`...) sit on the
    stack in reverse declaration order: the game's order tells what the source's was;
  - the inline virtuals `GetNumColumns` and `GetNumRows` come out at the end of the `.o` in
    **reverse** declaration order in the class. Since the vtable order comes from the base
    class (`I2DAProducer`), `C2DA` declares them reversed without changing the vtable.
- **Erro nº 20: the tool picked the wrong constructor, and G2DA already had the problem.**
  The `.o` has the constructor twice (`C2` and `C1`, same bytes), but between them the
  compiler put the base's inline destructor (`I2DAProducer::~I2DAProducer`).
  `nomes_do_fonte.py` only merged the copies when they were side by side: it saw `C2` alone
  and gave its name to `0x02003fec`. Our `LoadData` calls `C1`; with both referenced, the
  linker kept both, and the ARM9 grew by 32 bytes. The build caught it: the secure area
  (`0x02000000-0x02004000`) changed, because C2DA starts 20 bytes before its end. The linker
  map showed `C2DA::C2DA[base]` and `C2DA::C2DA` at `0x02003fec` and `0x02004018`. The first
  fix merged any pair of identical copies separated by multidefs, and G2DA stopped fitting:
  there are multidef templates with identical bytes there that the game keeps separate.
  Now only the C1/C2 (or D1/D2) pair of the same function skips the multidef copies in
  between. With that, the G2DA constructor (`0x0201ae10`) also went from `C2` to `C1`. The
  old name worked, because nobody in its `.o` calls `C1`, but it was wrong: whoever creates
  an object calls `C1`. The other four linked files were fitted again with the new tool.
  Only the local string numbers changed (`@671` became `@673`), because the `sscanf`
  declaration in `msl.h` moved the compiler's numbering.
- **Test with real data** (part 6 of `test_arquivos.cpp`): **370 2DA tables**, all "V2.0"
  (160 `.itm` and 116 `.spl`, plus 59 `.ITM` and 35 `.SPL`, in `test.herf`, uncompressed),
  are opened by name with our C++. The column and row names, and the text and integer of
  each of the **12,376 cells** (1,768 rows), match an independent read of the text done in
  the test. Four planted bugs were caught, each by the right check: without the `strlwr` of
  the row names, the cell position without the terminator, `"***"` instead of `"****"` and
  `%d` instead of `%i` (that one by the hexadecimal cells, `0x00000003`). The test runs on
  64 bits, where a pointer is 8 bytes; that's why the compact block is measured with
  `sizeof(char*)`, which on the DS gives the same code as 4.
- **Findings about the game.** The game only uses `C2DA` for items and spells. The file
  name is in column 5 of a GDA table (the function at `0x02019f34`, which builds the rules,
  ignores `****` and empty), and the values are read with `GetINTEntry`. The files mark an
  empty cell with `***` (three asterisks), but the reader only recognizes `****`: those
  cells keep the text `"***"` and give 0 as an integer (with TRUE). The binary path
  ("V2.b") and the `DEFAULT:` line exist in the code, but no file in the ROM uses them.
  `Parse` has a bug with `DEFAULT` without a colon: it subtracts the line length without
  advancing the text.

## 31. `ResourcePool<T>` belongs to the inventory (Phase 3.2)
- **What the game has.** The RTTI has a single instance, `ResourcePool<InventoryData>`. Its
  vtable (`0x020fabac`, 2 entries) points to the only two compiled functions of the
  template, the destructors: D1 (`0x02056a0c`) and D0 (`0x020594fc`). Each destroys two
  pointer lists (`CExoArrayList<T*>`, at `+0x10` and at `+4`, in that order), and D0 also
  calls `delete`. The two copies are in different files of the inventory mode, like the
  copies of the list destructor they call (`0x0201466c`, `0x0205957c`).
- **The rest is inline.** The function at `0x020565d8` (0x428 bytes) writes the pool's
  vtable and builds the lists; the same code creates the `InventoryData` objects (0x94
  bytes; per the RTTI, they derive from `ListData`, at `+0`, and from `Poolable`, at `+0xc`)
  and puts them in the lists.
- **Decision.** Like `FileReference` (section 28), `ResourcePool<T>` leaves Phase 3: without
  the inventory code there is no way to know each list's role or to check the template. It
  moves to Phase 5, together with the inventory mode. Phase 3.2 keeps the allocator,
  `Singleton<T>` and `CExoArrayList<T>`.

## What we don't know yet
`.vx` videos (Actimagine codec), the layout of the `.gui` screens, the Chao palettes, 311
GDA column names, whether a new item in a shop works, the limits the code imposes (number
of items, of characters), the exact compiler service pack (2.0 base or sp1+) and the
combat parts listed in [COMBATE.md](COMBATE.md#16-what-we-dont-know-yet). The next steps are
in the [plan](PLANO-DECOMPILACAO.md).
