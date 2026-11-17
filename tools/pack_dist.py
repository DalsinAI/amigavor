#!/usr/bin/env python3
"""Pack a built drawer as a release archive: NAME/, NAME.info and the Installer
script (Install-*) found in DIST, as NAME-VERSION-68040.lha with .sha256 and
.readme. VERSION is the Installer script's $VER. Prints a JSON summary."""
# MIT, Copyright (c) 2026 AmiCradle Team.
import argparse, hashlib, json, os, re, subprocess, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import lha_archive

ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
ap.add_argument("--dist", required=True, type=Path)
ap.add_argument("--name", required=True)
ap.add_argument("--out", required=True, type=Path)
a = ap.parse_args()

drawer = a.dist / a.name
icon = a.dist / f"{a.name}.info"
installers = sorted(p for p in a.dist.glob("Install-*") if p.is_file())
script = next((p for p in installers if p.suffix != ".info"), None)
if not drawer.is_dir() or not icon.is_file() or script is None:
    raise SystemExit(f"{a.dist}: needs {a.name}/, {a.name}.info and an Install- script")
m = re.match(rb"; \$VER: \S+ (\S+) \(", script.read_bytes().split(b"\n", 1)[0])
if not m:
    raise SystemExit(f"{script.name}: no $VER line")
version = m.group(1).decode()

commit = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
stamp = float(subprocess.run(["git", "log", "-1", "--format=%ct"], capture_output=True, text=True).stdout.strip() or 0)
os.environ["TZ"] = "UTC"
time.tzset()

members = []
for p in sorted(drawer.rglob("*")):
    if p.is_file():
        members.append(lha_archive.Member(f"{a.name}/{p.relative_to(drawer).as_posix()}", p.read_bytes(), 0, "", stamp))
members.append(lha_archive.Member(f"{a.name}.info", icon.read_bytes(), 0, "", stamp))
for p in installers:
    members.append(lha_archive.Member(p.name, p.read_bytes(), 0, "", stamp))

base = f"{a.name}-{version}-68040"
a.out.mkdir(parents=True, exist_ok=True)
lha = lha_archive.write(a.out / f"{base}.lha", members)
back = lha_archive.read(lha.read_bytes())
if [(x.path, x.data) for x in back] != [(x.path, x.data) for x in members]:
    raise SystemExit("the archive does not read back as written")
digest = hashlib.sha256(lha.read_bytes()).hexdigest()
(a.out / f"{base}.lha.sha256").write_text(f"{digest}  {lha.name}\n")
readme = drawer / "ReadMe"
head = readme.read_text(errors="replace").split("\n\n")[0] if readme.is_file() else a.name
(a.out / f"{base}.readme").write_text(
    f"Short:        {a.name} {version} for AmigaOS 3.x, 68040 + FPU, GFOpenGPU SDL 2 and GL\n"
    f"Version:      {version}\nArchitecture: m68k-amigaos >= 3.2\nSource:       commit {commit}\n\n"
    f"{head}\n\nUnpack the archive and run {script.name}, or copy the {a.name} drawer where you like.\n")
print(json.dumps({"archive": lha.name, "bytes": lha.stat().st_size, "sha256": digest,
                  "version": version, "members": len(members)}, indent=2))
