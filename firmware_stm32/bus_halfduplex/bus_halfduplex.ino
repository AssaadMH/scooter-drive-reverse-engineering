/*
 * STM32 F401RE — SNIFF + INJECTION sur le bus scooter (fil bleu, 9600 8N1)
 * Bus  : PA9 (= D8 sur le Nucleo), UART1 en HALF-DUPLEX SINGLE-WIRE MATERIEL. GND commun.
 * Console PC : Serial (VCP /dev/ttyACM0 @115200).
 *
 * Avantage vs Uno : le HW gere TX(open-drain)+RX sur LA MEME broche, timing precis.
 *
 * COMMANDES (115200) :
 *   t -> emet UNE fois la trame candidate S/D dans le prochain silence
 *   p -> affiche la trame candidate
 * Regarde l'afficheur : S-drive <-> D-drive ?
 */

HardwareSerial busSerial(PA9);   // single-wire sur PA9 (USART1)

#define SPEED_K  2520.0
#define GAP_MS   8

uint8_t CANDIDATE[14] = {0x02,0x0E,0x01,0x20,0x80,0x00,0x00,0x00,0xEA,0x60,0x00,0x00,0x00,0x27};
uint8_t lastFrame[32]; uint8_t lastLen = 0;
unsigned long lastRxByte = 0, lastPrint = 0;
bool txPending = false;

void readBus() {
  static uint8_t buf[32]; static uint8_t idx = 0, len = 0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read(); lastRxByte = millis();
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

void sendCandidate() {
  busSerial.write(CANDIDATE, 14);   // half-duplex : passe en TX
  busSerial.flush();                // attendre fin emission
  busSerial.enableHalfDuplexRx();   // repasser en ECOUTE
  Serial.println("[TX] trame S/D emise. >>> REGARDE L'AFFICHEUR");
}

void setup() {
  Serial.begin(115200);
  busSerial.setHalfDuplex();        // AVANT begin : mode single-wire
  busSerial.begin(9600);
  busSerial.enableHalfDuplexRx();   // demarrer en ecoute
  delay(500);
  Serial.println("\n=== STM32 BUS half-duplex (PA9/D8) === 't'=emettre S/D | 'p'=voir trame");
}

void loop() {
  readBus();

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't') { txPending = true; Serial.println("[t] arme : emission au prochain silence..."); }
    else if (c == 'g') {  // FLOOD de bruit ~1s : doit perturber l'afficheur SI le TX atteint le bus
      Serial.println("[g] FLOOD bruit ~1s -> REGARDE L'AFFICHEUR (erreur/E-06/fige ?)");
      unsigned long t0=millis();
      while (millis()-t0 < 1000) { busSerial.write(0xFF); busSerial.write(0x00); busSerial.write(0x55); }
      busSerial.flush(); busSerial.enableHalfDuplexRx();
      Serial.println("[g] fini.");
    }
    else if (c == 'g') {  // FLOOD ~3s : doit perturber l'afficheur SI le TX atteint le bus
      Serial.println("[g] FLOOD ~3s -> REGARDE L'AFFICHEUR (E-06 / blanc / fige ?)");
      unsigned long t0=millis();
      while (millis()-t0 < 3000) { busSerial.write(0xFF); busSerial.write(0x00); busSerial.write(0x55); }
      busSerial.flush(); busSerial.enableHalfDuplexRx();
      Serial.println("[g] fini.");
    }
    else if (c == 'p') { Serial.print("candidate: "); for (uint8_t i=0;i<14;i++){ if(CANDIDATE[i]<0x10)Serial.print('0'); Serial.print(CANDIDATE[i],HEX); Serial.print(' '); } Serial.println(); }
  }

  unsigned long now = millis();
  if (txPending && (now - lastRxByte > GAP_MS)) { sendCandidate(); txPending = false; lastRxByte = now; }

  if (now - lastPrint >= 1000) {
    lastPrint = now;
    float mph = 0; bool brake = false;
    if (lastLen >= 13) { brake = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) { uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9]; if (per >= 10) mph = SPEED_K / (float)per; } }
    Serial.print("[rx] spd="); Serial.print(mph,1); Serial.print(" brk="); Serial.print(brake?"OUI":"NON"); Serial.print(" | ");
    if (lastLen) { for (uint8_t i=0;i<lastLen;i++){ if(lastFrame[i]<0x10)Serial.print('0'); Serial.print(lastFrame[i],HEX); Serial.print(' '); } }
    else Serial.print("(aucune trame)");
    Serial.println();
  }
}
