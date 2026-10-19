#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile

runner, generator = sys.argv[1:3]
handshake = len(sys.argv) == 4 and sys.argv[3] == "--handshake"
if len(sys.argv) not in (3, 4) or (len(sys.argv) == 4 and not handshake):
    sys.exit(2)
with tempfile.TemporaryDirectory() as d:
    subprocess.run([sys.executable,generator,d],check=True)
    main_name = "main_handshake.bin" if handshake else "main_test.bin"
    sub_name = "display_handshake.bin" if handshake else "display_test.bin"
    args=[runner,str(pathlib.Path(d)/main_name),str(pathlib.Path(d)/sub_name)]
    if handshake:
        args.append("--handshake")
    output=subprocess.run(args,
                          text=True,capture_output=True)
    print(output.stdout,end="")
    print(output.stderr,end="",file=sys.stderr)
    sys.exit(output.returncode)
