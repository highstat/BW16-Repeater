import os, termios, time, sys

port = "/dev/cu.usbserial-10"
try:
    fd = os.open(port, os.O_RDWR | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[4] = termios.B115200
    attrs[5] = termios.B115200
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    print(">>> МОНИТОР ПОРТА ЗАПУЩЕН <<<", flush=True)
    while True:
        try:
            d = os.read(fd, 512)
            if d:
                print(d.decode("utf-8", errors="replace"), end="", flush=True)
        except BlockingIOError:
            pass
        time.sleep(0.05)
except Exception as e:
    print("Monitor error:", e)
