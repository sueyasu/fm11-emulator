#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile
runner,generator=sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True,
                   stdout=subprocess.DEVNULL)
    result=subprocess.run([runner,str(pathlib.Path(d)/"display_raster.bin")],
                          text=True,capture_output=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
