#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile
runner,generator,mode=sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True,stdout=subprocess.DEVNULL)
    root=pathlib.Path(d)
    result=subprocess.run([runner,str(root/f"main_layers_{mode}.bin"),
        str(root/"display_layers.bin"),str(root/"ank_layers.bin"),mode],
        capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
