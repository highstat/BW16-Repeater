#!/usr/bin/env python3
"""Select a diagnostic topology after reboot and print sanitized Serial output."""
import argparse
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from do_flash import print_sanitized_line


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbserial-10")
    parser.add_argument("--mode", choices=("s", "c", "b", "p"))
    parser.add_argument("--seconds", type=int, default=120)
    args = parser.parse_args()
    with serial.Serial(args.port, 115200, timeout=0.2) as connection:
        if args.mode:
            connection.write(b"\nreboot\n")
        pending = bytearray()
        selected = False
        deadline = time.monotonic() + args.seconds
        while time.monotonic() < deadline:
            pending.extend(connection.read(4096))
            while b"\n" in pending:
                line, _, tail = pending.partition(b"\n")
                pending = bytearray(tail)
                decoded = line.decode("utf-8", errors="replace") + "\n"
                print_sanitized_line(decoded)
                if args.mode and not selected and "[DIAG] Select within" in decoded:
                    connection.write(args.mode.encode("ascii"))
                    selected = True
                    print("Selected diagnostic topology:", args.mode, flush=True)
        if args.mode and not selected:
            raise SystemExit("Diagnostic boot selector was not observed")


if __name__ == "__main__":
    main()
