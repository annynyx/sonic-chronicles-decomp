# Sonic Chronicles mod project

This folder was created by `sonic-mod unpack`. Edit whatever you want and build the game:

```
sonic-mod pack original_rom.nds this_folder modded_rom.nds
```

`pack` compares everything with the original game and **only rewrites what changed**. It
prints a report of what was changed and checks the generated game before saving it. Play the
modified game in an emulator (melonDS or DeSmuME). Start a new save, because old saves may
not match changed tables.

> Personal use. This folder contains game data (SEGA/BioWare): don't publish it.
> To share a mod, share only the files you changed.

## What is here

| Folder | Contents | How to edit |
|---|---|---|
| `tabelas/test/` | the game's 229 tables (items, creatures, shops, areas...) | LibreOffice Calc or VS Code |
| `textos/` | every text, one file per language (`en`, `fr`, `de`, `es`, `it`) | same |
| `arquivos/test/` | text tables: item effects (`.ITM`), moves (`.SPL`)... | any text editor |

(The folder names are Portuguese: *tabelas* = tables, *textos* = texts, *arquivos* = files.)

**Spreadsheets:** save as comma-separated CSV in UTF-8. Avoid Excel: it turns numbers like
`1.5` into dates and cuts decimal places. Prefer LibreOffice (when opening, choose UTF-8 and
comma) or VS Code. `pack` also accepts `;`.

**Column types:** `pack` knows each column's type from the original table and tells you the
file, row and column when something doesn't fit:
- integers: `ID`, `Name` (which is a **text id**), `HitPoints`...
- numbers with decimals (DS fixed point): `MoveSpeed`, `ScaleModifier`... (e.g. `150.0`)
- texts/file names: ASCII only, no accents (e.g. `ITM_ACE_seed_`); an empty cell = none
- `col_xxxxxxxx` columns: the original name is still unknown (we only have its hash)

**Texts:** `textos/en.csv` has `id,text`. Texts accept accented Latin characters (the game
uses cp1252). Characters outside it, such as emoji, become `?` and `pack` warns you. Ids you
create should be new, high numbers (e.g. from `990000` up), so they don't collide with the
game's.

## Recipes

### Change a character's stats
`tabelas/test/creatures.csv`. Rows 0 to 9 are the playable characters (0 Sonic, 1 Knuckles,
2 Tails, 3 Amy, 4 Shadow, 5 Rouge, 6 Big, 7 Cream, 8 Omega, 9 Eggman); the following rows are
enemies. Columns: `HitPoints`, `Attack`, `Defense`, `Speed`, `Health`, `Power`, `Grit`,
`Luck`, `MoveSpeed`... The level-up curve is in `tabelas/test/Adv_<Character>.csv`.

### Rename anything
Find the text in `textos/en.csv` (e.g. `15877,Health Seed`) and change it. For a character's
name, see the `NameStrRef` column of `creatures.csv`. For an item, see the `Name` column of
`Items.csv`.

### Change an item
`tabelas/test/Items.csv`: `Name` and `Description` (text ids), `Type`/`SubType`, `BaseItem1`
(effects file), icons, `MinimumCost` (price). The effects are in `arquivos/test/ItemN.ITM`.
For example, `Item1.ITM` (Health Seed) has `HealHP ... 50`, and changing the 50 changes how
much the item heals.

### Add a new item (experimental)
1. In `Items.csv`, copy the row of a similar item to the end and give it a new `ID` (the next
   free number).
2. Create two new texts in `textos/en.csv`, for example `990100,Chili Dog` and
   `990101,Restores a lot of health.`, and put `990100` in `Name` and `990101` in
   `Description`.
3. Copy `arquivos/test/Item1.ITM` to `arquivos/test/Item900.ITM`, edit the effects and put
   `Item900.ITM` in `BaseItem1`. Files with a new name are **added** to the pack.
4. For the item to show up in the game, put it in a shop (`Store1.csv`... the columns don't
   have names yet; the first seems to be the item ID) or in a reward.

Warning: this flow is **experimental**. The "table + new text" mechanism was tested in the
game; a new item in a shop not yet.

### Remove an item
Prefer **taking the item out of shops and rewards** to deleting its row in `Items.csv`. Other
tables refer to items by ID, and deleting the row can leave broken references.

### New characters
A new **playable** character needs a 3D model, animations, moves and portraits, and the
game's code may limit the number of characters. That depends on the decompilation (see
`docs/PLANO-DECOMPILACAO.md` in the repository
https://github.com/annynyx/sonic-chronicles-decomp). What you can already do is a
**variation**: change the name, stats, moves and portraits of an existing character or of an
enemy in `creatures.csv`.

## Problems?
- `a linha tem N colunas` ("the row has N columns"): the editor merged or split columns.
  Check the quotes.
- The game freezes: undo the change and apply half of it at a time, to find out which row
  causes the problem. Always keep the original game file untouched.
