#!/usr/bin/env python3
"""Envoie une ou plusieurs commandes au firmware et affiche la reponse.

Usage:
    python3 tools/send.py "c8"                 # tenir 8 mph
    python3 tools/send.py "1.5" --secs 5       # manuel 1.5V, lire 5s
    python3 tools/send.py "x"                  # arret d'urgence
    python3 tools/send.py "c7" "n" --secs 10   # plusieurs commandes a la suite

Options:
    --port PORT   (defaut /dev/ttyACM0)
    --baud BAUD   (defaut 115200)
    --secs N      duree de lecture par commande (defaut 6)

Note: ouvrir le port peut resetter l'Uno (DTR). On met dtr/rts a False et on attend le boot.
Un reset relance le firmware AU REPOS (sur), il faut juste le savoir.
"""
import sys, time, argparse
import serial

ap = argparse.ArgumentParser()
ap.add_argument("cmds", nargs="+")
ap.add_argument("--port", default="/dev/ttyACM0")
ap.add_argument("--baud", type=int, default=115200)
ap.add_argument("--secs", type=float, default=6.0)
a = ap.parse_args()

s = serial.Serial()
s.port = a.port; s.baudrate = a.baud
s.dtr = False; s.rts = False; s.timeout = 0.3
s.open()
time.sleep(2.0)            # laisser le boot si reset
s.reset_input_buffer()

for c in a.cmds:
    print(f">>> {c!r}")
    s.write((c + "\n").encode()); s.flush()
    t0 = time.time()
    while time.time() - t0 < a.secs:
        line = s.readline().decode("utf-8", "replace").rstrip()
        if line:
            print("   ", line)
s.close()
