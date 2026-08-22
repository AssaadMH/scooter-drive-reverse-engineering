#!/usr/bin/env bash
# Installe arduino-cli (local, sans sudo) + le core AVR, et verifie pyserial.
set -e

BINDIR="$HOME/.local/bin"
mkdir -p "$BINDIR"
export PATH="$BINDIR:$PATH"

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "[setup] Installation d'arduino-cli dans $BINDIR ..."
  curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$BINDIR" sh
else
  echo "[setup] arduino-cli deja present : $(command -v arduino-cli)"
fi

echo "[setup] Version : $(arduino-cli version)"
echo "[setup] Mise a jour de l'index + installation du core arduino:avr ..."
arduino-cli config init >/dev/null 2>&1 || true
arduino-cli core update-index
arduino-cli core install arduino:avr

echo "[setup] Verification pyserial (pour tools/*.py) ..."
if ! python3 -c "import serial" 2>/dev/null; then
  echo "[setup]  -> pyserial manquant. Installe-le : pip install pyserial  (ou: pipx, apt install python3-serial)"
else
  echo "[setup]  -> pyserial OK"
fi

echo "[setup] Cartes detectees :"
arduino-cli board list || true

cat <<'EOF'

[setup] Termine.
  - Si arduino-cli n'est pas trouve dans un nouveau shell :
        export PATH="$HOME/.local/bin:$PATH"
  - Flasher un firmware :   ./flash.sh <nom_firmware>
EOF
