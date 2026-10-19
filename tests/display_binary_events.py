#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner, generator = sys.argv[1:]
with tempfile.TemporaryDirectory() as folder:
    subprocess.run([sys.executable,generator,folder],check=True,
                   stdout=subprocess.DEVNULL)
    result=subprocess.run([runner,str(pathlib.Path(folder)/"main_events.bin")],
                          capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
