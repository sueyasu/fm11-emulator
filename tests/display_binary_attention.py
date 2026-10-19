#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner, generator = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    subprocess.run([sys.executable,generator,directory],
                   capture_output=True,text=True,check=True)
    result=subprocess.run([runner,str(pathlib.Path(directory)/"main_attention.bin")],
                          capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
