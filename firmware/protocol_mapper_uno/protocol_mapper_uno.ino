/*
 * PROTOCOL MAPPER — cartographie des commandes du tableau de bord (LECTURE SEULE)
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : repérer quel octet/bit de la trame UART bouge quand on presse chaque bouton du
 *       guidon (GEAR +/-, phare, clignotants, power) ou quand l'état change (gear, mode...).
 *       N'imprime QUE lorsqu'un octet d'ETAT change (ignore le bruit de la vitesse B8/B9),
 *       donc chaque appui ressort clairement.
 *
 * CABLAGE (lecture seule — l'afficheur d'origine reste branché et fonctionne) :
 *   D10 ── fil BLEU (UART 9600, tap // — NE PAS couper)
 *   D8  ── non connecté (TX factice SoftwareSerial)
 *   GND ── fil NOIR (masse commune)
 *   ☠️ rien sur rouge/orangé (52 V).
 *
 * SORTIE (115200) :
 *   [CHG] spd=0.0 brk=NON | B0=02 B1=0E B2=01 B3=20 B4=80 B5=00 B6=80 B7=03* B8=EA B9=60 B10=00 B11=00 B12=80 B13=..
 *   ('*' = octet d'etat qui vient de changer)  +  un battement [hb] toutes les 3 s.
 *
 * METHODE : presser UN bouton a la fois, noter ce qui change. Repeter pour chaque fonction.
 */

#include <SoftwareSerial.h>

#define RX_PIN    10
#define TX_DUMMY  8
#define BUS_BAUD  9600
#define SPEED_K   2520.0
#define BEAT_MS   3000

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);

uint8_t lastShown[32]; uint8_t shownLen = 0; bool haveShown = false;
unsigned long lastBeat = 0;

// octets d'ETAT a surveiller (on saute STX=0, LEN=1, vitesse=8/9, checksum=13)
bool watched(uint8_t i) {
  return (i == 2 || i == 3 || i == 4 || i == 5 || i == 6 || i == 7 || i == 10 || i == 11 || i == 12);
}

void printFrame(uint8_t* f, uint8_t len, const char* tag) {
  float mph = 0; bool brake = false;
  if (len >= 13) {
    brake = (f[4] == 0xA0);
    if (f[8] != 0xEA) { uint16_t per = ((uint16_t)f[8] << 8) | f[9]; if (per >= 10) mph = SPEED_K / (float)per; }
  }
  Serial.print(tag); Serial.print(F(" spd=")); Serial.print(mph, 1);
  Serial.print(F(" brk=")); Serial.print(brake ? F("OUI") : F("NON")); Serial.print(F(" | "));
  for (uint8_t i = 0; i < len; i++) {
    bool chg = haveShown && watched(i) && (i < shownLen) && (f[i] != lastShown[i]);
    Serial.print('B'); Serial.print(i); Serial.print('=');
    if (f[i] < 0x10) Serial.print('0'); Serial.print(f[i], HEX);
    if (chg) Serial.print('*');
    Serial.print(' ');
  }
  Serial.println();
}

void onFrame(uint8_t* f, uint8_t len) {
  bool changed = !haveShown;
  if (haveShown) {
    for (uint8_t i = 0; i < len && i < shownLen; i++)
      if (watched(i) && f[i] != lastShown[i]) { changed = true; break; }
  }
  if (changed) {
    printFrame(f, len, "[CHG]");
    memcpy(lastShown, f, len); shownLen = len; haveShown = true;
    lastBeat = millis();
  }
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
        if (x == buf[len - 1]) onFrame(buf, len);   // trame valide
        idx = 0; len = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  busSerial.begin(BUS_BAUD);
  delay(800);
  Serial.println(F("\n=== PROTOCOL MAPPER (lecture seule) ==="));
  Serial.println(F("Presse UN bouton a la fois (GEAR +/-, phare, clignotant, power...). '*' = octet change."));
}

void loop() {
  readBus();
  unsigned long now = millis();
  if (haveShown && now - lastBeat >= BEAT_MS) {
    lastBeat = now;
    printFrame(lastShown, shownLen, "[hb ]");
  }
}
