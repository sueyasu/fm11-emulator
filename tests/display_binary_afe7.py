#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner,generator=sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    subprocess.run([sys.executable,generator,directory],check=True,
                   stdout=subprocess.DEVNULL)
    base=pathlib.Path(directory)
    result=subprocess.run([runner,str(base/"main_afe7.bin"),
                           str(base/"display_afe7.bin")],
                          capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
