#!/usr/bin/env python3
import os, sys, time, glob, subprocess, termios, fcntl, struct

def find_tool():
    paths = [
        os.path.expanduser("~/Library/Arduino15/packages/realtek/tools/ameba_d_tools/1.1.3"),
        os.path.expanduser("~/.arduino15/packages/realtek/tools/ameba_d_tools/1.1.3")
    ]
    for p in paths:
        if os.path.isdir(p):
            tool_name = "upload_image_tool_macos" if sys.platform == "darwin" else "upload_image_tool_linux"
            tool_path = os.path.join(p, tool_name)
            if os.path.exists(tool_path):
                return tool_path, p
    # fallback
    for p in paths:
        if os.path.isdir(p):
            for f in os.listdir(p):
                if "upload_image_tool" in f:
                    return os.path.join(p, f), p
    return None, None

def find_port():
    ports = [p for p in glob.glob("/dev/cu.*") if any(x in p for x in ["usbserial", "wch", "slab", "usbmodem"])]
    if not ports and sys.platform != "darwin":
        ports = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*")
    return ports[0] if ports else "/dev/cu.usbserial-10"

TOOL, TOOL_DIR = find_tool()
if not TOOL or not os.path.exists(TOOL):
    print("Error: AmebaD upload tool not found. Please install the AmebaD core first.")
    sys.exit(1)

PORT = find_port()

print("=" * 60)
print("BW16 REPEATER: FLASHER")
print(f"Target Serial Port: {PORT}")
print("1. Hold the BURN button.")
print("2. Press and release the RST button.")
print("3. Release the BURN button.")
print("=" * 60)

for attempt in range(1, 21):
    PORT = find_port()
    if not os.path.exists(PORT):
        print(f"Waiting for port {PORT}...")
        time.sleep(1)
        continue

    print(f"\n--- [Attempt {attempt}/20] Starting bootloader... ---")
    cmd = [TOOL, TOOL_DIR, PORT, "{board}", "Disable", "Disable", "1500000"]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    
    success = False
    for line in iter(p.stdout.readline, ""):
        print(line, end="", flush=True)
        if "All images are sent successfully" in line:
            success = True
    p.wait()

    if success:
        print("\n" + "=" * 60)
        print("Firmware flashed successfully.")
        print("=" * 60)
        time.sleep(0.5)
        try:
            TIOCMBIS = 0x8004746c
            TIOCMBIC = 0x8004746b
            TIOCM_RTS = 0x0004
            TIOCM_DTR = 0x0002

            fd = os.open(PORT, os.O_RDWR | os.O_NONBLOCK)
            attrs = termios.tcgetattr(fd)
            attrs[4] = termios.B115200
            attrs[5] = termios.B115200
            termios.tcsetattr(fd, termios.TCSANOW, attrs)

            fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_DTR))
            fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_RTS))
            time.sleep(0.15)
            fcntl.ioctl(fd, TIOCMBIC, struct.pack("I", TIOCM_RTS))
            print("Board reset. Starting serial log monitor (5s):")
            t0 = time.time()
            while time.time() - t0 < 5.0:
                try:
                    d = os.read(fd, 512)
                    if d:
                        print(d.decode("utf-8", errors="replace"), end="", flush=True)
                except BlockingIOError:
                    pass
                time.sleep(0.02)
            os.close(fd)
        except Exception as e:
            print("Reset note:", e)
        sys.exit(0)
    else:
        print(f"Attempt {attempt} did not catch download mode. Retrying...")
        time.sleep(1)

print("\nFailed to connect to bootloader after 20 attempts.")
sys.exit(1)
