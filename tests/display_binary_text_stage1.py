#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner,generator=sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True,stdout=subprocess.DEVNULL)
    root=pathlib.Path(d)
    result=subprocess.run([runner,str(root/"main_text_stage1.bin"),
        str(root/"display_text_stage1.bin"),str(root/"ank_text_stage1.bin")],
        capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
