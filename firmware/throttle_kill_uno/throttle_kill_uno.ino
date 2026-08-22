/*
 * THROTTLE KILL — arrêt logiciel FORT du moteur
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : quand la ligne throttle (gris/blanc) est tenue par un BIAIS (~1.5 V, au-dessus du
 *       seuil de démarrage) et que le moteur surge tout seul, on force D11 = 0 V pour
 *       TIRER ACTIVEMENT le nœud vers le bas à travers la R 1 kΩ et passer sous le seuil.
 *       0 V = "zéro accélérateur" => l'ESC ne peut pas accélérer.
 *
 * CABLAGE (inchangé) :
 *   D11 --[R 1k]--+-- gris/blanc (côté contrôleur)   [throttle : ici forcé à 0 V]
 *                 +--[C 10uF]-- GND
 *   A1  --------- nœud gris/blanc (relecture tension réelle)
 *   D10 --------- fil BLEU (UART 9600, tap //)        [lecture vitesse, pour vérifier]
 *   D8  --------- non connecté (TX factice)
 *   GND --------- fil NOIR
 *   ☠️ rien sur rouge/orangé (52 V).  ROUE SURÉLEVÉE.
 *
 * SORTIE (115200) : t=.. | D11=0V A1=x.xxV | spd=y.y mph | trame=...
 *   -> on veut voir A1 descendre SOUS ~1.35 V et spd tomber à 0.
 *   Si A1 reste haut malgré D11=0V => le biais est trop fort pour 1 kΩ : il faudra
 *   couper le gris/blanc / réduire la résistance d'injection (voir explications).
 */

#include <SoftwareSerial.h>

#define RX_PIN    10
#define TX_DUMMY  8
#define PWM_PIN   11
#define PIN_THR   A1
#define BUS_BAUD  9600
#define VREF      5.0
#define SPEED_K   2520.0   // recalibre 2026-06-03 (ex-667 faux)
#define PRINT_MS  300

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);
uint8_t lastFrame[32]; uint8_t lastLen = 0;
unsigned long lastPrint = 0;

void killThrottle() {        // tire le nœud throttle au plus bas que l'Uno permet
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(PWM_PIN, LOW);   // 0 V franc (sink actif via la R 1k)
}

void readBus() {
  static uint8_t buf[32]; static uint8_t idx = 0, len = 0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read();
    if (idx == 0)      { if (b == 0x02) buf[idx++] = b; }
    else if (idx == 1) { if (b >= 4 && b <= 32) { buf[idx++] = b; len = b; } else idx = 0; }
    else {
      buf[idx++] = b;
      if (idx >= len) {
        uint8_t x = 0; for (uint8_t i = 0; i < len - 1; i++) x ^= buf[i];
        if (x == buf[len - 1]) { memcpy(lastFrame, buf, len); lastLen = len; }
        idx = 0; len = 0;
      }
    }
  }
}

void setup() {
  killThrottle();              // AVANT tout le reste : throttle à 0 V immédiatement
  Serial.begin(115200);
  busSerial.begin(BUS_BAUD);
  Serial.println(F("\n=== THROTTLE KILL === D11 force a 0V (pull-down throttle max). ROUE EN L'AIR."));
}

void loop() {
  killThrottle();              // ré-assure 0 V en continu
  readBus();

  unsigned long now = millis();
  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    int thr = analogRead(PIN_THR); float thrv = thr * VREF / 1023.0;
    float mph = 0;
    if (lastLen >= 13 && lastFrame[8] != 0xEA) {
      uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9];
      if (per >= 30) mph = SPEED_K / (float)per;
    }
    Serial.print(F("t=")); Serial.print(now);
    Serial.print(F(" | D11=0V A1=")); Serial.print(thrv, 2); Serial.print('V');
    Serial.print(F(" | spd=")); Serial.print(mph, 1); Serial.print(F("mph"));
    Serial.print(F(" | trame="));
    if (lastLen) { for (uint8_t i = 0; i < lastLen; i++) { if (lastFrame[i] < 0x10) Serial.print('0'); Serial.print(lastFrame[i], HEX); Serial.print(' '); } }
    else Serial.print(F("(aucune)"));
    Serial.println();
  }
}
