#!/usr/bin/env python3
"""Choisir une VITESSE CIBLE dans une liste (boucle PI) et voir le resultat — clavier.

Pour le firmware STM32 `throttle_pi` (commande c<mph>). On choisit une consigne avec les
chiffres, la boucle PI ajuste le throttle pour la tenir, et on voit consigne vs mesure.

TOUCHES
  1..9   : choisir une vitesse de la LISTE (mph)
  + / -  : ajuster la consigne de +/- 1 mph
  0 / n  : repos (boucle off)
  ESPACE / x : ARRET d'urgence
  q / Ctrl-C : quitter (repos puis ferme)

Usage : python3 tools/pi_keyboard_stm32.py [port] [baud]   (defaut /dev/ttyACM0 115200)

/!\\ ROUE SURELEVEE. Coupure batterie a portee. (A vide la mesure oscille, c'est normal.)
"""
import sys, os, time, termios, tty, select, serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

# liste de vitesses (mph) sur les touches 1..9
PRESETS = [4, 6, 8, 10, 12, 14, 16, 18, 20]


def main():
    s = serial.Serial()
    s.port = PORT; s.baudrate = BAUD; s.dtr = False; s.rts = False; s.timeout = 0
    try:
        s.open()
    except serial.SerialException as e:
        print("Erreur ouverture %s : %s" % (PORT, e)); return
    time.sleep(1.5); s.reset_input_buffer()

    target = 0          # 0 = repos
    meas = "?"; thr = "?"

    def send(c): s.write((c + "\n").encode()); s.flush()
    def set_speed(mph):
        nonlocal target
        target = max(0, mph)
        send("c%d" % target if target > 0 else "n")

    menu = "  ".join("%d=%dmph" % (i+1, v) for i, v in enumerate(PRESETS))

    def status():
        sys.stdout.write("\r consigne=%s mph | mesuree=%s mph | thr=%s V    "
                         "[1-9=liste  +/-  0=repos  x=ARRET  q]   "
                         % (target, meas, thr))
        sys.stdout.flush()

    fd = sys.stdin.fileno(); old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        send("n")
        print("=== BOUCLE PI — choix de vitesse (ROUE EN L'AIR) ===")
        print("Liste : " + menu)
        status()
        while True:
            r, _, _ = select.select([sys.stdin, s], [], [], 0.1)
            if sys.stdin in r:
                ch = os.read(fd, 4)
                if ch in (b'q', b'Q', b'\x03'): send("x"); break
                elif ch in (b' ', b'x', b'X'): target = 0; send("x")
                elif ch in (b'0', b'n', b'N'): set_speed(0)
                elif ch == b'+': set_speed(target + 1)
                elif ch == b'-': set_speed(target - 1)
                elif ch.isdigit():
                    i = int(ch) - 1
                    if 0 <= i < len(PRESETS): set_speed(PRESETS[i])
                status()
            if s in r:
                line = s.readline().decode("utf-8", "replace").strip()
                if "set=" in line and "meas=" in line:
                    try:
                        meas = line.split("meas=")[1].split("mph")[0].strip()
                        thr = line.split("thr=")[1].split("V")[0].strip()
                    except Exception:
                        pass
                    status()
    except KeyboardInterrupt:
        pass
    finally:
        try: send("x")
        except Exception: pass
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        s.close()
        print("\n[pi_keyboard_stm32] ferme — repos.")


if __name__ == "__main__":
    main()
