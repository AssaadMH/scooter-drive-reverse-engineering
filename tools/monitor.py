#!/usr/bin/env python3
"""Console serie persistante BIDIRECTIONNELLE (pyserial).

Affiche ce que la carte envoie ET transmet ce que tu tapes (ligne + Entree).
Pourquoi pas `arduino-cli monitor` ? En tache de fond son stdin = /dev/null -> il lit un EOF
et quitte aussitot. Cette console reste attachee, log en continu, et accepte la saisie clavier.

Usage:
    python3 tools/monitor.py [port] [baud] [logfile]
Defaults: /dev/ttyACM0 115200 /tmp/uart_monitor.log

Tape une commande (ex: m2.0) puis Entree -> envoyee a la carte (avec '\\n').
Ne reset PAS la carte (dtr/rts a False). Ctrl-C ou Ctrl-D pour arreter.
"""
import sys, time, threading
import serial  # pip install pyserial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
logf = sys.argv[3] if len(sys.argv) > 3 else "/tmp/uart_monitor.log"

s = serial.Serial()
s.port = port
s.baudrate = baud
s.dtr = False          # ne pas resetter la carte a l'ouverture
s.rts = False
s.timeout = 1
s.open()

print(f"[monitor] {port} @ {baud} -> {logf}  (tape une commande + Entree ; Ctrl-C pour arreter)")

stop = threading.Event()

def reader():
    """Thread : lit le serie, affiche + log."""
    with open(logf, "a", buffering=1) as f:
        f.write(f"\n--- moniteur {baud} attache {time.strftime('%H:%M:%S')} ---\n")
        while not stop.is_set():
            try:
                line = s.readline()
            except Exception:
                break
            if line:
                txt = line.decode("utf-8", "replace").rstrip()
                print(txt)
                f.write(txt + "\n")

t = threading.Thread(target=reader, daemon=True)
t.start()

try:
    for cmd in sys.stdin:                 # bloque jusqu'a une ligne + Entree
        cmd = cmd.rstrip("\n")
        s.write((cmd + "\n").encode())    # le firmware lit avec readStringUntil('\n')
        s.flush()
        print(f"[>> envoye] {cmd!r}")
except KeyboardInterrupt:
    pass
finally:
    stop.set()
    time.sleep(0.2)
    s.close()
