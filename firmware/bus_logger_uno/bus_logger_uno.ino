/*
 * BUS LOGGER COMBINÉ — sniff UART (bleu) + ADC frein (violet) + ADC throttle (gris/blanc)
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : capturer EN MÊME TEMPS, sur une seule base de temps, les 3 signaux du
 *       connecteur DASHBOARD 6-pin, pour corréler frein/throttle avec les trames UART.
 *
 * CABLAGE (lecture seule, config NORMALE — on ne coupe rien, taps en parallèle) :
 *   D10 ── fil BLEU        (UART 9600, tap // — NE PAS couper)
 *   A0  ── fil VIOLET      (frein, analogique)
 *   A1  ── fil GRIS/BLANC  (throttle, analogique)
 *   GND ── fil NOIR        (masse commune — indispensable)
 *   D11 ── (non connecté : TX factice imposé par SoftwareSerial)
 *   ☠️ RIEN sur ROUGE / ORANGÉ (52 V). ADC Uno = 0–5 V (violet/throttle ≤5 V : OK direct).
 *
 * SORTIE (115200) :
 *   t=12345 | thr=0.82V(168) | brk=4.97V(1018) | spd=0.0mph | FREIN(uart)=NON | trame= 02 0E ...
 *
 * Actionne le frein PROGRESSIVEMENT (puis le throttle) et observe :
 *   - brk = tension du fil violet  -> analogique proportionnel ? ou saute (tout-ou-rien) ?
 *   - FREIN(uart) -> le flag B4=0xA0 dans la télémétrie
 *   - thr / spd pour le contexte
 */

#include <SoftwareSerial.h>

#define RX_PIN     10        // fil bleu (UART)
#define TX_DUMMY   11        // non connecté
#define BUS_BAUD   9600
#define PIN_BRAKE  A0        // fil violet
#define PIN_THR    A1        // fil gris/blanc
#define VREF       5.0       // ajuster si Vcc mesuré != 5.00 V
#define PRINT_MS   150       // cadence d'affichage

#define SPEED_K    2520.0    // mph = SPEED_K / B8B9 (recalibre 2026-06-03 ; ex-667 faux, cf. docs/02)

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);

uint8_t lastFrame[32]; uint8_t lastLen = 0;
unsigned long lastPrint = 0;

// min/max des ADC entre deux affichages (pour attraper une pression brève)
int brkMin = 1023, brkMax = 0, thrMin = 1023, thrMax = 0;

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
        if (x == buf[len - 1]) { memcpy(lastFrame, buf, len); lastLen = len; }  // trame valide
        idx = 0; len = 0;
      }
    }
  }
}

void sampleADC() {
  int b = analogRead(PIN_BRAKE);
  int t = analogRead(PIN_THR);
  if (b < brkMin) brkMin = b; if (b > brkMax) brkMax = b;
  if (t < thrMin) thrMin = t; if (t > thrMax) thrMax = t;
}

void setup() {
  Serial.begin(115200);
  busSerial.begin(BUS_BAUD);
  delay(800);
  Serial.println(F("\n=== BUS LOGGER (bleu UART + violet/frein + gris/throttle) ==="));
  Serial.println(F("Actionne le frein puis le throttle progressivement. Ctrl pour lire."));
}

void loop() {
  readBus();          // priorité : ne pas rater d'octets
  sampleADC();        // échantillonnage analogique (rapide)

  unsigned long now = millis();
  if (now - lastPrint >= PRINT_MS) {
    lastPrint = now;

    // valeurs courantes (dernier échantillon) + on affiche aussi min/max de la fenêtre
    int braw = analogRead(PIN_BRAKE);
    int traw = analogRead(PIN_THR);
    float bv = braw * VREF / 1023.0;
    float tv = traw * VREF / 1023.0;

    // décodage rapide depuis la dernière trame valide
    float mph = 0; bool brakeFlag = false;
    if (lastLen >= 13) {
      brakeFlag = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) {
        uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9];
        if (per >= 30) mph = SPEED_K / (float)per;
      }
    }

    Serial.print(F("t=")); Serial.print(now);
    Serial.print(F(" | thr=")); Serial.print(tv, 2); Serial.print('('); Serial.print(traw); Serial.print(')');
    Serial.print(F(" | brk=")); Serial.print(bv, 2); Serial.print('('); Serial.print(braw); Serial.print(')');
    Serial.print(F(" brkMinMax=")); Serial.print(brkMin); Serial.print('-'); Serial.print(brkMax);
    Serial.print(F(" | spd=")); Serial.print(mph, 1); Serial.print(F("mph"));
    Serial.print(F(" | FREIN(uart)=")); Serial.print(brakeFlag ? F("OUI") : F("NON"));
    Serial.print(F(" | trame="));
    if (lastLen) { for (uint8_t i = 0; i < lastLen; i++) { if (lastFrame[i] < 0x10) Serial.print('0'); Serial.print(lastFrame[i], HEX); Serial.print(' '); } }
    else Serial.print(F("(aucune)"));
    Serial.println();

    // reset fenêtre min/max
    brkMin = thrMin = 1023; brkMax = thrMax = 0;
  }
}
