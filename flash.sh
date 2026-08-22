#!/usr/bin/env bash
# Compile + flashe un firmware sur l'Arduino Uno (auto-detection du port).
#
# Usage:
#   ./flash.sh                       # liste les firmwares disponibles
#   ./flash.sh throttle_pi_uno       # compile + flashe ce firmware
#   ./flash.sh throttle_pi_uno /dev/ttyACM1   # port force
set -e

export PATH="$HOME/.local/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$HERE/firmware"
FQBN="arduino:avr:uno"

if [ -z "$1" ]; then
  echo "Firmwares disponibles :"
  for d in "$FW_DIR"/*/; do echo "  - $(basename "$d")"; done
  echo
  echo "Usage: ./flash.sh <nom_firmware> [port]"
  exit 0
fi

NAME="$1"
SKETCH="$FW_DIR/$NAME"
if [ ! -d "$SKETCH" ]; then
  echo "Firmware introuvable : $NAME" >&2; exit 1
fi

# Port : argument 2, sinon auto-detection (premier port avec un Uno)
PORT="$2"
if [ -z "$PORT" ]; then
  PORT=$(arduino-cli board list 2>/dev/null | awk '/arduino:avr:uno/ {print $1; exit}')
  [ -z "$PORT" ] && PORT=$(arduino-cli board list 2>/dev/null | awk '/ttyACM|ttyUSB/ {print $1; exit}')
fi
if [ -z "$PORT" ]; then
  echo "Aucun port detecte. Branche l'Arduino, ou precise le port : ./flash.sh $NAME /dev/ttyACM0" >&2
  exit 1
fi

echo "[flash] firmware=$NAME  port=$PORT  fqbn=$FQBN"

# Liberer le port : tuer un eventuel logger pyserial (par PID, jamais pkill -f sur soi-meme)
P=$(pgrep -f '[m]onitor.py' || true)
[ -n "$P" ] && { echo "[flash] arret du logger pyserial (PID $P)"; kill $P 2>/dev/null || true; sleep 1; }

echo "[flash] compilation ..."
arduino-cli compile --fqbn "$FQBN" "$SKETCH" | tail -n 2
echo "[flash] televersement ..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$SKETCH"
echo "[flash] OK. Moniteur : python3 tools/monitor.py $PORT 115200"
