# Contributing

Thanks for your interest! First read the [spirit of the project](docs/ESPIRITO.md).

## Non-negotiable rules
1. **Nothing from the game in Git:** no ROM, no assets, no extracted PNG/CSV, no assembly and
   no generated pseudo-C. That stays in `work/` / `saida/` (already in `.gitignore`).
   Addresses, function/class names and format descriptions may go in.
2. **Every finding comes with its evidence** (function address, a count that matches,
   a test against the real game files). A name given by hand goes in `analise/symbols_manual.txt` with a
   comment saying why.
3. **Matching code counts only when the rebuilt game is identical**:
   `decomp/tools/montar_rom.sh rom.nds` must print the original SHA-1
   (`f4ff8291b38bf9a33ab280fb9f7625cf4d549473`) before a file is marked `complete` in
   `config/YWSE/arm9/delinks.txt`. See [`docs/BUILD.md`](docs/BUILD.md).

## Branches
- **`main`**: stable. Only receives finished work, through a pull request from `dev`, with
  green CI. Releases are cut from here.
- **`dev`**: where day-to-day work happens. For something bigger, create a branch from it
  (`dev` → `phase0/delink`, `phase3/ctlktable`...) and open the PR back into `dev`.

```
git switch dev && git pull
git switch -c phase0/delink        # optional, for bigger tasks
# ... work, commits ...
git push -u origin phase0/delink   # PR into dev; when dev is ready, PR dev -> main
```

## Workflow
- Pick an *issue* (the backlog follows the [plan](docs/PLANO-DECOMPILACAO.md)).
- Mention the issue in the commit or PR (`#12`), and use `Closes #12` when it completes it.
- One small PR per topic. Describe what changed and how you checked it.
- Before the PR:
  ```
  python3 decomp/tools/conferir_sem_rom.py        # the decompiled functions still match
  decomp/tools/montar_rom.sh /path/rom.nds        # the rebuilt game is identical
  make -C decomp test arquivos HERF_DIR=../work/herf
  ```
- Mistakes and discarded hypotheses are contributions too: write them down in the
  [journal](docs/DIARIO.md).
- To see a file you are writing next to the game's code, use objdiff:
  `python3 decomp/tools/objdiff_config.py` after a `montar_rom.sh`, then open the repository
  folder in [objdiff](https://github.com/encounter/objdiff). See
  [BUILD.md](docs/BUILD.md#comparing-with-objdiff).

## Naming
A name is a claim about the game, so it follows the same rule as everything else: it needs
evidence. A neutral name is better than a wrong one.

- **Unknown functions and data keep the names `dsd` gives them**: `func_0201ef68`,
  `data_020f5404` (the address in 8 lowercase hex digits).
- **Classes are named as the game's RTTI spells them** (`CExoString`, `CRules`, `ActionQueue`;
  the 290 classes are in [hierarquia_classes.md](docs/hierarquia_classes.md)). Don't invent a
  class the RTTI doesn't have without evidence for it.
- **Names found automatically** by `analise/tools/rtti.py` keep the address as a suffix, so
  two of them never collide, and say how they were found: `Class__vfuncNN_<address>` (slot
  NN of the class's vtable), `Class__dtor_<address>`, `Class__ctor_<address>`, and
  `constructs_Class_<address>` for a function that builds the object inline (a singleton's
  `Instance()`, for example), which is not the constructor itself.
- **A name given by hand** goes in `analise/symbols_manual.txt` as `Class_Method`, or a
  verb phrase for a free function (`HashResourceName`), with the evidence in the comment.
  Method names taken from what is known about other Aurora games (Neverwinter Nights) are
  hypotheses until the code agrees.
- **Library functions keep their real names** (`OS_GetOwnerInfo`, `FX_Div`), the ones in the
  NitroSDK, NitroSystem and MSL sources.
- **Decompiled functions use the compiler's names.** When a file matches,
  `nomes_do_fonte.py` writes the mangled names (`_ZN10CExoStringC1EPKc`) into `symbols.txt`,
  because that is the name the rest of the game calls ([BUILD.md](docs/BUILD.md#a-decompiled-file-enters-the-build)).
- **In the C++**, members start with `m_`; the Aurora-style type letter (`m_bLoaded`,
  `m_nNumRows`, `m_pBuffer`, `m_sString`) is welcome but not required.
- **Files**: an engine class goes in `src/Aurora/<Class>.cpp` with its header in
  `include/Aurora/<Class>.h`. A game file not decompiled yet is named in `delinks.txt` as
  `jogo/<Class>_<address>.cpp` (its most cited class and first address) or `jogo/<address>.cpp`.

## Style
- Documentation, comments and commit messages in English, explaining the *why* (formats:
  the layout in a comment at the top of the module). Older code and file names are still in
  Portuguese; translate a comment when you touch it, but don't rename files or scripts in
  unrelated PRs, because the build and the decompilation agent depend on those names.
- Commit messages in the imperative mood.
- C and C++ follow [`.clang-format`](.clang-format), the style the code already has. Format
  only what you change: `git clang-format` formats the lines changed since the last commit,
  and CI checks the lines a pull request changes, the same way.
