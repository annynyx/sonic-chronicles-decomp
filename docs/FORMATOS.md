# Format reference

Everything is little-endian. "Validated" = read and rewritten byte-identical with the real
ROM, or checked against the game's behavior.

## ROM (`.nds`)
Standard DS header. Used: ARM9 at `0x20`, ARM7 at `0x30`, FNT `0x40`, FAT `0x48`, used size
`0x80`, capacity `0x14` (128 KB << n), **header CRC16** at `0x15E` (polynomial 0xA001, start
0xFFFF, bytes `0x000..0x15D`). Files aligned to `0x200`; the rest of the cartridge is `0xFF`.
ARM9 uncompressed and without overlays; autoload to ITCM (`0x01FF8000`, 0x6DE0 bytes) and
DTCM (`0x027E0000`). *Validated.*

## HERF (resource pack)
`u32 0x00F1A5C0, u32 n, n × {u32 hash, u32 size, u32 offset}`. Index **sorted by hash**
(binary search); data in the same order, aligned to 4 with zeros, and the end too. Hash =
DJB2 of the name with a lowercase table and a **signed** char (function `0x02009b78`).
*Validated (6 packs).*

**`erf.dict`** (inside `test.herf`): `u32 magic, u32 n, n × {u32 hash, char name[128]}`,
sorted by hash: the official names of every resource.

## `.small`
`u32 = type | size << 8`. Type `0x00`: uncompressed. Type `0x10`: the BIOS's **LZ10** (flags
for 8 items, bit 1 = a reference of `3 + (b0>>4)` bytes at `((b0&0xF)<<8 | b1) + 1` back).
When compressing, avoid distance 1 (the BIOS routine that writes to VRAM writes 16 bits at a
time). *Validated.*

## GFF V4.0 (BioWare container)
`"GFF " "V4.0" platform type version, u32 n_templates, u32 data_offset`; templates
`{label, n_fields, fields_offset, size}`; fields `{u32 label, u32 type|flags<<16, u32 offset}`.
Flags: `0x8000` list, `0x4000` struct, `0x2000` reference. Types: 0 u8, 1 s8, 2 u16, 3 s16,
4 u32, 5 s32, 6 u64, 7 s64, 8 f32, 9 f64, 10 vec3, 12 vec4, 13 quat, 14 ECString, 15 color,
16 matrix, 17 TlkString `{id, rel}`, **18 fixed point 20.12 (Sonic)**, **20 inline ASCII
string (Sonic)**, `0xFFFF` generic `{type|flags, rel}`.
ECString: `u32 n` + text, 8-bit in TLK and UTF-16 in DLG/ARE.
Used by: TLK, GDA, DLG (`CONV`), ARE, GUI, PLO...

## TLK (texts)
Root → list of `STRN {19002 id, 19003 text}`. The list is a **hash table** with linear
probing (function `0x020989fc`): slot = `h(id) % n`, where
```
u = (id << 15) - id - 1;  u = ((u >> 12) ^ u) * 5;  u = (u ^ (u >> 4)) * 0x809;  h = (u >> 16) ^ u
```
`id = 0xFFFFFFFF` is an empty slot. Texts in cp1252, with the `\0` included in the size,
without repeats, in the order of the first slot that uses them, aligned to 4 with `0xFF`.
*Validated (5 languages).*
An id whose text position is `0xFFFFFFFF` has no text in that language (6,579 of the 14,134
in English); the game's lookup does not check this (DIARIO 28).

## GDA (tables)
GFF4 `G2DA`: `gtop {10002 columns, 10003 rows}`; column = `{u32 CRC32(name.lower() in
UTF-16LE), u8 type, 3 × 0xFF}`; fixed-size rows (free bytes `0xFF`). Field types used: 5
(s32), 0 (u8), 18 (fixed point), 20 by reference (ASCII with `\0`, without repeats, aligned
to 4 with `0xFF`). Known column names: 534 of 845 (xoreos table). *Validated (229 tables).*
In the 229 tables, cells are of only four types: 5 (s32), 0 (u8), 18 (fixed point) and 20 by
reference (type `0x20000014`, with the flag); none has a float (8) or UTF-16 text (14). The
row is found by id (the value of column 0) and the column by hash (label 10001). Read by the
game's C++ in `decomp/tests/test_arquivos.cpp` (DIARIO 29).

## 2DA (text)
`2DA V2.0`, a default line, header and rows separated by TAB. Used by `.ITM` (item effects),
`.SPL` (moves), and the backgrounds' palette maps.
How the game reads it (`C2DA`, `src/Aurora/C2DA.cpp`): after the 8 header bytes, blank lines
and the spaces at the start of each line are skipped; a first line `DEFAULT: value` (or
`DEFAULT :value`) gives the value of a missing cell; the next line has the column names; each
of the others has the row name and the cells, separated by space or TAB (a quoted text may
contain spaces). `****` is an empty cell, and row and column names are lowercased. There is
also the binary form `2DA V2.b`: the column names separated by TAB or `\0`, one byte, the
number of rows (u32), the row names, one u16 position per cell, 2 bytes and the texts.
*Validated (370 tables, all `V2.0` with CRLF: 219 `.itm`/`.ITM` and 151 `.spl`/`.SPL` in
`test.herf`; 12,376 cells read by the game's C++, DIARIO 30).* None uses `DEFAULT`, quotes or
the binary form. The empty cell in the files is `***`, which the game does **not** treat as
empty: it keeps the text `"***"`, which gives 0 as an integer.

## Backgrounds
- `.cbgt`: index of 4096 × `{u16 size, u16 offset/512}` + LZ10 tiles of 64×64 at 8 bpp,
  arranged in 8×8 blocks; tiles row by row.
- `.pal`: palettes of 256 BGR555 colors (512 bytes); index 0 (magenta) is transparent.
- `.2da`: grid with `paletteNN.pal` per tile; palettes belong to 2×2 blocks, numbered column
  by column.
- `.cdpth`: same index; LZ10 tiles of 64×64 × u16, **linear**; `0x7FFF` = no object.

*Validated visually (62 areas), with the order chosen by edge continuity.*

## Nitro graphics
NCLR (`RLCN`/`TTLP`) and NCGR (`RGCN`/`RAHC`), standard NitroSDK formats. Portraits and icons
come in 4 pieces of 64×64 (`_0`..`_3`, in 2×2). Palette of each image: the `IMG` struct of the
`.gui` screens (60004 = ncgr, 60015 = nclr) or GDA rows.

## Dialogs (`.dlg`, GFF4 `CONV`)
`12002` `NTRY` nodes: `12201` text (TlkString), `12202` portrait
(`prtl_<character><emotion>`), `12208`/`12209` condition and action (`PLOT`), `12400` links.
`12000` `STRT` entries.

## Not decoded yet
`.vx` videos (Actimagine), the layout of the `.gui` screens (stretched/repeated pieces), the
Chao palettes, 311 GDA column names.
