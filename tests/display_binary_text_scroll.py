#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile
runner,generator,mode=sys.argv[1:]
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True,stdout=subprocess.DEVNULL)
    root=pathlib.Path(d)
    result=subprocess.run([runner,str(root/"main_text_stage1.bin"),
                           str(root/f"display_text_scroll_{mode}.bin"),
                           str(root/"ank_text_scroll.bin"),mode],
                          capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
