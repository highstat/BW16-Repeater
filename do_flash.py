#!/usr/bin/env python3
"""
BW16 Wi-Fi Repeater Firmware Flasher
Cross-platform tool supporting Windows, macOS, and Linux.
"""
import os
import sys
import time
import glob
import argparse
import subprocess
import re
import atexit
import hashlib
import shutil
import tempfile
from pathlib import Path

_SECRET_LINE = re.compile(r"(?i)(?:pass(?:word|phrase)?|psk|credential|secret|(?:sta|ap)_pass)\s*[:=]")

def print_sanitized_line(line):
    if _SECRET_LINE.search(line):
        print("[REDACTED password-like line]", flush=True)
    else:
        print(line, end="", flush=True)

def serial_line_sink():
    pending = bytearray()

    def feed(data, final=False):
        pending.extend(data)
        while True:
            try:
                end = pending.index(b"\n") + 1
            except ValueError:
                break
            line = bytes(pending[:end]).decode("utf-8", errors="replace")
            del pending[:end]
            print_sanitized_line(line)
        if final and pending:
            print_sanitized_line(bytes(pending).decode("utf-8", errors="replace"))
            pending.clear()

    return feed

def get_arduino_roots():
    roots = []
    # Environment variable overrides
    for env in ["AMEBAD_TOOLS_DIR", "ARDUINO15_DIR", "ARDUINO_DIRECTORIES_DATA"]:
        val = os.environ.get(env)
        if val and os.path.isdir(val):
            roots.append(val)

    if sys.platform.startswith("win"):
        for env_var in ["LOCALAPPDATA", "APPDATA"]:
            val = os.environ.get(env_var)
            if val:
                roots.append(os.path.join(val, "Arduino15"))
        roots.append(os.path.expanduser(r"~\\AppData\\Local\\Arduino15"))
        roots.append(os.path.expanduser(r"~\\AppData\\Roaming\\Arduino15"))
        roots.append(r"C:\\Program Files (x86)\\Arduino15")
    elif sys.platform == "darwin":
        roots.append(os.path.expanduser("~/Library/Arduino15"))
        roots.append(os.path.expanduser("~/.arduino15"))
    else:  # Linux / BSD
        roots.append(os.path.expanduser("~/.arduino15"))
        xdg = os.environ.get("XDG_DATA_HOME")
        if xdg:
            roots.append(os.path.join(xdg, "arduino15"))
    return [r for r in roots if os.path.isdir(r)]

def find_tool(custom_dir=None):
    if sys.platform.startswith("win"):
        tool_name = "upload_image_tool_windows.exe"
    elif sys.platform == "darwin":
        tool_name = "upload_image_tool_macos"
    else:
        tool_name = "upload_image_tool_linux"

    if custom_dir and os.path.isdir(custom_dir):
        candidate = os.path.join(custom_dir, tool_name)
        if os.path.exists(candidate):
            return candidate, custom_dir

    for root in get_arduino_roots():
        # Check standard path: <root>/packages/realtek/tools/ameba_d_tools/<version>
        base = os.path.join(root, "packages", "realtek", "tools", "ameba_d_tools")
        if os.path.isdir(base):
            # Sort versions descending to pick newest installed version
            try:
                versions = sorted(os.listdir(base), reverse=True)
                for v in versions:
                    v_dir = os.path.join(base, v)
                    if os.path.isdir(v_dir):
                        candidate = os.path.join(v_dir, tool_name)
                        if os.path.exists(candidate):
                            return candidate, v_dir
            except Exception:
                pass

        # Direct search if root is the ameba_d_tools directory
        candidate = os.path.join(root, tool_name)
        if os.path.exists(candidate):
            return candidate, root

        # Fallback search inside root
        for cur_root, dirs, files in os.walk(root):
            if tool_name in files:
                return os.path.join(cur_root, tool_name), cur_root

    return None, None

def find_port():
    # 1. Try pyserial if installed
    try:
        import serial.tools.list_ports
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            return ports[0]
    except ImportError:
        pass

    # 2. Windows registry detection without external dependencies
    if sys.platform.startswith("win"):
        try:
            import winreg
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\\DEVICEMAP\\SERIALCOMM")
            ports = []
            for i in range(winreg.QueryInfoKey(key)[1]):
                _, port_name, _ = winreg.EnumValue(key, i)
                ports.append(port_name)
            winreg.CloseKey(key)
            if ports:
                return ports[0]
        except Exception:
            pass
        return "COM3"

    # 3. macOS device discovery
    if sys.platform == "darwin":
        ports = [p for p in glob.glob("/dev/cu.*") if any(x in p.lower() for x in ["usbserial", "wch", "slab", "usbmodem", "uart"])]
        if ports:
            return ports[0]
        return "/dev/cu.usbserial-10"

    # 4. Linux device discovery
    ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    if ports:
        return ports[0]
    return "/dev/ttyUSB0"

