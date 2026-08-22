/*
 * STM32 F401RE — SNIFF du fil VIOLET comme LIAISON SERIE
 *
 * Hypothèse (mesure 2026-06-04) : le violet n'est pas un PWM mais une ligne série numérique
 *   ~9600 bauds (impulsion mini ~104 µs = 1 bit @9600 ; repos-haut ; contenu varie avec le frein).
 *   On le lit donc en UART.
 *
 * CABLAGE (écoute seule, AUCUNE alimentation) :
 *   PA9  (= D8)  <- fil BLEU   : bus télémétrie half-duplex HW (contexte vitesse/frein). GND commun.
 *   PC7  (= D9)  <- fil VIOLET : USART6 RX, 5V-tolérant. (DEPLACER le violet de PB3 vers PC7/D9)
 *   GND  <- fil NOIR.
 *
 * COMMANDES (Serial 115200) :
 *   b<baud>   change le baud du violet (ex. b19200, b4800) et réinitialise
 *
 * SORTIE : à chaque rafale du violet -> dump hex. + 1x/s : spd, brk, nb octets/s.
 */

HardwareSerial busSerial(PA9);          // bus bleu (half-duplex)
HardwareSerial violetSerial(PC7, PC6);  // USART6 : RX=PC7 (violet), TX=PC6 (non câblé)

#define SPEED_K  2520.0
uint32_t vbaud = 9600;

// ---- bus (contexte vitesse/frein) ----
uint8_t lastFrame[32]; uint8_t lastLen = 0;
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

// ---- violet (dump brut, regroupé par rafales) ----
uint8_t vbuf[64]; uint8_t vidx = 0;
unsigned long vlastByte = 0, bytesAccum = 0;

void flushViolet() {
  if (!vidx) return;
  Serial.print("[VIO] ");
  for (uint8_t i = 0; i < vidx; i++) { if (vbuf[i] < 0x10) Serial.print('0'); Serial.print(vbuf[i], HEX); Serial.print(' '); }
  Serial.println();
  vidx = 0;
}

void setup() {
  Serial.begin(115200);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  violetSerial.begin(vbaud);   // 8N1 par défaut
  delay(300);
  Serial.print("\n=== VIOLET SNIFF (PC7/D9 @"); Serial.print(vbaud); Serial.println(" 8N1) === b<baud> pour changer");
}

unsigned long lastPrint = 0;
void loop() {
  readBus();

  // lire le violet : regrouper les octets, flush apres un silence (>3ms = fin de rafale)
  while (violetSerial.available()) {
    uint8_t b = violetSerial.read();
    vlastByte = millis(); bytesAccum++;
    if (vidx < sizeof(vbuf)) vbuf[vidx++] = b;
    else flushViolet();
  }
  if (vidx && millis() - vlastByte > 3) flushViolet();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); s.trim();
    if (s.length() && s[0] == 'b') {
      vbaud = s.substring(1).toInt(); if (vbaud < 300) vbaud = 9600;
      violetSerial.end(); violetSerial.begin(vbaud); vidx = 0;
      Serial.print("[baud] violet @ "); Serial.println(vbaud);
    }
  }

  unsigned long now = millis();
  if (now - lastPrint >= 1000) {
    lastPrint = now;
    unsigned long bps = bytesAccum; bytesAccum = 0;
    float mph = 0; bool brake = false;
    if (lastLen >= 13) { brake = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) { uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9]; if (per >= 10) mph = SPEED_K / (float)per; } }
    Serial.print("--- spd="); Serial.print(mph,1); Serial.print(" brk="); Serial.print(brake?"O":"N");
    Serial.print(" | violet="); Serial.print(bps); Serial.print(" octets/s @"); Serial.print(vbaud);
    if (bps == 0) Serial.print("  [rien : violet muet ou baud faux]");
    Serial.println();
  }
}
