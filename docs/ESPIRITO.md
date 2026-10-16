# The spirit of the project

This project started with a simple question: *"can you open a DS game and understand how it
works?"* This document says how we work and why.

## 1. Learning is the goal, not a side effect
Every tool is written to be **read**, not only used. The code is commented explaining *why*
each byte is what it is; the formats are documented in [`FORMATOS.md`](FORMATOS.md); the
story of how each thing was found is in [`DIARIO.md`](DIARIO.md). If you don't understand a
part, that is a bug in the documentation.

## 2. Nothing counts until it is checked against the game
A hypothesis that "looks right" is not a finding. Examples of what that means here:

- **Formats:** every writer must reproduce the original file **byte for byte** when nothing
  changes (229 tables, 5 TLKs, 6 packs: all identical).
- **Rewritten functions:** `HashResourceName` in C recomputes the game's 8,690 names and gets
  all of them right.
- **Images:** the tile order of the backgrounds was chosen by *measuring* edge continuity
  (4.8 against 35 to 56 for the other hypotheses), not by eye.
- **Mods:** opened in the emulator and checked by screenshot.
- **Matching decompilation:** a file counts as decompiled only when the whole game, rebuilt
  with it, is byte-identical to the original.

## 3. Mistakes are recorded, not hidden
Several hypotheses were wrong: the "constructors" that were not constructors, the
destructors at the wrong vtable slot, the 12 false names accepted through hash collisions,
the portraits painted with Tails' palette. Each one is in [`DIARIO.md`](DIARIO.md) with the
evidence that brought the hypothesis down. That is worth more than pretending we got it
right the first time.

## 4. Honesty about what doesn't work
The README and the documents say clearly what is done, what is experimental and what is only
a plan. "Tested in the emulator" means it was tested in the emulator. "Not tested" is written
down.

## 5. Nothing from the game in the repository
The game belongs to SEGA and BioWare. Here there is only **knowledge and tools**: code we
wrote, documentation and metadata (addresses, names). Everything derived from the game, such
as the ROM, assets, assembly and pseudo-C, is generated locally from the user's own copy and
stays out of Git (`work/`, `saida/`, `.gitignore`). This is the same stance as projects like
[pret](https://github.com/pret).

## 6. Reusable tools
The `sonic-formats` library knows nothing about disks or command lines: it only reads and
writes formats. That way the same pieces serve the extractor, the modding tools and, one day,
a native executable of the game.

## 7. Small, verifiable steps
The [decompilation plan](PLANO-DECOMPILACAO.md) is split into parts of 1 to 5 days, each
with an objective "done" criterion. A small, checked PR is worth more than a big, doubtful
one.
