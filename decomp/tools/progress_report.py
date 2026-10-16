"""Write the decompilation progress as an objdiff progress report (report.json).

Progress trackers such as recomp.fyi read the report that objdiff
(https://github.com/encounter/objdiff) produces: a JSON file with the matched
share of code and data, per category and per translation unit, uploaded by
GitHub Actions as an artifact named VERSION_report (here YWSE_report).

objdiff-cli builds that report by diffing compiled objects against objects cut
from the game, which needs the ROM. CI has no ROM, so this script writes the
same report format from the project's own metric instead, the one
progresso.py prints (same counting rules, same files):

  - code: the "kind:code" sections of config/YWSE/arm9 (ARM9 .text and .init,
    ITCM .text);
  - data: every other section with bytes in the ROM (.rodata, .data, .ctor...;
    .bss is left out because it has no bytes);
  - matched: the bytes of the files marked "complete" in delinks.txt. A file is
    only marked complete when the rebuilt ROM is identical (SHA-1 f4ff8291...),
    so matched here means "comes from compiled source in a byte-identical build",
    never a partial match. That is why fuzzy_match_percent equals
    matched_code_percent.

Functions are counted from symbols.txt (kind:function inside a code section);
a function is matched when it lies inside a complete file.

The format is objdiff's report.proto, version 2, as objdiff serializes it:
snake_case field names, 64-bit integers as strings.

Usage:
  python3 decomp/tools/progress_report.py                 writes build/report.json
  python3 decomp/tools/progress_report.py -o other.json
"""
import argparse, json, os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import progresso  # noqa: E402  (the project metric: delinks.txt reader and library regions)

REPORT_VERSION = 2
CATEGORIES = [("game", "Game"), ("libs", "Libraries")] + [
    (lib.lower(), lib) for lib in progresso.LIBS
]
SYMBOLS = {"main": "symbols.txt", "itcm": "itcm/symbols.txt", "dtcm": "dtcm/symbols.txt"}


def new_counts():
    return {"code": [0, 0], "data": [0, 0], "functions": [0, 0], "units": [0, 0]}


def categories_of(owner):
    """Category ids for bytes owned by a library (by name) or by the game (None)."""
    return ["libs", owner.lower()] if owner else ["game"]


def read_functions(module):
    """(address, size) of every function symbol of the module."""
    path = os.path.join(progresso.CONFIG, SYMBOLS[module])
    if not os.path.exists(path):
        return []
    found = []
    for line in open(path):
        m = re.search(r"kind:function\([^)]*size=0x(\w+)[^)]*\) addr:0x(\w+)", line)
        if m:
            found.append((int(m.group(2), 16), int(m.group(1), 16)))
    return found


def split_by_owner(module, start, end):
    """Split a byte range into (owner, bytes) pieces along the library regions."""
    pieces, inside = [], 0
    for mod, a, b, lib in progresso.BIBLIOTECAS:
        if mod == module:
            n = max(0, min(end, b) - max(start, a))
            if n:
                pieces.append((lib, n))
                inside += n
    if end - start - inside:
        pieces.append((None, end - start - inside))
    return pieces


def percent(matched, total):
    return 100.0 * matched / total if total else 0.0


def measures(c):
    """objdiff Measures from counts; zero fields are left out, as objdiff does."""
    code, data, funcs, units = c["code"], c["data"], c["functions"], c["units"]
    out = {
        "fuzzy_match_percent": percent(code[0], code[1]),
        "total_code": str(code[1]),
        "matched_code": str(code[0]),
        "matched_code_percent": percent(code[0], code[1]),
        "total_data": str(data[1]),
        "matched_data": str(data[0]),
        "matched_data_percent": percent(data[0], data[1]),
        "total_functions": funcs[1],
        "matched_functions": funcs[0],
        "matched_functions_percent": percent(funcs[0], funcs[1]),
        "complete_code": str(code[0]),
        "complete_code_percent": percent(code[0], code[1]),
        "complete_data": str(data[0]),
        "complete_data_percent": percent(data[0], data[1]),
        "total_units": units[1],
        "complete_units": units[0],
    }
    return {k: v for k, v in out.items() if v not in (0, 0.0, "0")}


def build_report():
    total = new_counts()
    per_category = {cid: new_counts() for cid, _ in CATEGORIES}
    units = []

    def add(kind, cats, matched, size):
        for counts in [total] + [per_category[c] for c in cats]:
            counts[kind][0] += matched
            counts[kind][1] += size

    for module in progresso.MODULOS:
        sections, files = progresso.ler_delinks(module)
        code = {n for n, (_, _, kind) in sections.items() if kind == "code"}

        # totals: every byte of every section. Code is split between game and
        # libraries by the regions Phase 2 proved; data only counts in the total,
        # since those regions only cover code.
        for name, (start, end, kind) in sections.items():
            if kind == "bss":
                continue
            if name in code:
                for owner, n in split_by_owner(module, start, end):
                    add("code", categories_of(owner), 0, n)
            else:
                add("data", [], 0, end - start)

        # matched: the bytes of the complete files
        complete_ranges = []
        for f in files:
            prefix = f["nome"].split("/")[0]
            owner = prefix if prefix in progresso.LIBS else None
            cats = categories_of(owner)
            unit = new_counts()
            for name, start, end in f["secoes"]:
                if sections[name][2] == "bss":
                    continue
                kind = "code" if name in code else "data"
                size = end - start
                unit[kind][1] += size
                if f["complete"]:
                    unit[kind][0] += size
                    if kind == "code":
                        complete_ranges.append((start, end))
            if f["complete"]:
                add("code", cats, unit["code"][0], 0)
                add("data", [], unit["data"][0], 0)
            for counts in [total, unit] + [per_category[c] for c in cats]:
                counts["units"][1] += 1
                counts["units"][0] += f["complete"]
            unit_measures = measures(unit)
            meta = {"complete": f["complete"], "module_name": module,
                    "progress_categories": cats}
            if f["nome"].startswith(("src/", "decomp/")):
                meta["source_path"] = f["nome"]
            units.append({"name": f"{module}/{os.path.splitext(f['nome'])[0]}",
                          "measures": unit_measures, "metadata": meta})

        # functions: matched when inside a complete file's code
        for addr, size in read_functions(module):
            if not any(start <= addr < end and kind == "code"
                       for start, end, kind in sections.values()):
                continue
            owner = progresso.regiao(module, addr, addr + 1)
            matched = any(start <= addr < end for start, end in complete_ranges)
            add("functions", categories_of(owner), int(matched), 1)

    return {
        "measures": measures(total),
        "units": units,
        "version": REPORT_VERSION,
        "categories": [{"id": cid, "name": name, "measures": measures(per_category[cid])}
                       for cid, name in CATEGORIES],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("-o", "--output", default=os.path.join(progresso.REPO, "build/report.json"))
    args = parser.parse_args()
    report = build_report()
    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)
    with open(args.output, "w") as out:
        json.dump(report, out, indent=1)
        out.write("\n")
    m = report["measures"]
    print(f"{args.output}: code {m.get('matched_code_percent', 0):.2f}% "
          f"({m.get('matched_code', 0)} of {m.get('total_code', 0)} bytes), "
          f"data {m.get('matched_data_percent', 0):.2f}%, "
          f"functions {m.get('matched_functions', 0)} of {m.get('total_functions', 0)}, "
          f"units {m.get('complete_units', 0)} of {m.get('total_units', 0)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
