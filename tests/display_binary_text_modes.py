#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner,generator,columns=sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    subprocess.run([sys.executable,generator,directory],check=True,
                   stdout=subprocess.DEVNULL)
    root=pathlib.Path(directory)
    result=subprocess.run([runner,str(root/"main_text_stage1.bin"),
                           str(root/f"display_text_{columns}col.bin"),
                           str(root/"ank_text_stage1.bin"),columns],
                          capture_output=True,text=True)
    print(result.stdout,end="")
    print(result.stderr,end="",file=sys.stderr)
    sys.exit(result.returncode)
