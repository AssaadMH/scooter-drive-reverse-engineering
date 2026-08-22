#!/usr/bin/env python3
"""Pilotage du throttle au CLAVIER (fleches), en direct.

Envoie des consignes de tension au firmware `drive_logger_uno` (ou `throttle_injector_uno`)
qui accepte : un nombre = tension cible, `n` = repos, `x` = ARRET d'urgence.

TOUCHES
  Fleche HAUT   : +0.10 V   (accelerer)
  Fleche BAS    : -0.10 V   (ralentir)
  Fleche DROITE : +0.05 V   (cran fin +)
  Fleche GAUCHE : -0.05 V   (cran fin -)
  n             : repos (0.80 V)
  ESPACE ou x   : ARRET d'urgence (repos immediat)
  q ou Ctrl-C   : quitter (met au repos puis ferme)

Affiche en continu la consigne + la vitesse lue (spd) + l'etat frein.

Usage:
    python3 tools/throttle_keyboard.py [port] [baud]
Defauts: /dev/ttyACM0 115200

/!\\ ROUE SURELEVEE. Demarre au repos. VMAX plafonne la consigne cote PC ; le firmware
    a aussi son propre plafond (MAX_V). Garder la coupure batterie a portee.
"""
import sys, os, time, termios, tty, select, serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

IDLE = 0.80          # tension repos
VMIN = IDLE          # on ne descend pas sous le repos
VMAX = 2.80          # plafond de securite cote PC (= MAX_V du firmware)
STEP = 0.10          # pas fleches haut/bas
FINE = 0.05          # pas fleches gauche/droite


def main():
    s = serial.Serial()
    s.port = PORT
    s.baudrate = BAUD
    s.dtr = False        # eviter (autant que possible) le reset a l'ouverture
    s.rts = False
    s.timeout = 0
    try:
        s.open()
    except serial.SerialException as e:
        print("Erreur ouverture %s : %s" % (PORT, e))
        print("-> Ferme tout autre programme qui utilise le port (logger, send.py, autre moniteur).")
        return
    time.sleep(2.0)      # laisser le boot si l'Uno a quand meme reset
    s.reset_input_buffer()

    target = IDLE
    spd = "?"
    frein = "?"

    def send_val(v):
        s.write(("%.2f\n" % v).encode())
        s.flush()

    def send_cmd(c):
        s.write((c + "\n").encode())
        s.flush()

    def status():
        sys.stdout.write(
            "\r consigne=%.2f V | spd=%s mph | frein=%s    "
            "[HAUT/BAS +-0.10  GAUCHE/DROITE +-0.05  n=repos  ESPACE/x=ARRET  q=quitter]   "
            % (target, spd, frein)
        )
        sys.stdout.flush()

    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setcbreak(fd)
        send_cmd("n")                 # demarrer au repos
        print("=== PILOTAGE THROTTLE CLAVIER ===  (ROUE EN L'AIR)")
        status()
        while True:
            r, _, _ = select.select([sys.stdin, s], [], [], 0.1)

            if sys.stdin in r:
                ch = os.read(fd, 8)
                if   ch == b'\x1b[A':                       # HAUT
                    target = min(VMAX, round(target + STEP, 2)); send_val(target)
                elif ch == b'\x1b[B':                       # BAS
                    target = max(VMIN, round(target - STEP, 2)); send_val(target)
                elif ch == b'\x1b[C':                       # DROITE
                    target = min(VMAX, round(target + FINE, 2)); send_val(target)
                elif ch == b'\x1b[D':                       # GAUCHE
                    target = max(VMIN, round(target - FINE, 2)); send_val(target)
                elif ch in (b'n', b'N'):                    # repos
                    target = IDLE; send_cmd("n")
                elif ch in (b' ', b'x', b'X'):              # ARRET d'urgence
                    target = IDLE; send_cmd("x")
                elif ch in (b'q', b'Q', b'\x03'):           # quitter
                    send_cmd("x"); break
                status()

            if s in r:
                line = s.readline().decode("utf-8", "replace").strip()
                if "spd=" in line:
                    try:
                        spd = line.split("spd=")[1].split("mph")[0].strip()
                        if "FREIN=" in line:
                            frein = line.split("FREIN=")[1].split()[0]
                    except Exception:
                        pass
                    status()
    except KeyboardInterrupt:
        pass
    finally:
        try:
            send_cmd("x")             # securite : repos en sortant
        except Exception:
            pass
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
        s.close()
        print("\n[throttle_keyboard] ferme — throttle au repos.")


if __name__ == "__main__":
    main()
