#!/usr/bin/env bash
# Compile + flashe un firmware STM32 (Nucleo F401RE) en copiant le .bin sur le
# lecteur mass-storage ST-Link (/media/manar/NUCLEO).
#
# Usage:
#   ./flash_stm32.sh                 # liste les firmwares STM32 disponibles
#   ./flash_stm32.sh violet_decode   # compile + copie le .bin sur NUCLEO
set -e

export PATH="$HOME/.local/bin:$PATH"
HERE="$(cd "$(dirname "$0")" && pwd)"
FW_DIR="$HERE/firmware_stm32"
FQBN="STMicroelectronics:stm32:Nucleo_64:pnum=NUCLEO_F401RE"
NUCLEO=$(ls -d /media/$USER/NUCLEO* 2>/dev/null | head -n1)

if [ -z "$1" ]; then
  echo "Firmwares STM32 disponibles :"
  for d in "$FW_DIR"/*/; do echo "  - $(basename "$d")"; done
  echo; echo "Usage: ./flash_stm32.sh <nom_firmware>"
  exit 0
fi

NAME="$1"
SKETCH="$FW_DIR/$NAME"
[ -d "$SKETCH" ] || { echo "Firmware STM32 introuvable : $NAME" >&2; exit 1; }
[ -n "$NUCLEO" ] || { echo "Lecteur NUCLEO non monté (allume/branche la carte ST-Link)." >&2; exit 1; }

# Liberer le port serie : tuer un eventuel monitor pyserial (par PID)
P=$(pgrep -f '[m]onitor.py' || true)
[ -n "$P" ] && { echo "[flash] arret du monitor pyserial (PID $P)"; kill $P 2>/dev/null || true; sleep 1; }

echo "[flash] compilation $NAME ..."
arduino-cli compile --fqbn "$FQBN" --export-binaries "$SKETCH" | tail -n 2

BIN=$(find "$SKETCH/build" -name '*.ino.bin' | head -n1)
[ -n "$BIN" ] || { echo "Binaire .bin introuvable apres compilation." >&2; exit 1; }

echo "[flash] copie $(basename "$BIN") -> $NUCLEO"
cp "$BIN" "$NUCLEO/"
sync
echo "[flash] OK (la carte clignote puis reboot). Moniteur : python3 tools/monitor.py /dev/ttyACM0 115200"
