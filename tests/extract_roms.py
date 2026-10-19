#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile
import zipfile

script = sys.argv[1]
specs = [
    ("BootRom.h", "BootRom", "boot6809.bin", 4096),
    ("BootRom86.h", "BootRom86", "boot8088.bin", 4096),
    ("SubRom.h", "SubRom", "display6809.bin", 20480),
    ("CharRom.h", "AnkRom", "ank.bin", 4096),
    ("KanRom.h", "KanRom", "kanji.bin", 131072),
]
with tempfile.TemporaryDirectory() as tmp:
    root = pathlib.Path(tmp)
    source = root / "Fm11"
    source.mkdir()
    for hdr, symbol, _, n in specs:
        source.joinpath(hdr).write_text(
            "static const BYTE " + symbol + "[] = {\n" +
            "0x5a," * n + "\n};", encoding="ascii")
    archive = root / "original.zip"
    with zipfile.ZipFile(archive, "w") as zipfile_out:
        for hdr, _, _, _ in specs:
            zipfile_out.write(source / hdr, "Fm11/" + hdr)
    for input_source in (archive, source):
        output = root / ("zipout" if input_source == archive else "dirout")
        subprocess.run([sys.executable, script, str(input_source),
                        "--output", str(output)], check=True,
                       capture_output=True, text=True)
        for _, _, name, expected in specs:
            result = (output / name).read_bytes()
            assert len(result) == expected and result[0] == 0x5a
    source.joinpath("SubRom.h").write_text(
        "static const BYTE SubRom[] = {0x11};", encoding="ascii")
    bad = subprocess.run([sys.executable, script, str(source), "--output",
                          str(root / "badout")], capture_output=True, text=True)
    assert bad.returncode != 0
    assert not (root / "badout").exists()
