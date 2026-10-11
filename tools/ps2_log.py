#!/usr/bin/env python3
"""
Receives the logs sent by the OPL debug build (UDP broadcast, port 18194).

Usage:
  python ps2_log.py

Leave it running while you use OPL / the game. Ctrl+C to quit.
Everything is also saved to ps2_log.txt.

Nothing showing up?
  - The PC and the PS2 have to be on the same network (same router).
  - Windows may block the port: the first time you run it, allow it in the
    firewall prompt (tick "Private networks").
"""
import socket
import time

PORT = 18194

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("0.0.0.0", PORT))

log = open("ps2_log.txt", "a", encoding="utf-8")
t0 = time.time()
print(f"Listening for PS2 logs on UDP {PORT}... (Ctrl+C to quit)")
log.write(f"\n===== session {time.strftime('%Y-%m-%d %H:%M:%S')} =====\n")

buf = ""
try:
    while True:
        data, addr = sock.recvfrom(4096)
        buf += data.decode("latin-1", errors="replace")
        while "\n" in buf:
            line, buf = buf.split("\n", 1)
            out = f"[{time.time() - t0:8.3f}] {line.rstrip()}"
            print(out)
            log.write(out + "\n")
            log.flush()
except KeyboardInterrupt:
    print("Bye.")