def reset_and_monitor(port, duration=5.0):
    # Try pyserial
    try:
        import serial
        ser = serial.Serial(port, 115200, timeout=0.1)
        ser.dtr = False
        ser.rts = True
        time.sleep(0.15)
        ser.rts = False
        print("Board reset. Starting serial log monitor (5s):")
        sink = serial_line_sink()
        t0 = time.time()
        while time.time() - t0 < duration:
            data = ser.read(512)
            if data:
                sink(data)
            time.sleep(0.02)
        sink(b"", final=True)
        ser.close()
        return True
    except Exception:
        pass

    # POSIX ioctl fallback on macOS and Linux
    if not sys.platform.startswith("win"):
        try:
            import termios, fcntl, struct
            TIOCMBIS = 0x8004746c
            TIOCMBIC = 0x8004746b
            TIOCM_RTS = 0x0004
            TIOCM_DTR = 0x0002

            fd = os.open(port, os.O_RDWR | os.O_NONBLOCK)
            attrs = termios.tcgetattr(fd)
            attrs[4] = termios.B115200
            attrs[5] = termios.B115200
            termios.tcsetattr(fd, termios.TCSANOW, attrs)

            fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_DTR))
            fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_RTS))
            time.sleep(0.15)
            fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_RTS))
            print("Board reset. Starting serial log monitor (5s):")
            sink = serial_line_sink()
            t0 = time.time()
            while time.time() - t0 < duration:
                try:
                    d = os.read(fd, 512)
                    if d:
                        sink(d)
                except BlockingIOError:
                    pass
                time.sleep(0.02)
            sink(b"", final=True)
            os.close(fd)
            return True
        except Exception as e:
            print("Note on serial monitor:", e)

    return False

def main():
    parser = argparse.ArgumentParser(description="BW16 Wi-Fi Repeater Firmware Flasher (Cross-Platform)")
    parser.add_argument("-p", "--port", help="Serial port (e.g. COM3, /dev/ttyUSB0, /dev/cu.usbserial-10)")
    default_baud = "921600" if sys.platform == "darwin" else "1500000"
    parser.add_argument("-b", "--baud", default=default_baud, help=f"Flashing baud rate (default: {default_baud})")
    parser.add_argument("--tool-dir", help="Explicit path to ameba_d_tools directory")
    parser.add_argument("--attempts", type=int, default=30, help="Number of download mode attempts (default: 30)")
    parser.add_argument("--firmware", type=Path,
                        default=Path(__file__).resolve().parent / "bin/bw16_repeater_firmware.bin",
                        help="Exact firmware image to upload (default: repository bin image)")
    args = parser.parse_args()

    tool, tool_dir = find_tool(args.tool_dir)
    if not tool or not os.path.exists(tool):
        print("Error: AmebaD upload tool not found.")
        print("Please ensure the Realtek AmebaD Arduino core is installed or specify --tool-dir.")
        sys.exit(1)

    if not args.firmware.is_file() or args.firmware.stat().st_size == 0:
        print("Error: firmware image is missing or empty.")
        sys.exit(1)
    # The vendor uploader reads its tools directory, not the repository bin.
    # Stage a private copy so another build cannot replace the image mid-upload.
    staging = tempfile.TemporaryDirectory(prefix="bw16-flash-")
    atexit.register(staging.cleanup)
    staged_tools = Path(staging.name) / "tools"
    shutil.copytree(tool_dir, staged_tools)
    shutil.copyfile(args.firmware, staged_tools / "km0_km4_image2.bin")
    tool_dir = str(staged_tools)
    firmware_hash = hashlib.sha256((staged_tools / "km0_km4_image2.bin").read_bytes()).hexdigest()

    port = args.port or find_port()
    current_baud = str(args.baud)

    print("=" * 60)
    print("BW16 REPEATER: FLASHER (CROSS-PLATFORM)")
    print(f"OS: {sys.platform}")
    print(f"Upload Tool: {tool}")
    print(f"Target Serial Port: {port}")
    print(f"Baud Rate: {current_baud}")
    print(f"Firmware SHA256: {firmware_hash}")
    print("Instructions to enter Download Mode:")
    print("  1. Press and hold the BURN button.")
    print("  2. Press and release the RST button.")
    print("  3. Release the BURN button (do not hold it during upload).")
    print("=" * 60)

    for attempt in range(1, args.attempts + 1):
        if not args.port:
            port = find_port()
        
        # On POSIX check if path exists; on Windows COM ports are device names
        if not sys.platform.startswith("win") and not os.path.exists(port):
            print(f"Waiting for port {port}...")
            time.sleep(1)
            continue

        print(f"\n--- [Attempt {attempt}/{args.attempts}] (Baud: {current_baud}) Starting bootloader... ---")
        cmd = [tool, tool_dir, port, "{board}", "Disable", "Disable", current_baud]
        
        transfer_failed = False
        success = False
        try:
            p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            for line in iter(p.stdout.readline, ""):
                print_sanitized_line(line)
                if "All images are sent successfully" in line:
                    success = True
                if "Download fail" in line or "checksum error" in line:
                    transfer_failed = True
            p.wait()
        except Exception as e:
            print(f"Execution error on attempt {attempt}: {e}")
            time.sleep(1)
            continue

        if success:
            print("\n" + "=" * 60)
            print("Firmware flashed successfully!")
            print("=" * 60)
            time.sleep(0.5)
            reset_and_monitor(port, duration=5.0)
            sys.exit(0)
        elif transfer_failed:
            print(f"\n[!] Transfer started but failed midway (CRC checksum/baud error).")
            # Automatically try a lower baud rate if we were at 1500000 or 921600
            if current_baud == "1500000":
                current_baud = "921600"
                print(f"[>] Auto-switching baud rate down to {current_baud} for next attempt.")
            elif current_baud == "921600":
                current_baud = "115200"
                print(f"[>] Auto-switching baud rate down to {current_baud} for next attempt.")
            time.sleep(1)
        else:
            print(f"Attempt {attempt} did not catch download mode. Retrying...")
            time.sleep(1)

    print(f"\nFailed to connect to bootloader after {args.attempts} attempts.")
    sys.exit(1)

if __name__ == "__main__":
    main()
