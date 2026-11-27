#!/usr/bin/env python3
"""Exercise the normal fm11-sdl execution path without any display server.

The temporary ROMs are deliberately self-contained: the main and display
CPUs execute endless BRA loops. This exercises independent guest workers,
the host-independent clock, signal shutdown and absence of SDL_Init.
"""
import os
import signal
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: sdl_no_gui_integration.py FM11_SDL_BINARY", file=sys.stderr)
        return 2

    binary = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="fm11-no-gui-") as directory:
        root = Path(directory)
        main_rom = bytearray(0x1000)
        # F000: BRA F000; reset vector FFFE -> F000.
        main_rom[0:2] = b"\\x20\\xfe"
        main_rom[-2:] = b"\\xf0\\x00"
        display_rom = bytearray(0x5000)
        # C000: BRA C000; display reset vector FFFE -> C000.
        display_rom[0x1000:0x1002] = b"\\x20\\xfe"
        display_rom[-2:] = b"\\xc0\\x00"
        (root / "main.bin").write_bytes(main_rom)
        (root / "display.bin").write_bytes(display_rom)
        (root / "machine.conf").write_text(
            "[main_6809]\\ninstalled = true\\n"
            "[serial]\\nserial0 = none\\n",
            encoding="ascii",
        )

        # An unavailable video backend makes accidental SDL_Init fail.
        env = dict(os.environ)
        env.pop("DISPLAY", None)
        env.pop("WAYLAND_DISPLAY", None)
        env["SDL_VIDEODRIVER"] = "fm11-nonexistent-video-backend"
        env["SDL_AUDIODRIVER"] = "fm11-nonexistent-audio-backend"
        base = [
            binary, "--no-gui", "--config", str(root / "machine.conf"),
            "--main-rom", str(root / "main.bin"),
            "--rom", str(root / "display.bin"),
            "--speed", "1",
        ]
        result = subprocess.run(
            base + ["--run-for-ms", "180"], cwd=root,
            env=env, capture_output=True, text=True, timeout=12,
        )
        if result.returncode != 0 or "headless mode active" not in result.stdout:
            print("Timed headless run failed:", result, file=sys.stderr)
            return 1
        if "FM-11 execution: headless" not in result.stderr:
            print("Headless startup marker missing:", result, file=sys.stderr)
            return 2

        # Confirm that the debug monitor can be constructed without ImGui
        # or a video driver, and that SIGTERM takes the normal sync path.
        process = subprocess.Popen(
            base + ["--monitor"], cwd=root, env=env,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True,
        )
        try:
            import time
            time.sleep(0.3)
            if process.poll() is not None:
                out, err = process.communicate(timeout=3)
                print("Monitor stopped early:", out, err, file=sys.stderr)
                return 3
            process.send_signal(signal.SIGTERM)
            out, err = process.communicate(timeout=8)
            if process.returncode != 0:
                print("SIGTERM shutdown failed:", out, err, file=sys.stderr)
                return 4
            if "Debug Monitor" not in out:
                print("Monitor startup missing:", out, err, file=sys.stderr)
                return 5
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
