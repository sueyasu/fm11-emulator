#!/usr/bin/env python3
"""End-to-end monitor + PTY smoke test."""
import os
import re
import select
import subprocess
import sys
import tempfile
import time
import tty

def read_until(fd, token, deadline):
    data = bytearray()
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], 0.1)
        if ready:
            part = os.read(fd, 4096)
            if not part:
                raise AssertionError("unexpected EOF")
            data.extend(part)
            if token in data:
                return bytes(data)
    raise AssertionError(f"timeout waiting for {token!r}: {data!r}")

with tempfile.TemporaryDirectory() as directory:
    path = os.path.join(directory, "serial.bin")
    with open(path, "wb") as out:
        out.write(bytes([0x86, 0x58, 0xB7, 0xFD, 0x06, 0x20, 0xF9]))
    proc = subprocess.Popen([sys.argv[1], "--binary", path, "--load", "0x1000",
                             "--pc", "0x1000", "--instructions", "10000"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, bufsize=0)
    slave = None
    try:
        banner = read_until(proc.stdout.fileno(), b"fm11> ", time.monotonic()+5)
        match = re.search(rb"Serial: (/dev/pts/[0-9]+)", banner)
        assert match, banner
        slave = os.open(match.group(1).decode(), os.O_RDWR | os.O_NOCTTY)
        tty.setraw(slave)
        proc.stdin.write(b"c\n")
        data = read_until(slave, b"X", time.monotonic()+5)
        assert b"X" in data
        proc.stdin.write(b"stop\nregs\nq\n")
        out, err = proc.communicate(timeout=5)
        assert proc.returncode == 0, err
        assert b"PC=" in out, out
    finally:
        if slave is not None:
            os.close(slave)
        if proc.poll() is None:
            proc.kill()
            proc.wait()

# Verify that --clock is enforced in elapsed wall time, rather than just
# advancing peripheral time while executing instructions as fast as possible.
with tempfile.TemporaryDirectory() as directory:
    path = os.path.join(directory, "loop.bin")
    # BRA -2 is a 3-cycle infinite loop on the MC6809.
    with open(path, "wb") as out:
        out.write(bytes([0x20, 0xFE]))
    proc = subprocess.Popen([sys.argv[1], "--binary", path,
                             "--load", "0x1000", "--pc", "0x1000",
                             "--clock", "1000", "--instructions", "100"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, bufsize=0)
    try:
        read_until(proc.stdout.fileno(), b"fm11> ", time.monotonic()+5)
        start = time.monotonic()
        proc.stdin.write(b"c\\n")
        read_until(proc.stdout.fileno(), b"Instruction limit reached.",
                   time.monotonic()+5)
        elapsed = time.monotonic()-start
        assert elapsed >= 0.15, f"CPU not throttled: {elapsed:.3f}s"
        proc.stdin.write(b"q\\n")
        proc.communicate(timeout=5)
        assert proc.returncode == 0
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
