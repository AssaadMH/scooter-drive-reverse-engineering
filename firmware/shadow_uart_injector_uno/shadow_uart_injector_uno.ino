/*
 * SHADOW / NAVERA — Injecteur UART (emule le guidon)
 * Cible  : Arduino Uno R3 (ATmega328P, logique 5V)
 * Scooter: Ecoxtrem M41 Tank Dual  /  ESC CHK2-K1-03
 *
 * MODE B : sequence AUTOMATIQUE
 *   neutre -> rampe d'acceleration douce -> maintien -> retour neutre -> frein -> neutre
 *   La sequence joue une fois au boot (apres un compte a rebours), puis l'Uno
 *   reste au NEUTRE. Envoyer 'g' dans le moniteur serie pour rejouer la sequence.
 *
 * PROTOCOLE (decode par sniff, trame de 14 octets, 9600 8N1) :
 *   02 0E 01 00 [F] 00 [F] [N] [aH aL] [A] 00 [F] [XOR]
 *   B0=STX 02 | B1=longueur 0E(=14) | B2..B3=entete 01 00
 *   B4/B6/B12 = drapeau FREIN : 80=relache, A0=freine
 *   B7        = cran/intensite accelerateur (0 au repos, 1..4 en poussee)
 *   B8..B9    = position : EA 60 = neutre ; sinon 00 [pos] avec pos ~125..161
 *   B10       = bit "accelerateur actif" (0/1)
 *   B11       = 00 reserve
 *   B13       = checksum = XOR(B0..B12)
 *
 * CABLAGE (DIFFERENT du sniffer !)
 *  - Debrancher le VRAI guidon : l'Uno prend sa place (sinon collisions sur le bus).
 *  - D11 (TX logiciel) -> ligne data / entree RX du controleur
 *  - GND Uno <-> GND scooter  (masse commune OBLIGATOIRE)
 *
 * /!\ SECURITE
 *  - NE JAMAIS relier la batterie 52V a l'Uno. On ne touche que data + GND.
 *  - ROUE SURELEVEE / scooter sur bequille : ce sketch fait REELLEMENT tourner le moteur.
 *  - L'Uno emet en 5V. Si le bus du controleur est en 3.3V, intercaler un adaptateur
 *    de niveau (diviseur ou level shifter) sur D11 pour ne pas stresser son entree.
 *  - Garder l'arret d'urgence (coupure alim) a portee de main.
 *  - MAX_POS est volontairement bas : augmenter PROGRESSIVEMENT apres calibration.
 */

#include <SoftwareSerial.h>

#define RX_DUMMY      10     // non utilise en emission (SoftwareSerial exige un RX)
#define TX_PIN        11     // TX logiciel -> entree data du controleur
#define BUS_BAUD      9600   // baud du bus (confirme par le sniff)

#define FRAME_MS      100    // cadence d'emission (~ce qu'emet le vrai guidon)
#define NEUTRAL_POS   0x80   // position de repos de l'accelerateur (centre ~128)
#define MAX_POS       0x96   // <<< CALIBRER : pic d'accel (150). Pousse a fond pour le vrai max.
#define ACCEL_LEVEL   0x02   // B7 : cran "accelerateur engage" (valeur dominante observee)
#define STARTUP_DELAY 5000   // compte a rebours avant la 1ere sequence (ms)

SoftwareSerial busSerial(RX_DUMMY, TX_PIN);

// Construit une trame de 14 octets et calcule le checksum XOR en B13.
void buildFrame(uint8_t f[14], bool brake, uint8_t level, uint8_t posHi, uint8_t posLo, uint8_t active) {
  uint8_t flag = brake ? 0xA0 : 0x80;
  f[0] = 0x02; f[1] = 0x0E; f[2] = 0x01; f[3] = 0x00;
  f[4] = flag; f[5] = 0x00; f[6] = flag;
  f[7] = level;
  f[8] = posHi; f[9] = posLo;
  f[10] = active; f[11] = 0x00; f[12] = flag;
  uint8_t x = 0;
  for (int i = 0; i < 13; i++) x ^= f[i];
  f[13] = x;
}

void sendFrame(const uint8_t f[14]) {
  busSerial.write(f, 14);
}

// Emet une trame en boucle pendant 'durationMs' (maintient le watchdog du controleur).
void hold(const uint8_t f[14], unsigned long durationMs) {
  unsigned long t0 = millis();
  while (millis() - t0 < durationMs) {
    sendFrame(f);
    delay(FRAME_MS);
  }
}

void neutralFrame(uint8_t f[14])      { buildFrame(f, false, 0x00, 0xEA, 0x60, 0x00); }      // repos
void brakeFrame(uint8_t f[14])        { buildFrame(f, true,  0x00, 0xEA, 0x60, 0x00); }      // frein
void accelFrame(uint8_t f[14], uint8_t pos) { buildFrame(f, false, ACCEL_LEVEL, 0x00, pos, 0x01); } // accel a 'pos'

uint8_t neutral[14], brake[14], frame[14];

void runSequence() {
  Serial.println(F(">>> SEQUENCE : accel douce -> frein"));

  // 1) Neutre 1s
  Serial.println(F("NEUTRE"));
  hold(neutral, 1000);

  // 2) Rampe montante NEUTRAL_POS -> MAX_POS
  for (uint8_t pos = NEUTRAL_POS; pos <= MAX_POS; pos++) {
    accelFrame(frame, pos);
    Serial.print(F("ACCEL pos=")); Serial.println(pos);
    hold(frame, 200);   // ~2 trames par palier
  }

  // 3) Maintien du pic 1.5s
  accelFrame(frame, MAX_POS);
  Serial.println(F("MAINTIEN pic"));
  hold(frame, 1500);

  // 4) Rampe descendante MAX_POS -> NEUTRAL_POS
  for (uint8_t pos = MAX_POS; pos > NEUTRAL_POS; pos--) {
    accelFrame(frame, pos);
    Serial.print(F("DECEL pos=")); Serial.println(pos);
    hold(frame, 200);
  }

  // 5) Neutre 1s
  Serial.println(F("NEUTRE"));
  hold(neutral, 1000);

  // 6) Frein 2s
  Serial.println(F("FREIN"));
  hold(brake, 2000);

  // 7) Retour neutre
  Serial.println(F("NEUTRE (fin de sequence)"));
  hold(neutral, 500);
  Serial.println(F(">>> Sequence terminee. Envoyer 'g' pour rejouer.\n"));
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println(F("\n=== SHADOW UART INJECTOR (Uno R3) — MODE B (auto) ==="));
  Serial.println(F("/!\\ ROUE SURELEVEE. D11 -> entree data controleur, GND commun, guidon debranche."));

  busSerial.begin(BUS_BAUD);
  neutralFrame(neutral);
  brakeFrame(brake);

  // Compte a rebours en maintenant le NEUTRE sur le bus
  for (int s = STARTUP_DELAY / 1000; s > 0; s--) {
    Serial.print(F("Demarrage sequence dans ")); Serial.print(s); Serial.println(F(" s..."));
    hold(neutral, 1000);
  }

  runSequence();
}

void loop() {
  // Hors sequence : on maintient le NEUTRE en continu (watchdog controleur)
  sendFrame(neutral);
  delay(FRAME_MS);

  // 'g' = rejouer la sequence
  if (Serial.available() && Serial.read() == 'g') {
    runSequence();
  }
}
