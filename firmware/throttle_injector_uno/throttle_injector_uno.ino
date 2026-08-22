/*
 * THROTTLE INJECTOR — pilote l'accelerateur ANALOGIQUE du controleur
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BROCHAGE confirme du connecteur DASHBOARD 6-pin (mesures multimetre) :
 *   rouge      = +52V batterie              ☠️ JAMAIS sur l'Arduino
 *   orange     = +52V commute (power/enable) ☠️ JAMAIS sur l'Arduino
 *   noir       = GND                         -> reference commune Arduino
 *   bleu       = comm UART afficheur<->ctrl  (E-06 si coupe) -> laisser branche
 *   gris/blanc = THROTTLE signal (0.8V repos -> 3.2V @20km/h) <-- ON PILOTE CELUI-CI
 *   violet     = frein / signal variable 3-5V -> laisser branche
 *
 * MONTAGE : D11 --[ R 1k ]--+-- fil gris/blanc (cote CONTROLEUR) ; +--[ C 10uF ]-- GND
 *           GND Arduino <-> fil noir. Couper le gris/blanc, injecter cote controleur.
 *           ✅ Pilotage confirme fonctionnel (roue tourne, afficheur affiche la vitesse).
 *
 * /!\ SECURITE : ROUE SURELEVEE. Jamais rouge/orange (52V). 'x' = ARRET d'urgence.
 *
 * COMMANDES (moniteur serie 115200) :
 *   nombre, ex "1.5"  -> rampe douce vers 1.5 V (annule un scenario en cours)
 *   u / d             -> +0.1 / -0.1 V
 *   n                 -> repos (0.8 V)
 *   x                 -> ARRET d'urgence (repos immediat)
 *   ?                 -> etat courant
 *   s1                -> SCENARIO rampe douce  (accel -> maintien -> repos)
 *   s2                -> SCENARIO paliers      (1.0 / 1.4 / 1.8 V)
 *   s3                -> SCENARIO cycle conduite (accel/decel repetes)
 */

#define PWM_PIN    11
#define VCC        5.0
#define IDLE_V     0.80     // tension repos throttle
#define MAX_V      2.00     // <<< plafond de securite (relever progressivement vers 3.2V)
#define ABS_MAX_V  3.40     // garde-fou DUR
#define CAL        1.00     // calibration = V_mesure_au_fil / V_commande (ajuster apres mesure)
#define RAMP_VPS   0.50     // vitesse de rampe (V/s)

float targetV = IDLE_V;
float curV    = IDLE_V;
unsigned long lastStep = 0;

// --- moteur de scenarios ---
struct Step { float v; unsigned long holdMs; };
Step SCN_RAMP[]  = {{1.6, 3000}, {IDLE_V, 0}};
Step SCN_STEPS[] = {{1.0, 2000}, {1.4, 2000}, {1.8, 2000}, {IDLE_V, 0}};
Step SCN_DRIVE[] = {{1.6, 2500}, {1.0, 1500}, {1.9, 2500}, {1.2, 1500}, {IDLE_V, 0}};

Step* scn = 0;
int scnLen = 0, scnIdx = 0;
bool holding = false;
unsigned long holdStart = 0;

void applyVoltage(float v) {
  if (v < 0) v = 0;
  if (v > ABS_MAX_V) v = ABS_MAX_V;
  int duty = (int)((v / CAL) / VCC * 255.0 + 0.5);
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;
  analogWrite(PWM_PIN, duty);
}

void setTarget(float v) {
  if (v < IDLE_V) v = IDLE_V;
  if (v > MAX_V)  v = MAX_V;
  targetV = v;
  Serial.print(F("Cible = ")); Serial.print(targetV, 2); Serial.println(F(" V"));
}

void startScenario(Step* s, int len, const __FlashStringHelper* name) {
  scn = s; scnLen = len; scnIdx = 0; holding = false;
  Serial.print(F(">>> SCENARIO: ")); Serial.println(name);
  setTarget(scn[0].v);
}

void stopScenario(const __FlashStringHelper* why) {
  scn = 0; curV = targetV = IDLE_V; applyVoltage(IDLE_V);
  Serial.print(F("*** ")); Serial.print(why); Serial.println(F(" -> repos ***"));
}

void setup() {
  Serial.begin(115200);
  pinMode(PWM_PIN, OUTPUT);
  applyVoltage(IDLE_V);
  delay(1000);
  Serial.println(F("\n=== THROTTLE INJECTOR (Uno) — manuel + scenarios ==="));
  Serial.print(F("Repos ")); Serial.print(IDLE_V, 2);
  Serial.print(F("V | MAX_V ")); Serial.print(MAX_V, 2); Serial.println(F("V"));
  Serial.println(F("nombre=cible | u/d | n=repos | x=ARRET | s1/s2/s3=scenarios | ?=etat"));
}

void loop() {
  // --- commandes serie ---
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if      (s == "x")  stopScenario(F("ARRET"));
    else if (s == "s1") startScenario(SCN_RAMP,  sizeof(SCN_RAMP)/sizeof(Step),  F("rampe douce"));
    else if (s == "s2") startScenario(SCN_STEPS, sizeof(SCN_STEPS)/sizeof(Step), F("paliers"));
    else if (s == "s3") startScenario(SCN_DRIVE, sizeof(SCN_DRIVE)/sizeof(Step), F("cycle conduite"));
    else if (s == "n")  { scn = 0; setTarget(IDLE_V); }
    else if (s == "u")  { scn = 0; setTarget(targetV + 0.1); }
    else if (s == "d")  { scn = 0; setTarget(targetV - 0.1); }
    else if (s == "?")  {
      Serial.print(F("cur=")); Serial.print(curV, 2);
      Serial.print(F("V cible=")); Serial.print(targetV, 2);
      Serial.print(F("V scenario=")); Serial.println(scn ? F("oui") : F("non"));
    }
    else if (s.length()) { float v = s.toFloat(); if (v > 0) { scn = 0; setTarget(v); } }
  }

  // --- rampe douce non bloquante (50 Hz) ---
  unsigned long now = millis();
  if (now - lastStep >= 20) {
    float step = RAMP_VPS * 0.020;
    if      (curV < targetV) { curV += step; if (curV > targetV) curV = targetV; }
    else if (curV > targetV) { curV -= step; if (curV < targetV) curV = targetV; }
    applyVoltage(curV);
    lastStep = now;

    // --- avancement du scenario (pilote targetV palier par palier) ---
    if (scn) {
      float diff = curV - scn[scnIdx].v;
      if (diff < 0) diff = -diff;
      if (!holding) {
        if (diff < 0.02) { holding = true; holdStart = now; }   // palier atteint
      } else if (now - holdStart >= scn[scnIdx].holdMs) {
        scnIdx++;
        if (scnIdx >= scnLen) { scn = 0; Serial.println(F(">>> SCENARIO termine")); }
        else { holding = false; setTarget(scn[scnIdx].v); }
      }
    }
  }
}
