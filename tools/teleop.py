#!/usr/bin/env python3
# Teleop clavier SHADOW — pilote le firmware violet_d10 (throttle D6 + violet D10).
# Touche = action immediate (pas besoin d'Entree). Roue SURELEVEE. Ctrl-C ou 'p' pour quitter.
#
#   a = accelerer (+0.1)      z = ralentir (-0.1)      x / ESPACE-bas = repos (0.8V)
#   ESPACE = FREIN (bascule)  f = FEU (bascule)
#   s = mode S                d = mode D
#   g = clignotant GAUCHE (bascule)   h = clignotant DROIT (bascule)
#   1 / 2 / 3 = gear          p = quitter
#
# Usage : python3 tools/teleop.py [/dev/ttyACM0]

import sys, termios, tty, threading, time
import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
IDLE, TMAX, TMIN = 0.80, 2.20, 0.80

ser = serial.Serial()
ser.port = PORT; ser.baudrate = 115200
ser.dtr = False; ser.rts = False
ser.timeout = 0.1
ser.open()

# etat local
thr = IDLE
brake = feu = modeD = cligG = cligD = False
gear = 3
running = True

def send(s):
    ser.write((s + "\n").encode())
    ser.flush()

def reader():
    while running:
        try:
            line = ser.readline().decode(errors="ignore").strip()
            if line:
                sys.stdout.write("\r\033[K" + line + "\n" + hud() )
                sys.stdout.flush()
        except Exception:
            break

def hud():
    return ("\r[THR %.2f | frein %s | feu %s | mode %s | gear %d | cligG %s cligD %s]  "
            % (thr, "ON " if brake else "off", "ON " if feu else "off",
               "D" if modeD else "S", gear, "ON" if cligG else "--", "ON" if cligD else "--"))

def show():
    sys.stdout.write("\r\033[K" + hud()); sys.stdout.flush()

HELP = """
=== TELEOP SHADOW (roue surelevee !) ===
  a/z = accelerer/ralentir   x = repos
  ESPACE = frein (bascule)   f = feu (bascule)
  s = mode S    d = mode D
  g = cligno gauche   h = cligno droit  (bascule)
  1/2/3 = gear        p = quitter
"""

def main():
    global thr, brake, feu, modeD, cligG, cligD, gear, running
    print(HELP)
    send("x")  # repos au demarrage
    t = threading.Thread(target=reader, daemon=True); t.start()
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        show()
        while True:
            c = sys.stdin.read(1)
            if c == 'p':
                break
            elif c == 'a':
                thr = min(TMAX, round(thr + 0.1, 2)); send("m%.2f" % thr)
            elif c == 'z':
                thr = max(TMIN, round(thr - 0.1, 2)); send("m%.2f" % thr)
            elif c == 'x':
                thr = IDLE; send("x")
            elif c == ' ':
                brake = not brake; send("f" if brake else "r")
            elif c == 'f':
                feu = not feu; send("L" if feu else "l")
            elif c == 's':
                modeD = False; send("S")
            elif c == 'd':
                modeD = True; send("D")
            elif c == 'g':
                cligG = not cligG; send("g" if cligG else "G")
            elif c == 'h':
                cligD = not cligD; send("h" if cligD else "H")
            elif c in ('1', '2', '3'):
                gear = int(c); send(c)
            show()
    finally:
        running = False
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        send("x")  # securite : repos en quittant
        time.sleep(0.2)
        ser.close()
        print("\n[teleop termine — throttle au repos]")

if __name__ == "__main__":
    main()
