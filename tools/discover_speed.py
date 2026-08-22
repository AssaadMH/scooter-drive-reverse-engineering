#!/usr/bin/env python3
"""Outil de DECOUVERTE de l'octet vitesse (a utiliser avec le firmware throttle_loop_uno).

Tient deux paliers de throttle et capture les trames affichees ("thr=<V> | <hex>"),
puis analyse le champ B8B9 (periode). Pendant l'execution, NOTE le km/h de l'afficheur
a chaque palier pour calibrer SPEED_K (cf. docs/04).

Usage:
    python3 tools/discover_speed.py [--lo 1.50] [--hi 1.90] [--secs 7] [--port /dev/ttyACM0]

Rappel calibration : mph = SPEED_K / B8B9   (recalibre 2026-06-03 : 126->20 mph, 360->7 mph => SPEED_K~2520 ; ex-667 faux).
"""
import sys, time, argparse
import serial

ap = argparse.ArgumentParser()
ap.add_argument("--lo", default="1.50")
ap.add_argument("--hi", default="1.90")
ap.add_argument("--secs", type=float, default=7.0)
ap.add_argument("--port", default="/dev/ttyACM0")
ap.add_argument("--baud", type=int, default=115200)
a = ap.parse_args()

s = serial.Serial()
s.port = a.port; s.baudrate = a.baud
s.dtr = False; s.rts = False; s.timeout = 0.3
s.open(); time.sleep(2.0); s.reset_input_buffer()

def parse(line):
    if "thr=" not in line or "|" not in line:
        return None
    try:
        left, right = line.split("|", 1)
        thr = float(left.split("thr=")[1].strip().rstrip("V").strip())
        by = [int(x, 16) for x in right.split()]
        return (thr, by[:14]) if len(by) >= 14 else None
    except Exception:
        return None

def hold(cmd, secs, label):
    s.write((cmd + "\n").encode()); s.flush()
    frames = []
    t0 = time.time()
    while time.time() - t0 < secs:
        line = s.readline().decode("utf-8", "replace").rstrip()
        p = parse(line)
        if p and p[1]:
            frames.append(p[1])
    print(f"--- {label} (cmd {cmd}) : {len(frames)} trames ---")
    return frames

def b8b9_active(frames):
    # 16-bit B8B9 sur trames actives (B8 != 0xEA = pas la sentinelle d'arret)
    return [(f[8] << 8 | f[9]) for f in frames if f[8] != 0xEA]

def med(a):
    a = sorted(a)
    return a[len(a)//2] if a else None

print(">>> palier BAS (note le km/h affiche !)")
lo = hold(a.lo, a.secs, f"BAS {a.lo}V")
print(">>> palier HAUT (note le km/h affiche !)")
hi = hold(a.hi, a.secs, f"HAUT {a.hi}V")
s.write(b"n\n"); s.flush(); s.close()

al, ah = b8b9_active(lo), b8b9_active(hi)
print("\nB8B9 (periode, trames actives) :")
print(f"  {a.lo}V : mediane={med(al)}  n={len(al)}")
print(f"  {a.hi}V : mediane={med(ah)}  n={len(ah)}")
print("\n=> Avec le km/h lu a chaque palier : SPEED_K = vitesse * B8B9 (moyenne des 2 points).")
