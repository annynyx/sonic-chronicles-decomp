"""Write objdiff.json, the project file of objdiff, for working on one file at a time.

objdiff (https://github.com/encounter/objdiff) shows each function of the game next to
what our C++ compiles to, instruction by instruction, and rebuilds the file every time
it is saved. It compares two objects per file ("unit"): the *target*, cut from the game
by `dsd delink` (work/build/delinks/), and the *base*, compiled from src/.

`dsd objdiff` already lists every unit of config/YWSE/arm9/delinks.txt. This script runs
it with the project's settings and fixes what does not fit this repository:

  - paths become relative to the repository, where objdiff.json is written;
  - a game file not decompiled yet (jogo/...) has no source, so it keeps only the
    target: objdiff shows the game's code, and nothing is built for it;
  - objdiff rebuilds a base with decomp/tools/objdiff_build.sh, which calls compilar.sh
    (the same compiler and flags as montar_rom.sh);
  - decomp.me scratches use mwcc_30_136, which is mwccarm 2.0 sp2 (its mwccarm.exe says
    "FileVersion 3.0 build 136"), with the flags compilar.sh --flags prints. The
    libraries get no scratch: they use other flags and are already linked from source.

objdiff pairs functions by symbol name. A file still in progress shows both sides when
its entry in delinks.txt is named after its source (src/Aurora/CRules.cpp instead of
jogo/0201ef68.cpp; without "complete" the build still links the game's code, so the ROM
stays identical) and symbols.txt has the names the compiler gives its functions.

Needs the extracted and delinked game: run decomp/tools/montar_rom.sh rom.nds first.
objdiff.json is generated, so it stays out of Git.

Usage:
  python3 decomp/tools/objdiff_config.py      then open the repository folder in objdiff
"""
import json, os, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOLS = os.environ.get("FERRAMENTAS", os.path.join(REPO, "work", "ferramentas"))
CONFIG = "config/YWSE/arm9/config.yaml"
COMPILER = "mwcc_30_136"  # mwccarm 2.0 sp2 on decomp.me
LIBS = ("NitroSDK/", "NitroSystem/", "MSL/")


def main():
    os.chdir(REPO)
    dsd = os.path.join(TOOLS, "bin", "dsd")
    if not os.access(dsd, os.X_OK):
        sys.exit("missing tools: run decomp/tools/ferramentas.sh")
    if not os.path.isdir("work/build/delinks"):
        sys.exit("missing work/build/delinks: run decomp/tools/montar_rom.sh rom.nds")
    flags = subprocess.run(["decomp/tools/compilar.sh", "--flags"], check=True,
                           capture_output=True, text=True).stdout.strip()
    out = subprocess.run([dsd, "objdiff", "-c", CONFIG, "--stdout", "--scratch",
                          "--compiler", COMPILER, "--c-flags", flags,
                          "--custom-make", "decomp/tools/objdiff_build.sh"],
                         check=True, capture_output=True, text=True).stdout
    project = json.loads(out)

    def rel(path):
        return os.path.relpath(os.path.normpath(path), REPO)

    for key in ("target_dir", "base_dir"):
        project[key] = rel(project[key])
    with_base = 0
    for unit in project["units"]:
        for key in ("target_path", "base_path"):
            if key in unit:
                unit[key] = rel(unit[key])
        if "scratch" in unit:
            unit["scratch"]["ctx_path"] = rel(unit["scratch"]["ctx_path"])
        source = unit.get("metadata", {}).get("source_path", "")
        if source.startswith(LIBS):
            unit.pop("scratch", None)
        elif "base_path" in unit and not os.path.isfile(source):
            del unit["base_path"]
        with_base += "base_path" in unit

    with open("objdiff.json", "w") as f:
        json.dump(project, f, indent=2)
        f.write("\n")
    print(f"objdiff.json: {len(project['units'])} units, {with_base} with a compiled side")


if __name__ == "__main__":
    main()
