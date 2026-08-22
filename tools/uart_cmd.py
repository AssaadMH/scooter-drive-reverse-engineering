#!/usr/bin/env python3
"""Envoie une (ou plusieurs) commande(s) a la carte et lit la telemetrie qq secondes.

Usage: python3 tools/uart_cmd.py [port] [secondes] [cmd1] [cmd2] ...
  ex:  python3 tools/uart_cmd.py /dev/ttyACM0 6            # lecture seule 6s
       python3 tools/uart_cmd.py /dev/ttyACM0 6 m2.0       # throttle 2.0V puis lit 6s
       python3 tools/uart_cmd.py /dev/ttyACM0 8 m2.0 f     # throttle puis frein force
Ne reset PAS la carte (dtr/rts=False).
"""
import sys, time, threading
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
cmds = sys.argv[3:]

s = serial.Serial()
s.port = port; s.baudrate = 115200
s.dtr = False; s.rts = False; s.timeout = 0.5
s.open()

stop = threading.Event()
def reader():
    while not stop.is_set():
        try: line = s.readline()
        except Exception: break
        if line:
            print(line.decode("utf-8", "replace").rstrip(), flush=True)
t = threading.Thread(target=reader, daemon=True); t.start()

time.sleep(1.0)  # laisse arriver une 1re trame telemetrie
for c in cmds:
    s.write((c + "\n").encode()); s.flush()
    print(f"[>> {c}]", flush=True)
    time.sleep(0.4)

time.sleep(secs)
stop.set(); time.sleep(0.2); s.close()
