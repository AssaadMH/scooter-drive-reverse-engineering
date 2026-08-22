/*
 * SHADOW / NAVERA — Sniffer UART afficheur <-> controleur
 * Cible  : Arduino Uno R3 (ATmega328P, logique 5V)
 * Scooter: Ecoxtrem M41 Tank Dual  /  ESC CHK2-K1-03
 *
 * Etape 1 : detection auto du baudrate (pulseIn, largeur de bit mini)
 * Etape 2 : capture passive via SoftwareSerial + dump hexa, trames sur silence
 *
 * CABLAGE
 *  - L'Uno a UNE seule UART materielle, partagee avec l'USB -> on garde
 *    Serial (USB) pour le moniteur PC, et on sniffe sur SoftwareSerial.
 *  - D10  <- ligne data du scooter   (RX logiciel)
 *  - GND Uno <-> GND scooter         (masse commune OBLIGATOIRE)
 *
 * /!\ SECURITE
 *  - NE JAMAIS relier la batterie 52V a l'Uno. On ne touche que data + GND.
 *  - L'Uno est 5V et tolere 5V : lire une ligne data 3.3V OU 5V est sans risque.
 *  - En lecture seule (sniff passif) on ne peut pas abimer le bus du scooter.
 */

#include <SoftwareSerial.h>

#define DATA_PIN      10     // RX logiciel <- ligne data scooter
#define TX_DUMMY      11     // non utilise (SoftwareSerial exige un pin TX)
#define FRAME_GAP_MS  5      // silence (ms) qui delimite deux trames
#define DETECT_MS     3000   // duree d'observation pour estimer le baud

SoftwareSerial busSerial(DATA_PIN, TX_DUMMY);

const long BAUDS[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600};
const int  N_BAUDS = sizeof(BAUDS) / sizeof(BAUDS[0]);

long detectBaud() {
  pinMode(DATA_PIN, INPUT);
  unsigned long minPulse = 0xFFFFFFFFUL;
  unsigned long tStart   = millis();

  // On mesure la plus courte impulsion (= 1 bit) sur la ligne pendant DETECT_MS.
  // /!\ Plancher anti-bruit : une ligne qui flotte (ou des parasites moteur) genere des
  // glitches de quelques us qui faussaient la detection (ex. 4 us -> 250000 baud -> 57600 a tort).
  // Le bus de cette trottinette est CONFIRME a 9600 (1 bit ~104 us), donc on ignore tout
  // ce qui est < NOISE_FLOOR_US. (Pour un autre modele plus rapide, baisser ce plancher.)
  const unsigned long NOISE_FLOOR_US = 60;
  while (millis() - tStart < DETECT_MS) {
    unsigned long lo = pulseIn(DATA_PIN, LOW, 50000);   // us, timeout 50ms
    unsigned long hi = pulseIn(DATA_PIN, HIGH, 50000);
    if (lo > NOISE_FLOOR_US && lo < minPulse) minPulse = lo;
    if (hi > NOISE_FLOOR_US && hi < minPulse) minPulse = hi;
  }

  if (minPulse == 0xFFFFFFFFUL) return 0;       // aucune activite (que du bruit) -> repli appelant
  long est = 1000000L / (long)minPulse;         // baud estime = 1 / bit-time

  long best = BAUDS[0], bestDiff = labs(est - BAUDS[0]);
  for (int i = 1; i < N_BAUDS; i++) {
    long d = labs(est - BAUDS[i]);
    if (d < bestDiff) { bestDiff = d; best = BAUDS[i]; }
  }
  Serial.print(F("Impulsion min = ")); Serial.print(minPulse);
  Serial.print(F(" us  ->  baud estime ")); Serial.print(est);
  Serial.print(F("  ->  retenu ")); Serial.println(best);
  return best;
}

void setup() {
  Serial.begin(115200);            // moniteur USB vers le PC / Claude Code
  delay(1500);
  Serial.println(F("\n=== SHADOW UART SNIFFER (Uno R3) ==="));
  Serial.println(F("Detection du baudrate — laisse l'afficheur communiquer..."));

  long baud = detectBaud();
  if (baud == 0) {
    Serial.println(F("Aucune activite sur D10. Verifie cablage + masse commune."));
    baud = 9600;
    Serial.println(F("Repli sur 9600 baud."));
  }
  if (baud > 57600) {
    Serial.println(F("Attention: SoftwareSerial peu fiable au-dela de 57600 baud."));
  }

  busSerial.begin(baud);
  Serial.print(F("Capture a ")); Serial.print(baud);
  Serial.println(F(" baud (8N1). Une ligne = une trame.\n"));
}

void loop() {
  static unsigned long lastByte = 0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read();
    unsigned long now = millis();
    if (lastByte != 0 && now - lastByte > FRAME_GAP_MS) Serial.println();  // nouvelle trame
    if (b < 0x10) Serial.print('0');   // padding hexa
    Serial.print(b, HEX);
    Serial.print(' ');
    lastByte = now;
  }
}
