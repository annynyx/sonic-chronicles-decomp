# The game's compiler

Result of **Phase 1** of the [plan](PLANO-DECOMPILACAO.md). Without the right compiler and
flags, no C++ comes out the same as the original; with them, a rewritten function can be
proven right byte for byte.

## Short answer

| | Value | How we know |
|---|---|---|
| Compiler | **Metrowerks CodeWarrior for DS, mwccarm 2.0** | 3 functions with a 100% match; 1.2 and "DSi" (4.0) get them wrong |
| Service pack | **2.0 sp2** (the libraries for sure; the compiler, almost) | the MSL and the Runtime linked into the game are sp2's: [below](#the-service-pack-from-the-libraries) |
| Optimization | `-O4,p` | `-O4,s` (size) gets 2 of the 3 wrong; `-O3,p` also gets them right |
| Mode | `-thumb -interworking` | the game's code is Thumb (and so are Nintendo's libraries, see below) |
| C++ | `-lang=c++ -Cpp_exceptions off -RTTI on` | below |
| IPA | `-ipa file` | the order of the typeinfos and strings in `.data`: [below](#-ipa-file-the-order-of-the-data) |
| Others | `-proc arm946e -enum int -char signed` | the usual for DS games; not yet tested one by one |

The flags live in one place: [`decomp/tools/compilar.sh`](../decomp/tools/compilar.sh).

## The three functions

| Function | Address | Size | File |
|---|---|---|---|
| `CExoString::CStr` | `0x020052f0` | 0x10 | [`src/Aurora/CExoString.cpp`](../src/Aurora/CExoString.cpp) |
| `HashResourceName` | `0x02009b78` | 0x44 | [`src/Aurora/CDSFileSystem.cpp`](../src/Aurora/CDSFileSystem.cpp) |
| list: remove item | `0x0202d428` | 0x2c | [`decomp/compilador/lista.cpp`](../decomp/compilador/lista.cpp) |

The first two are already real game files: the [Phase 0](BUILD.md) build uses their
compiled `.o` in place of the original code, and the ROM keeps the same SHA-1.

The matrix below is the output of `decomp/tools/testar_compilador.sh`: how many of the 3
functions come out identical for each version and optimization.

```
version    -O4,p   -O4,s   -O3,p   -O2
1.2/base    1/3     1/3     1/3     1/3      (1.2/sp2, sp2p3, sp3, sp4: same)
2.0/base    3/3     1/3     3/3     1/3      (2.0/sp1 ... sp2p4: same)
dsi/1.1     2/3     1/3     2/3     1/3      (all dsi/: same)
```

With all of `CExoString` decompiled (Phase 3.1), the same matrix has 35 functions and still
points to 2.0:

```
version    -O4,p   -O4,s   -O3,p   -O2
1.2/base    0/35    0/35    0/35    0/35     (1.2 doesn't know the -ipa option)
2.0/base    35/35   16/35   35/35   31/35    (2.0/sp1 ... sp2p4: same)
dsi/1.1     21/35   15/35   21/35   19/35    (dsi/1.6sp1 and sp2: 19 at -O4,s)
```

- **1.2** emits `mul` for `hash * 33` and saves registers with another convention
  (`push {r4, r5, lr}` + `sub sp, #4` instead of `push {r3, r4, r5, lr}`).
- **DSi (4.0)** gets CStr and the hash right, but in the item removal it allocates the
  registers differently (16 different instructions).
- **`-O4,s`** builds the hash loop differently; only the trivial function survives.

## Why exceptions off and RTTI on

- **RTTI on:** the game has typeinfo for 290 of its own classes (`13CGameCreature`...).
  That is how the class names were recovered.
- **Exceptions off:** with `-Cpp_exceptions on`, the compiler creates a 12-byte entry in the
  `.exceptix` section for every function that calls another (like the hash, which calls
  `CStr`). The 206 entries of the game's `.exceptix` are all for addresses above
  `0x020e09d1`, the C++ library's region (MSL). In other words: Metrowerks compiled the
  library with exceptions, BioWare compiled the game without.

## What we learned matching functions

Matching a function is adjusting the C++ until the compiler picks the same instructions.
Some lessons from these three, which should hold for the rest of the game:

- **`||` inside `?:` becomes a stored value.** The odd pattern
  `r7 = 1; if 0 <= c < 0x80 then r7 = 0; if r7 == 0 then c = table[c]` is an inline
  `tolower` written as `(c < 0 || c >= 0x80) ? c : table[c]`.
- **Temporary variables change the register order.** With
  `hash = (hash << 5) + hash + tolower(c)` the last addition comes out `add r4, r4, r5`;
  with the `tolower` in its own variable it comes out `add r4, r5, r4`, as in the game. The
  value is the same, the instruction is not.
- **The compiler rereads from memory what may have changed.** In the item removal, `data`
  and `count` are reread on every iteration: writing to `data[i]` could, as far as the
  compiler knows, change the object itself. Reproducing that requires reading the fields
  through the object, not copying them into local variables.
- **`ldrsb r5, [r3, r6]` with `r6 = 0`** is just a `*p`: in Thumb, `ldrsb` (signed byte)
  has no immediate-offset form, so the compiler uses a zeroed register.

More lessons, from `CExoString` (33 functions, [`src/Aurora/CExoString.cpp`](../src/Aurora/CExoString.cpp)):

- **A call inside the arguments changes the order of the loads.** In
  `stricmp(string.CStr(), m_sString)`, the compiler reads `m_sString` before calling `CStr`
  and keeps it in a register; the game reads it after. With the call on its own line
  (`const char* other = string.CStr();`), the order is the game's. The same in `Find`.
- **A value kept in a variable is read only once.** The game's `operator[]` reads
  `m_sString`, calls `strlen` and uses the same register afterwards; written with
  `m_sString` both times, the compiler rereads the field after the call.
- **A `BOOL` computed outside the loop** gives the pattern "r2 = 1 if the pointer is NULL",
  tested on every iteration (the 16-bit text constructor).
- **`ldr r0, =0` instead of `movs r0, #0`:** `movs` touches the flags, and the compiler still
  needs them for the following `beq`. Nothing needs to be done in the source.
- **The size of a local array** only shows up rounded on the stack (`sub sp, #0x24`): from
  33 to 36 bytes, the code is the same.
- **Constructors twice.** For each constructor, the compiler emits `C1` (complete object)
  and `C2` (the "base" part of a derived class), with the same bytes; the linker discards the
  one nobody calls. With `-ipa file`, `C2` comes before `C1`, and so it is in the game
  (`0x02004e2c`, called once, and `0x02004e68`, called 293 times): one more confirmation of
  the option. Destructors come out `D1`, `D0` (the one that also does `delete this`) and
  `D2`.

And from the allocator ([`src/Aurora/Memory.cpp`](../src/Aurora/Memory.cpp), 15 functions,
with `.bss`, `.data` and `__sinit`):

- **A `const` constant with a computed value goes into `.data`.** `const fx32 X =
  FX_Div(...)` stays in `.data` and is computed in `__sinit`; without the `const`, the
  variable goes into `.bss`. In the game it is in `.data`.
- **The order of the assignments in the constructor shows in `__sinit`.** The pools'
  inline constructor is copied into `__sinit`, and the game writes the count before the
  minimum and the size: the constructor assigns in that order.
- **Where the `return NULL` goes.** With `if (size == 0) return NULL;` at the start, the
  compiler puts that return right after the test; in the game it is at the end, which
  corresponds to `if (size != 0) { ... } return NULL;`.
- **The compiler folds constants, but not through a pointer.** `(u32)top - 0xa000 + 31`
  becomes `top - 0x9fe1`; `(u32)((char*)top - 0xa000) + 31` keeps both operations, as in the
  game.
- **An array of global objects with a constructor** (`static MemoryPool s_pools[5] = {
  MemoryPool(...), ... }`) produces, in `__sinit`, the construction and registration of each
  element, each with its own hidden variable; the variables come out in `.bss` in an order
  of the compiler's own, and it is the same as the game's.

And from the file system ([`src/Aurora/CDSFileSystem.cpp`](../src/Aurora/CDSFileSystem.cpp),
45 functions in the file and 24 template copies the game kept elsewhere):

- **On a bit field, `x.field--` repeats the expression.** `m_requests[i].timeout--` calls
  `operator[]` twice (once to read, once to write), and the arithmetic comes out as
  "+ 0xffff" in 16 bits. Written `x = x - 1` with two calls, it comes out `subs #1`. The same
  goes for `count++`.
- **An `(int)` changes the width of the arithmetic.** `state == 1 || state == 2`, on a
  16-bit bit field, becomes "`state - 1 <= 1`" done in 16 bits; the game does it in 32, which
  comes out with `(int)state == 1 || (int)state == 2`.
- **`if (type == 0) ... else switch`.** A 4-case `switch` becomes a tree of comparisons; the
  game tests 0 first (with the flags of the `ands`) and the other three in a row, which is
  the `if` followed by a 3-case `switch`. The bodies come out in source order (LZ, RL,
  Huffman), the comparisons in value order.
- **Reading a field only once before a call** (`int cache = m_files[i].cache;`) makes the
  compiler reuse the value after the call; read twice, `operator[]` is called again.
- **`b ? 1 : 0` into a 1-bit field** comes out with an extra cut to 16 bits; assigning the
  comparison directly (`field = x != 0`) doesn't.
- **A `struct` inside another is copied as a block.** The game copies the `FSFile` word by
  word for the first 0x20 bytes and with `ldm`/`stm` in two blocks after that: `prop` and
  `arg` are `union`s, as in the NitroSDK. Loose fields, or a 2-word array, come out word by
  word.
- **`memcpy` is the `__memcpy` intrinsic.** With a small constant size it becomes byte
  copies; with a variable size, `bl memcpy`. `include/msl.h` does the swap.
- **`(high + low) / 2`, not `(low + high) / 2`:** the order of the addition shows in the
  instruction.
- **The `Singleton<T>` base has an empty virtual destructor** (DIARIO 26). It produces no
  code that stays in the game, but changes three things that show: which field addresses
  come out as a ready-made constant in the constructor copied into `Instance()`, the order
  of the loads in the `CErfMan` constructor, and the order of the typeinfos and vtables in
  `.data`.
- **Each function in its own section.** mwccarm's `.o` has one `.text` section per function;
  small data (typeinfo, strings) too. An alignment-1 data section lands on a multiple of 4
  in the game: the linker aligns it.

And from the text table ([`src/Aurora/CTlkTable.cpp`](../src/Aurora/CTlkTable.cpp), 12 of 13
functions, DIARIO 28):

- **Template copies come out in the order the methods are declared in the class**, not in
  the order the file uses them. Those only another template method asks for (`Allocate`,
  which `Add` calls) come in a second pass, unless the file also asks for them. In the
  `CTlkTable` file, `Allocate` comes before `Add`, and that is what fixed the order in
  `include/Aurora/CExoArrayList.h`.
- **The element type shows in the copy.** For 4-byte elements, the `Allocate` loop that
  copies pointers prepares the source (`r3`) first and then the destination (`r5`); with a
  one-field `struct`, the order and the registers swap. The game has both forms, and that
  tells the type.

And from the GDA tables and the GFF4 reading ([`src/Aurora/G2DA.cpp`](../src/Aurora/G2DA.cpp),
52 functions, DIARIO 29):

- **Arguments are evaluated right to left.** An object built with
  `GFF4File(m_data, m_size)` reads the size before the data; the copy of the same object
  (`m_file`) reads the data first. When the game reads fields "back to front", it is a
  constructor.
- **Where a type cut goes.** `(u16)x` passed to a `u16` parameter cuts at the call; a `u32`
  parameter stored with `m_field((u16)x)` cuts when writing, after the other loads. The game
  has both, and the position of the `lsls`/`lsrs` says which.
- **Function templates come out right after whoever uses them.** The methods of a template
  class come out at the end of the file, in declaration order; a function template (or a
  template method, like `Get<T>`) comes out right after the first function of the file that
  instantiates it, followed by the ones it instantiates itself. That is the "function,
  helper, helper" pattern in the middle of a file.
- **A literal inside an inline function gets a name.** The `""` of an inline function becomes
  a named piece of data (`_ZZ...Es`), multidef, which goes into the `.data` variables block,
  sorted by size; in an ordinary function, it is local and goes to the end, with the strings.
  A literal shared between game files is of this kind.
- **A class with a copy constructor goes by pointer.** Passed by value, a 4-byte struct goes
  in a register; with a hand-written copy constructor, the compiler passes the address of a
  copy.
- **Inline functions change the stack and the registers.** The value an inline function
  returns becomes a temporary, and its temporaries go on the stack after those of the outer
  function. In the same function, writing `s.GetTemplate().FindField(x)->type` or calling an
  inline that does it gives different allocations; both appear in the game.
- **The MSL's `flag_found` pattern.** A variable that is 1 before a `switch` and 0 in the
  `default` is set up in `r0` before the comparison tree, and each case gets its own
  function exit. With `return TRUE` in each case, the 1 is repeated in each one.
- **Search for the sequence in the game.** When a form doesn't show up, it is worth searching
  the ARM9 for the same instruction sequence: if it is in a library function (MSL, NitroSDK),
  its source shows the construct.
- **The compiler factors arithmetic, and the factored form is not the source's.** `C2DA`
  allocates `rows * columns + columns + rows` cells; the game's code computes
  `(rows + 1) * columns + rows`, but writing it like that swaps the operands of the `muls`.
  Try the "naive" form when the factored one doesn't match.
- **Inline virtuals come out at the end of the `.o`, in reverse declaration order.** With
  `GetNumColumns` declared before `GetNumRows`, the `.o` has `GetNumRows` first; the game has
  the opposite. The vtable order comes from the base class that declares the virtuals, so the
  derived class can declare them in any order without changing the vtable.
- **`sizeof` instead of the number.** `rows * sizeof(char*)` gives the same code as
  `rows * 4` on the DS and stays right in the 64-bit test on the PC, where a pointer is 8
  bytes.

## Registers and stack: who goes where

When a function has more live values than registers, the compiler chooses which go to the
stack, and at what position. That explains most of the differences left once the
instructions are already the same. In Thumb, only `r4`-`r7` survive a call. What small test
functions showed (only calls and variables, nothing from the game; DIARIO 28):

- **The values with the fewest uses go to the stack; on a tie, the newest.** With seven
  values live across a call, the first four stay in registers and the last three go to
  `[sp]`, `[sp+4]`, `[sp+8]`. A value with more uses stays in its register and pushes another
  one to the stack.
- **Stack positions follow an internal numbering, not source order.** In the small tests,
  reversing the order of the declarations changed nothing. In `C2DA` (DIARIO 30) there were
  two cases where it did: variables whose address is passed (`&line`) sit on the stack in
  reverse declaration order, and in `SkipNewLines`, of the 24 possible orders of the four
  variables, only one gave the game's registers.
- **What enters the numbering changes everything.** In `C2DA`'s `CopyFrom`, the instructions
  were already the game's and only the stack differed. It matched when the loop counters
  became the same in every loop (`int i, j;` at the top, C style) and when the expression
  `labelText + position`, used three times, was left **without a variable**: the compiler
  keeps it as its own temporary, numbered after the variables. A counter declared at the top
  and only incremented in the loop (`column++`) stays in the code; declared in the loop, the
  compiler deletes it.
- **In a copied inline function**, the numbering puts first the inline's variables, then its
  parameters (last to first), then the outer function's variables (last declared to first).
  The game's `CTlkTable::GetString` follows this.
- **No rule yet:** a variable defined inside a loop of the inline goes to the end of the
  numbering if it receives the result of a call, and to the start if it receives a memory
  load. That is what `GetString` still lacks to match (in the game, the loop's two loads are
  at the end).

## `-ipa file`: the order of the data

The three functions match with or without `-ipa file` ("interprocedural analysis per file":
the compiler reads the whole file before generating code). What revealed the option was the
**order of the data** in `.data`, which matters as soon as a file with variables and classes
is decompiled whole.

Each class with RTTI has a typeinfo (`_ZTI`) and its name (`_ZTS`, the string `"4Time"`).
With the compiler's default (`-ipa function`), the names go to the end of the file, after the
code's strings. With `-ipa file`, they sit with the variables and the typeinfos, and the
code's strings come last. `decomp/tools/conferir_ipa.py` compiles an example with both
options and checks the game:

```
1. the prediction (the compiled example):
   default (-ipa function): string typeinfo name
   with -ipa file:          name typeinfo name string
2a. in the game: 51 of 270 names come before the typeinfo (with the default it would be 0)
2b. 266 of 270 typeinfos with no string in between (what -ipa file predicts)
```

The 4 exceptions in 2b are the same string, `"ScrollBG01.tga"`, read by a
`GameModeInventory` function. It is the string of an inline function: the compiler gives it
a name (`_ZZ...Es`) and, with `-ipa file`, it goes into the variables block. Of the 51 names
that come before the typeinfo, 49 belong to derived classes, as the `-ipa file` order
predicts (derived name, base typeinfo, derived typeinfo, base name).

Careful: `-ipa off` is not the default. It even reverses the order of the functions in the
`.o`.

What the option says about the layout of each file (and therefore about the Phase 2.4 file
map), checked on test `.o` files:

- **`.text`:** functions in source order, then the "multidef" ones (inline and template
  methods, of which the linker keeps a single copy).
- **`.data`:** first the variables, the vtables, the typeinfos, their names and the strings
  cited only by data (`static const char* names[] = {"a", ...}`); last, the strings cited by
  the code, each one before the first function that uses it.
- **`.rodata`:** the `const` tables. `float`s and `double`s stay in `.text`, next to the
  function (the constant "pool").

## The service pack, from the libraries

The game's own code doesn't tell the 2.0 versions apart. But the game also carries, at the
end of the ARM9 (from `0x020e09d0`), Metrowerks' C/C++ library (MSL) and the Runtime
(division, floating point, `new`/`delete`...). Those libraries come **prebuilt** with each
CodeWarrior version, and change a little from one version to another.

`decomp/tools/achar_funcoes.py` looks for each function of a library in the game (same size,
same bytes outside the relocations). `decomp/tools/versao_msl.sh` does it with the libraries
of each version (package from [metroskrew](https://github.com/mid-kid/metroskrew)):

```
version addresses found (of 283 that some version finds)
base     274
p2       274
p4       276
sp1      276
sp1p2    280
sp2      283      <- the only one that finds all of them
sp2p3    275
```

Of the 15 functions that change between versions, sp2 gets all 15 right; every other version
gets at least 3 wrong. So the game was linked with CodeWarrior 2.0 **sp2**. The compiler is
almost certainly from the same installation: `2.0/sp2`, or a patch of it that brought no new
libraries (the package has no libraries of `sp2p2`'s own; this is a deduction). It is what
`compilar.sh` uses.

For comparison: in the Pokémon Platinum decomp, NitroSDK 4.2.30001 (the same version as this
game) was compiled by Nintendo with `2.0/sp1p2`, and NitroSystem with `2.0/sp2`.

**Bonus:** the functions found without ambiguity got their real names in `symbols.txt` (171
names: `memcpy`, `fwrite`, `__flush_buffer`, `abort`...). Functions identical to each other
(`abs` and `labs`, the empty destructors) kept their old names, because there is no way to
tell which is which. The command:

```
L=work/ferramentas/metroskrew/lib/metroskrew/sdk/ds/2.0/sp2
python3 decomp/tools/achar_funcoes.py --de 0x020e09d0 --aplicar \
    $L/msl/MSL_C/MSL_ARM/Lib/MSL_C_NITRO_{T,Ai}_LE.a \
    $L/msl/MSL_C++/MSL_ARM/Lib/MSL_CPP_NITRO_{T,Ai}_LE.a \
    $L/Runtime/Runtime_ARM/Runtime_NITRO/Lib/NITRO_Runtime_{T,Ai}_LE.a
```

## The NitroSDK: compiled by Nintendo, as Thumb

The game uses NitroSDK **4.2.30001** (the number `0x04027531` in the ARM9). That exact
version was decompiled by the community ([ntrtwl/NitroSDK](https://github.com/ntrtwl/NitroSDK),
the same base as the Pokémon Platinum decomp). `decomp/tools/nitrosdk.sh` downloads that
source, compiles the 169 ARM9 files and looks for each function in the game.

- **Thumb, not ARM.** The first attempt, as ARM (like Platinum), found 0 of 4 functions of
  `os_ownerInfo.c`. The game's `OS_GetOwnerInfo` starts with `push {r4, lr}` in Thumb:
  BioWare linked the Thumb version of the SDK libraries (`-thumb -DSDK_CODE_THUMB`). With
  that, 3 of 4 matched right away.
- **The SDK's compiler version.** Functions found in the SDK region
  (`0x020d4000`-`0x020e09d0`) with each version: `2.0/base` 627, `sp1`/`sp1p2`/`sp1p5` 652,
  `sp2` 649. The SDK was compiled by Nintendo with a 2.0 sp1 (Platinum uses `sp1p2`, the one
  we adopted here), not with the game's sp2. It makes sense: Nintendo ships the SDK already
  compiled.
- **562 functions got their real names** (`OS_IrqHandler`, `MTX_Concat43`,
  `FS_OpenFileFast`...), counting ITCM and the `crt0` (`_start`, `do_autoload`). Only the
  unambiguous ones go in.
- **Independent proof:** `decomp/tools/conferir_chamadas.py` follows every call and pointer of
  the named functions: of the 1,140 references, all 1,140 point to the function with the name
  the source says. A name wrong by a coincidence of bytes would break that count.

With the MSL (171) and the SDK (562), the functions with automatic names (`func_...`) fell
from 5,685 to 4,973 in the ARM9 (and to 4,611 with NitroSystem, below).

## NitroSystem: 100% of the region recognized

NitroSystem is Nintendo's other library: 3D (`NNS_G3d*`, which draws the models), 2D
(`NNS_G2d*`), sound (`NNS_Snd*`), memory (`NNS_Fnd*`) and VRAM (`NNS_Gfd*`). It also has a
public decompilation ([ntrtwl/NitroSystem](https://github.com/ntrtwl/NitroSystem), version
071126). `decomp/tools/nitrosystem.sh` compiles its 96 files and looks for each function in
the game, as `nitrosdk.sh` does with the SDK.

- **Where it is:** from `0x020c8278` to `0x020d4394`, right before the NitroSDK (the SDK
  starts at `MTX_Identity22_`, not at `0x020d4000` as I thought).
- **Thumb again:** as Thumb, 519 library functions match. As ARM, only those of the files the
  source itself forces to ARM (`#include <nitro/code32.h>`, like the 3D animation and texture
  calculations) match, since they come out the same in both modes.
- **Compiler:** `2.0/sp2` and `sp2p2` find 519, `sp1p2` 517, `sp2p3` 513, `2.0/base` 494. The
  two functions only sp2 gets right are `NNSi_G3dFuncSbc_MAT_InternalDefault` and
  `NNSi_G3dFuncSbc_NODEDESC`. Same as Platinum: NitroSystem was compiled with sp2, and the
  SDK with sp1.
- **The version is the same:** of the region's 464 functions, **all** match some function of
  NitroSystem 071126. None is left over: the game uses exactly this version.
- **360 new names** (`NNS_G3dDraw`, `NNS_SndArcInit`, `NNS_FndAllocFromExpHeapEx`...). The
  other ~100 functions are ambiguous: `static` functions with the same name in several files
  (`texmtxCalc_flagTRS_` exists in the Maya, 3ds Max and XSI files) or functions identical to
  each other.
- **Checked through calls:** 708 of 708 references right (1,848 of 1,848 with the SDK).

With the MSL, the SDK and NitroSystem, the functions with automatic names in the ARM9 fell
from 5,685 to 4,611. In the SDK region, 628 of the 631 functions match the source; the 3 left
(`0x020d6930`, `0x020e067c`, `0x020e096c`) we don't know where they come from yet.

## What we don't know (yet)

- **The compiler's service pack with direct proof.** The libraries are sp2's, but in about 30
  C++ constructs versions `2.0/sp1` to `2.0/sp2p4` produced identical code; the only
  difference found was `while (n--)` between `2.0/base` and the others. If a future function
  only matches with another version, it is changed in `compilar.sh`.
- **The other libraries** (Actimagine's VX video, the backup one): not searched for yet, and
  with no known public decompilation.
- `-enum int`, `-char signed`, `-inline`, `-str`: no function that depends on them yet.
  (`-ipa file` was found through the data, above.)
- **`DelIndex` of lists whose element size is not a power of 2.** The game computes the
  address of the next element by adding the size to the current offset (strength reduction);
  the same template, here, redoes the multiplication. With 4- or 16-byte elements (a bit
  shift) both match. None of the 960 loop forms tested, nor another compiler version,
  reproduces both cases (DIARIO 26).
- **`CTlkTable::GetString` (`0x020989fc`).** The instructions are the game's, but two loop
  variables (`data` and `position`) go to different stack positions, and with them the loop's
  registers. About 300 forms tested (DIARIO 28); the numbering above is what is left to
  understand.

## How to reproduce

```
decomp/tools/ferramentas.sh
decomp/tools/montar_rom.sh your_copy.nds      # extracts the ARM9 into work/extract
decomp/tools/testar_compilador.sh             # the matrix above (~50 s)
```

To try a new function: write the C++, compile it with `compilar.sh` and compare with
`comparar.py` (examples in [BUILD.md](BUILD.md#when-the-build-fails)). When it matches, add
the line to `decomp/compilador/casos.txt` and rewrite `esperado.txt` with
`conferir_sem_rom.py --gerar` (see [BUILD.md](BUILD.md#what-is-left-of-phase-0)): it becomes
part of the test and of CI.
