/*
 * BRAKE PROBE — caractériser le fil VIOLET (frein) — LECTURE SEULE, MOTEUR COUPÉ
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : comprendre ce que fait le fil violet quand on SERRE LE LEVIER DE FREIN, pour savoir
 *       si/comment l'injecter ensuite. On lit en parallèle : violet (A0), throttle (A1),
 *       vitesse + flag frein (UART). Le throttle est FORCÉ A 0 V -> la roue NE bouge PAS.
 *
 * /!\ SECURITE : D11 est forcé LOW (throttle 0). Pré-requis : pull-down 10k nœud->GND
 *     (fail-safe anti-emballement si un fil lâche).
 *
 * CABLAGE
 *   D10 ── fil BLEU        (UART 9600, RX)
 *   A0  ── fil VIOLET      (frein, à caractériser)
 *   A1  ── nœud GRIS/BLANC (throttle, relecture)
 *   GND ── fil NOIR
 *   D8  ── TX factice SoftwareSerial (NON connecté ; surtout PAS D11)
 *   D11 ── forcé 0 V (throttle OFF)
 *   ☠️ rien sur rouge/orangé (52 V).
 *
 * SORTIE (115200), toutes ~120 ms :
 *   t=.. | violet=x.xxV(raw min-max) | A1=x.xxV | spd=y.y | FREIN=OUI/NON | trame=...
 *   -> serre/relâche le levier et observe comment 'violet' bouge vs FREIN.
 */

#include <SoftwareSerial.h>

#define RX_PIN    10
#define DUMMY_TX  8         // NON connecté (jamais D11)
#define PWM_PIN   11        // forcé LOW = throttle OFF
#define PIN_VIO   A0        // fil violet
#define PIN_THR   A1        // nœud throttle
#define BUS_BAUD  9600
#define VREF      5.0
#define SPEED_K   2520.0
#define PRINT_MS  120

SoftwareSerial busSerial(RX_PIN, DUMMY_TX);
uint8_t lastFrame[32]; uint8_t lastLen = 0;
unsigned long lastPrint = 0;
int vioMin = 1023, vioMax = 0;

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
  pinMode(PWM_PIN, OUTPUT); digitalWrite(PWM_PIN, LOW);   // THROTTLE OFF (moteur coupé)
  Serial.begin(115200);
  busSerial.begin(BUS_BAUD);
  delay(800);
  Serial.println(F("\n=== BRAKE PROBE (lecture seule, throttle=0) ==="));
  Serial.println(F("Serre/relache le LEVIER de frein. Observe 'violet' vs FREIN. (roue immobile)"));
}

void loop() {
  readBus();
  int v = analogRead(PIN_VIO); if (v < vioMin) vioMin = v; if (v > vioMax) vioMax = v;

  unsigned long now = millis();
  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;
    // duty-cycle du violet : rafale rapide de lectures numériques (~%temps haut)
    uint16_t highs = 0; for (uint16_t i = 0; i < 400; i++) { if (digitalRead(PIN_VIO)) highs++; }
    int vioDuty = (int)((long)highs * 100 / 400);
    int vio = analogRead(PIN_VIO); float viov = vio * VREF / 1023.0;
    int thr = analogRead(PIN_THR); float thrv = thr * VREF / 1023.0;
    float mph = 0; bool brake = false;
    if (lastLen >= 13) {
      brake = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) { uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9]; if (per >= 10) mph = SPEED_K / (float)per; }
    }
    Serial.print(F("t=")); Serial.print(now);
    Serial.print(F(" | violet=")); Serial.print(viov, 2); Serial.print(F("V duty=")); Serial.print(vioDuty); Serial.print(F("% (")); Serial.print(vioMin); Serial.print('-'); Serial.print(vioMax); Serial.print(')');
    Serial.print(F(" | A1=")); Serial.print(thrv, 2); Serial.print('V');
    Serial.print(F(" | spd=")); Serial.print(mph, 1);
    Serial.print(F(" | FREIN=")); Serial.print(brake ? F("OUI") : F("NON"));
    Serial.print(F(" | trame="));
    if (lastLen) { for (uint8_t i = 0; i < lastLen; i++) { if (lastFrame[i] < 0x10) Serial.print('0'); Serial.print(lastFrame[i], HEX); Serial.print(' '); } }
    else Serial.print(F("(aucune)"));
    Serial.println();
    vioMin = 1023; vioMax = 0;
  }
}
