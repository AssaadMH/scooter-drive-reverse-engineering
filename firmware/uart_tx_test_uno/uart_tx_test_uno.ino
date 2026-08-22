/*
 * UART TX TEST — émettre une commande sur le bus bleu (half-duplex) pour piloter le mode S/D
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : tester si en EMETTANT la trame candidate observée lors du basculement mono/dual
 *       (S-drive/D-drive) on fait basculer le mode -> confirmerait que le bus est
 *       bidirectionnel et que l'afficheur commande le contrôleur par l'UART.
 *
 * /!\ TX EN HAUTE IMPEDANCE AU REPOS : D9 reste en INPUT (hi-Z) tant qu'on n'émet pas,
 *     pour NE PAS parasiter la réception. On le pilote (bit-bang 9600) UNIQUEMENT pendant
 *     l'émission, puis on le relâche. La lecture (RX) se fait via SoftwareSerial sur D10.
 *
 * CABLAGE
 *   D10 ─────────── fil BLEU         (RX, lecture télémétrie)
 *   D9  ──[R 470Ω]─ fil BLEU         (TX bit-bang, hi-Z au repos)
 *   GND ─────────── fil NOIR         (masse commune)
 *   D8  = TX factice SoftwareSerial (non connecté)
 *   D11 = forcé à 0 V (throttle OFF) -> la roue NE bouge PAS.
 *   ☠️ rien sur rouge/orangé (52 V).
 *
 * COMMANDES (115200) :
 *   t  -> émet UNE fois la trame candidate S/D dans le prochain silence
 *   p  -> affiche la trame candidate
 *
 * Regarde l'AFFICHEUR : s'il passe S-drive <-> D-drive après 't', c'est gagné.
 */

#include <SoftwareSerial.h>

#define RX_PIN    10
#define DUMMY_TX  8         // TX factice SoftwareSerial (NON connecté)
#define TXBB_PIN  9         // TX bit-bang vers le bleu (hi-Z au repos)
#define PWM_PIN   11        // forcé bas = throttle OFF
#define BUS_BAUD  9600
#define BIT_US    100       // ~1 bit a 9600 (104us - overhead digitalWrite). Ajustable si TX corrompu.
#define GAP_MS    8         // silence min (ms) avant d'oser émettre

SoftwareSerial busSerial(RX_PIN, DUMMY_TX);

// trame candidate observée lors du basculement S/D (checksum 0x27 OK)
uint8_t CANDIDATE[14] = {0x02,0x0E,0x01,0x20,0x80,0x00,0x00,0x00,0xEA,0x60,0x00,0x00,0x00,0x27};

uint8_t lastFrame[32]; uint8_t lastLen = 0;
unsigned long lastRxByte = 0, lastPrint = 0;
bool txPending = false;

void readBus() {
  static uint8_t buf[32]; static uint8_t idx = 0, len = 0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read();
    lastRxByte = millis();
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

// bit-bang d'un octet 9600 8N1 sur TXBB_PIN (déjà en OUTPUT, niveau repos = HIGH)
void bbByte(uint8_t b) {
  digitalWrite(TXBB_PIN, LOW); delayMicroseconds(BIT_US);            // start
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(TXBB_PIN, (b & 1) ? HIGH : LOW); b >>= 1;
    delayMicroseconds(BIT_US);
  }
  digitalWrite(TXBB_PIN, HIGH); delayMicroseconds(BIT_US);           // stop
}

void sendCandidate() {
  // prendre la ligne, émettre, puis RELÂCHER (hi-Z) -> ne parasite pas la lecture
  noInterrupts();
  pinMode(TXBB_PIN, OUTPUT); digitalWrite(TXBB_PIN, HIGH);           // repos ligne avant start
  delayMicroseconds(BIT_US);
  for (uint8_t i = 0; i < 14; i++) bbByte(CANDIDATE[i]);
  pinMode(TXBB_PIN, INPUT);                                          // relâche -> hi-Z
  interrupts();
  Serial.println(F("[TX] trame S/D emise. >>> REGARDE L'AFFICHEUR (S-drive <-> D-drive ?)"));
}

void setup() {
  pinMode(PWM_PIN, OUTPUT); digitalWrite(PWM_PIN, LOW);   // throttle OFF (roue immobile)
  pinMode(TXBB_PIN, INPUT);                               // TX en hi-Z au repos (NE parasite pas le bus)
  Serial.begin(115200);
  busSerial.begin(BUS_BAUD);
  delay(800);
  Serial.println(F("\n=== UART TX TEST v2 (TX hi-Z au repos) ==="));
  Serial.println(F("'t' = emettre la trame S/D dans le prochain silence | 'p' = voir la trame. ROUE EN L'AIR."));
}

void loop() {
  readBus();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') { txPending = true; Serial.println(F("[t] arme : emission au prochain silence...")); }
    else if (c == 'p') { Serial.print(F("candidate: ")); for (uint8_t i=0;i<14;i++){ if(CANDIDATE[i]<0x10)Serial.print('0'); Serial.print(CANDIDATE[i],HEX); Serial.print(' '); } Serial.println(); }
  }

  unsigned long now = millis();

  if (txPending && (now - lastRxByte > GAP_MS)) {
    sendCandidate();
    txPending = false;
    lastRxByte = now;
  }

  if (now - lastPrint >= 1000) {
    lastPrint = now;
    Serial.print(F("[rx] "));
    if (lastLen) { for (uint8_t i = 0; i < lastLen; i++) { if (lastFrame[i] < 0x10) Serial.print('0'); Serial.print(lastFrame[i], HEX); Serial.print(' '); } }
    else Serial.print(F("(aucune trame)"));
    Serial.println();
  }
}
