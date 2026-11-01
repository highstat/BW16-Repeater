#!/usr/bin/env python3
"""
BW16 Wi-Fi Repeater Serial Monitor
Cross-platform terminal monitor supporting Windows, macOS, and Linux.
"""
import os
import sys
import time
import glob
import argparse

def find_port():
    try:
        import serial.tools.list_ports
        ports = [p.device for p in serial.tools.list_ports.comports()]
        if ports:
            return ports[0]
    except ImportError:
        pass

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

    if sys.platform == "darwin":
        ports = [p for p in glob.glob("/dev/cu.*") if any(x in p.lower() for x in ["usbserial", "wch", "slab", "usbmodem", "uart"])]
        if ports:
            return ports[0]
        return "/dev/cu.usbserial-10"

    ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    if ports:
        return ports[0]
    return "/dev/ttyUSB0"

def main():
    parser = argparse.ArgumentParser(description="BW16 Serial Monitor (Cross-Platform)")
    parser.add_argument("port", nargs="?", default=None, help="Serial port (optional, auto-detected if omitted)")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    args = parser.parse_args()

    port = args.port or find_port()
    baud = args.baud

    print(f"Opening {port} at {baud} baud (Ctrl+C to exit)...")

    # 1. Try pyserial
    try:
        import serial
        ser = serial.Serial(port, baud, timeout=0.1)
        print("Serial monitor connected.")
        while True:
            data = ser.read(512)
            if data:
                print(data.decode("utf-8", errors="replace"), end="", flush=True)
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nExiting monitor.")
        sys.exit(0)
    except ImportError:
        pass
    except Exception as e:
        print("Serial error:", e)
        sys.exit(1)

    # 2. POSIX fallback on Unix
    if not sys.platform.startswith("win"):
        try:
            import termios
            fd = os.open(port, os.O_RDWR | os.O_NONBLOCK)
            attrs = termios.tcgetattr(fd)
            baud_attr = getattr(termios, f"B{baud}", termios.B115200)
            attrs[4] = baud_attr
            attrs[5] = baud_attr
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            print("Serial monitor connected (POSIX).")
            while True:
                try:
                    d = os.read(fd, 512)
                    if d:
                        print(d.decode("utf-8", errors="replace"), end="", flush=True)
                except BlockingIOError:
                    pass
                time.sleep(0.02)
        except KeyboardInterrupt:
            print("\nExiting monitor.")
            sys.exit(0)
        except Exception as e:
            print("Monitor error:", e)
            sys.exit(1)
    else:
        print("Please install pyserial for Windows serial monitoring: pip install pyserial")
        sys.exit(1)

if __name__ == "__main__":
    main()
