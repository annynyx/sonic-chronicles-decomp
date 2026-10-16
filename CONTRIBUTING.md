# Contributing

Thanks for your interest! First read the [spirit of the project](docs/ESPIRITO.md).

## Non-negotiable rules
1. **Nothing from the game in Git:** no ROM, no assets, no extracted PNG/CSV, no assembly and
   no generated pseudo-C. That stays in `work/` / `saida/` (already in `.gitignore`).
   Addresses, function/class names and format descriptions may go in.
2. **Every format change must keep the byte-for-byte round trip**:
   `SONIC_ROM=rom.nds cargo test --release` has to pass.
3. **Every finding comes with its evidence** (function address, a count that matches,
   an emulator capture). A name given by hand goes in `analise/symbols_manual.txt` with a
   comment saying why.
4. **Matching code counts only when the rebuilt game is identical**:
   `decomp/tools/montar_rom.sh rom.nds` must print the original SHA-1
   (`f4ff8291b38bf9a33ab280fb9f7625cf4d549473`) before a file is marked `complete` in
   `config/YWSE/arm9/delinks.txt`. See [`docs/BUILD.md`](docs/BUILD.md).

## Branches
- **`main`**: stable. Only receives finished work, through a pull request from `dev`, with
  green CI. Releases are cut from here.
- **`dev`**: where day-to-day work happens. For something bigger, create a branch from it
  (`dev` → `phase0/delink`, `modding/new-item`...) and open the PR back into `dev`.

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
  cd engine && cargo fmt && cargo clippy --release && cargo test --release
  SONIC_ROM=/path/rom.nds cargo test --release
  ```
- Mistakes and discarded hypotheses are contributions too: write them down in the
  [journal](docs/DIARIO.md).

## Style
- Documentation, comments and commit messages in English, explaining the *why* (formats:
  the layout in a comment at the top of the module). Older code and file names are still in
  Portuguese; translate a comment when you touch it, but don't rename files or scripts in
  unrelated PRs, because the build and the decompilation agent depend on those names.
- Rust: no `unwrap()` on data that comes from the game; errors with context
  (`Error::Truncated { what, offset }`).
- Commit messages in the imperative mood.
