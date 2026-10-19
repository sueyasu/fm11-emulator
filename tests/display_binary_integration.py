#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner, generator = sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True)
    output=subprocess.run([runner,str(pathlib.Path(d)/"main_test.bin"),
                           str(pathlib.Path(d)/"display_test.bin")],
                          text=True,capture_output=True)
    print(output.stdout,end="")
    print(output.stderr,end="",file=sys.stderr)
    sys.exit(output.returncode)
