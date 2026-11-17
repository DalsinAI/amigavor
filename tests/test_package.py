"""Host tests for the port's package step (no stove, no Amiga):

  * the Installer script (made by amicradle's scripts/ports/port_installer.py)
    agrees with package/installer.json: version, drawer, program, CPU, FPU,
    space, the GFOpenGPU modules and the settings it keeps;
  * tools/pack_dist.py packs the drawer, its icon and the Installer with its
    icon at the archive's root, names the archive after the Installer's $VER
    and writes a matching .sha256; a drawer without its icon or without an
    Installer beside it is refused;
  * tools/lha_archive.py reads back what it writes.

    python3 tests/test_package.py
MIT, Copyright (c) 2026 AmiCradle Team.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "package"
ENV = dict(re.findall(r"^([A-Z]+)=\"?([^\"\n]*)\"?$", (PKG / "package.env").read_text(), re.M))
SPEC = json.loads((PKG / "installer.json").read_text())
SCRIPT = (PKG / SPEC["script"]).read_text()
NAME = ENV["NAME"]
sys.path.insert(0, str(ROOT / "tools"))
import lha_archive  # noqa: E402

CPUS = ["68020", "68030", "68040", "68060", "68080"]


class InstallerSpecTests(unittest.TestCase):
    def test_ver_line_names_the_script_and_version(self):
        ver = re.match(r"; \$VER: (\S+) (\S+) \(\d+\.\d+\.\d{4}\)", SCRIPT.split("\n", 1)[0])
        self.assertIsNotNone(ver)
        self.assertEqual(ver.groups(), (SPEC["script"], SPEC["version"]))
        self.assertIn(f'(set #version "{SPEC["version"]}")', SCRIPT)

    def test_drawer_and_program(self):
        self.assertEqual(SPEC["drawer"], NAME)
        self.assertEqual(SPEC["script"], f"Install-{NAME}")
        self.assertIn(f'(set #drawer "{SPEC["drawer"]}")', SCRIPT)
        self.assertIn(f'(set #program "{SPEC["program"]}")', SCRIPT)
        self.assertIn(f'(set @app-name "{SPEC["name"]}")', SCRIPT)

    def test_cpu_and_fpu(self):
        cpu = SPEC.get("cpu", "68040")
        for c in CPUS:
            self.assertEqual(f'(= #cpu "{c}")' in SCRIPT, CPUS.index(c) >= CPUS.index(cpu), c)
        self.assertEqual('(database "fpu") "NOFPU"' in SCRIPT, bool(SPEC["fpu"]))

    def test_space_in_kb(self):
        self.assertIn(f"(set #space {(SPEC['space'] + 1023) // 1024})", SCRIPT)

    def test_modules_and_kept_settings(self):
        for module in SPEC.get("modules", []):
            self.assertIn(f'"LIBS:GFOpenGPU/{module}"', SCRIPT)
        for keep in SPEC.get("keep", []):
            self.assertIn(f'(tackon #dest "{keep}")', SCRIPT)

    def test_installer_icon_is_a_disk_object(self):
        self.assertEqual((PKG / f"{SPEC['script']}.info").read_bytes()[:2], b"\xe3\x10")


class PackDistTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dist = Path(tmp.name) / "dist"
        drawer = self.dist / NAME
        drawer.mkdir(parents=True)
        (drawer / SPEC["program"]).write_bytes(b"\0\0\3\xf3" + bytes(range(256)) * 4)
        (drawer / "ReadMe").write_text(f"{SPEC['name']} for AmigaOS 3.x.\n\nMore text.\n")
        icon = (PKG / f"{SPEC['script']}.info").read_bytes()
        (self.dist / f"{NAME}.info").write_bytes(icon)
        for name in (SPEC["script"], f"{SPEC['script']}.info"):
            (self.dist / name).write_bytes((PKG / name).read_bytes())
        self.out = Path(tmp.name) / "out"

    def pack(self):
        return subprocess.run([sys.executable, str(ROOT / "tools/pack_dist.py"), "--dist", str(self.dist),
                               "--name", NAME, "--out", str(self.out)], capture_output=True, text=True, cwd=ROOT)

    def test_archive_root_holds_drawer_icon_and_installer(self):
        r = self.pack()
        self.assertEqual(r.returncode, 0, r.stderr)
        lha = self.out / f"{NAME}-{SPEC['version']}-68040.lha"
        members = lha_archive.read(lha.read_bytes())
        paths = [m.path for m in members]
        self.assertEqual(sorted(p for p in paths if "/" not in p),
                         sorted([f"{NAME}.info", SPEC["script"], f"{SPEC['script']}.info"]))
        self.assertIn(f"{NAME}/{SPEC['program']}", paths)
        self.assertIn(f"{NAME}/ReadMe", paths)
        by_path = {m.path: m.data for m in members}
        self.assertEqual(by_path[SPEC["script"]], (PKG / SPEC["script"]).read_bytes())
        digest = hashlib.sha256(lha.read_bytes()).hexdigest()
        self.assertEqual((self.out / f"{lha.name}.sha256").read_text(), f"{digest}  {lha.name}\n")
        self.assertEqual(json.loads(r.stdout)["version"], SPEC["version"])
        self.assertIn(SPEC["script"], (self.out / f"{NAME}-{SPEC['version']}-68040.readme").read_text())

    def test_refuses_a_dist_without_the_installer(self):
        for name in (SPEC["script"], f"{SPEC['script']}.info"):
            (self.dist / name).unlink()
        self.assertNotEqual(self.pack().returncode, 0)

    def test_refuses_a_drawer_without_its_icon(self):
        (self.dist / f"{NAME}.info").unlink()
        self.assertNotEqual(self.pack().returncode, 0)


class LhaRoundTripTests(unittest.TestCase):
    def test_reads_back_what_it_writes(self):
        with tempfile.TemporaryDirectory() as tmp:
            members = [lha_archive.Member("D/a", b"x" * 5000 + bytes(range(256)), 0, "", 1.7e9),
                       lha_archive.Member("D/empty", b"", 0, "", 1.7e9),
                       lha_archive.Member("D.info", b"\xe3\x10" + b"\0" * 100, 0, "", 1.7e9)]
            lha = lha_archive.write(Path(tmp) / "t.lha", members)
            back = lha_archive.read(lha.read_bytes())
            self.assertEqual([(m.path, m.data) for m in back], [(m.path, m.data) for m in members])


if __name__ == "__main__":
    unittest.main()
