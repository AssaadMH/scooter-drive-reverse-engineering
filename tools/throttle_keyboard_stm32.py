#!/usr/bin/env python3
"""Pilotage throttle au CLAVIER pour le firmware STM32 `throttle_drive` (Nucleo F401RE).

Le STM32 sort du 3,3 V et le noeud est attenue (~0,8 x commande), donc la roue ne
demarre que vers ~2,0 V commande (noeud ~1,6 V). VMAX = 3,0 V.

TOUCHES
  Fleche HAUT   : +0.15 V      Fleche BAS    : -0.15 V
  Fleche DROITE : +0.05 V      Fleche GAUCHE : -0.05 V
  n             : repos (0.80 V)
  ESPACE / x    : ARRET d'urgence (repos)
  q / Ctrl-C    : quitter (repos puis ferme)

Affiche consigne + vitesse lue (spd) + frein.

Usage : python3 tools/throttle_keyboard_stm32.py [port] [baud]
Defaut: /dev/ttyACM0 115200    (le Nucleo apparait sur ttyACM0)

/!\\ ROUE SURELEVEE. Coupure batterie a portee. La roue ne bouge qu'a partir de ~2,0 V.
"""
import sys, os, time, termios, tty, select, serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

IDLE = 0.80
VMIN = IDLE
VMAX = 3.00          # = MAX_V du firmware STM32
STEP = 0.15
FINE = 0.05


def main():
    s = serial.Serial()
    s.port = PORT; s.baudrate = BAUD
    s.dtr = False; s.rts = False; s.timeout = 0
    try:
        s.open()
    except serial.SerialException as e:
        print("Erreur ouverture %s : %s" % (PORT, e)); return
    time.sleep(1.5); s.reset_input_buffer()

    target = IDLE; spd = "?"; frein = "?"

    def send_val(v): s.write(("%.2f\n" % v).encode()); s.flush()
    def send_cmd(c): s.write((c + "\n").encode()); s.flush()

    def status():
        sys.stdout.write(
            "\r consigne=%.2f V | spd=%s | frein=%s   "
            "[HAUT/BAS +-0.15  G/D +-0.05  n=repos  ESPACE/x=ARRET  q=quitter]   "
            % (target, spd, frein))
        sys.stdout.flush()

    fd = sys.stdin.fileno(); old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        send_cmd("n")
        print("=== PILOTAGE THROTTLE CLAVIER (STM32) ===  (ROUE EN L'AIR, demarre vers ~2.0V)")
        status()
        while True:
            r, _, _ = select.select([sys.stdin, s], [], [], 0.1)
            if sys.stdin in r:
                ch = os.read(fd, 8)
                if   ch == b'\x1b[A': target = min(VMAX, round(target + STEP, 2)); send_val(target)
                elif ch == b'\x1b[B': target = max(VMIN, round(target - STEP, 2)); send_val(target)
                elif ch == b'\x1b[C': target = min(VMAX, round(target + FINE, 2)); send_val(target)
                elif ch == b'\x1b[D': target = max(VMIN, round(target - FINE, 2)); send_val(target)
                elif ch in (b'n', b'N'): target = IDLE; send_cmd("n")
                elif ch in (b' ', b'x', b'X'): target = IDLE; send_cmd("x")
                elif ch in (b'q', b'Q', b'\x03'): send_cmd("x"); break
                status()
            if s in r:
                line = s.readline().decode("utf-8", "replace").strip()
                if "spd=" in line:
                    try:
                        spd = line.split("spd=")[1].split("|")[0].replace("mph", "").strip()
                        if "FREIN=" in line:
                            frein = line.split("FREIN=")[1].split()[0].split("|")[0]
                    except Exception:
                        pass
                    status()
    except KeyboardInterrupt:
        pass
    finally:
        try: send_cmd("x")
        except Exception: pass
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        s.close()
        print("\n[throttle_keyboard_stm32] ferme — throttle au repos.")


if __name__ == "__main__":
    main()
