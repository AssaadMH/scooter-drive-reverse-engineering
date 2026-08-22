/*
 * STM32 F401RE — BOUCLE FERMEE PI de vitesse
 *   PA9 (D8)  : bus bleu half-duplex -> lecture vitesse (mph = 2520 / B8B9)
 *   PB10 (D6) : PWM throttle (3.3V) -> R1k -> noeud gris/blanc
 *   PA0 (A0)  : relecture tension noeud
 *   GND       : noir
 *
 * COMMANDES (115200) :
 *   c8        -> BOUCLE : tenir 8 mph (c0 / n = repos)
 *   un nombre -> MANUEL : throttle = X volts
 *   x         -> ARRET   ;   ? -> etat
 *
 * /!\ ROUE EN L'AIR. Feedforward ajuste pour l'attenuation 3.3V (noeud ~0.8 x commande).
 */

HardwareSerial busSerial(PA9);

#define PWM_PIN     PB10
#define VREF        3.3
#define IDLE_V      0.80
#define FF_BASE     1.40      // feedforward = FF_BASE + FF_K*consigne (vise le throttle de la vitesse cible)
#define FF_K        0.06
#define MAX_V       3.00
#define ABS_MAX     3.20
#define SPEED_K     2520.0
#define KP          0.02
#define KI          0.012
#define CTRL_MS     150
#define SLEW        0.08
#define INTEG_CLAMP 60.0
#define NPER        14
#define DEADBAND    0.2      // bande morte (mph) : dans cette zone on fige le throttle

enum { MANUAL, LOOP }; int mode = MANUAL;
float curV = IDLE_V, targetV = IDLE_V;
float setMph = 0, integ = 0, measMph = 0, measF = 0;
unsigned long lastCtrl = 0, lastTick = 0, lastPrint = 0, lastFrameMs = 0;
uint16_t pbuf[NPER]; uint8_t pcount = 0, ppos = 0;

void applyV(float v) {
  if (v < 0) v = 0; if (v > ABS_MAX) v = ABS_MAX;
  int d = (int)(v / VREF * 255.0 + 0.5); if (d < 0) d = 0; if (d > 255) d = 255;
  analogWrite(PWM_PIN, d);
}
uint16_t medianPeriod() {
  uint16_t t[NPER]; uint8_t n = pcount < NPER ? pcount : NPER;
  for (uint8_t i = 0; i < n; i++) t[i] = pbuf[i];
  for (uint8_t i = 1; i < n; i++) { uint16_t k = t[i]; int j = i - 1; while (j >= 0 && t[j] > k) { t[j+1] = t[j]; j--; } t[j+1] = k; }
  return n ? t[n/2] : 0;
}
void pushPeriod(uint16_t p) { pbuf[ppos] = p; ppos = (ppos+1) % NPER; if (pcount < NPER) pcount++; }

void readBus() {
  static uint8_t buf[32]; static uint8_t idx = 0, len = 0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read();
    if (idx == 0)      { if (b == 0x02) buf[idx++] = b; }
    else if (idx == 1) { if (b >= 4 && b <= 32) { buf[idx++] = b; len = b; } else idx = 0; }
    else {
      buf[idx++] = b;
      if (idx >= len) {
        uint8_t x = 0; for (uint8_t i = 0; i < len-1; i++) x ^= buf[i];
        if (x == buf[len-1] && len >= 10) {
          lastFrameMs = millis();
          if (buf[8] == 0xEA) { measMph = 0; pcount = 0; ppos = 0; }
          else { uint16_t per = ((uint16_t)buf[8]<<8) | buf[9];
                 if (per >= 10) { pushPeriod(per); uint16_t m = medianPeriod(); if (m) measMph = SPEED_K/(float)m; } }
          measF += 0.30f * (measMph - measF);   // EMA TOUJOURS a jour (meme en MANUAL) -> fix affichage fige
        }
        idx = 0; len = 0;
      }
    }
  }
}
void idleAll(const char* why) {
  mode = MANUAL; targetV = curV = IDLE_V; integ = 0; setMph = 0; applyV(IDLE_V);
  Serial.print("*** "); Serial.print(why); Serial.println(" -> repos ***");
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(PWM_PIN, OUTPUT); applyV(IDLE_V);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  delay(500);
  Serial.println("\n=== STM32 BOUCLE PI === c<mph>=tenir | nombre=manuel V | n=repos | x=ARRET | ?");
}

void loop() {
  readBus();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); s.trim();
    if (s == "x") idleAll("ARRET");
    else if (s == "n") idleAll("repos");
    else if (s == "?") {
      Serial.print("mode="); Serial.print(mode==LOOP?"LOOP":"MANUEL");
      Serial.print(" set="); Serial.print(setMph,1); Serial.print("mph meas="); Serial.print(measMph,1);
      Serial.print("mph thr="); Serial.print(curV,2); Serial.println("V");
    }
    else if (s.length() && (s[0]=='c' || s[0]=='C')) {
      float v = s.substring(1).toFloat();
      if (v <= 0) idleAll("cible 0");
      else { mode = LOOP; setMph = v; integ = 0; Serial.print(">>> BOUCLE: tenir "); Serial.print(setMph,1); Serial.println(" mph"); }
    }
    else if (s.length()) { float v = s.toFloat(); if (v > 0) { mode = MANUAL; targetV = (v<IDLE_V?IDLE_V:(v>MAX_V?MAX_V:v)); Serial.print("MANUEL cible="); Serial.print(targetV,2); Serial.println("V"); } }
  }

  unsigned long now = millis();
  if (mode == LOOP && now - lastFrameMs > 1500) idleAll("TELEMETRIE PERDUE");

  if (mode == LOOP && now - lastCtrl >= CTRL_MS) {
    float dt = (now - lastCtrl) / 1000.0; lastCtrl = now;
    float err = setMph - measF;
    float ae = err < 0 ? -err : err;
    if (ae >= DEADBAND) {                           // hors bande morte : on regule ; DANS la bande : throttle FIGE
      float out = FF_BASE + FF_K*setMph + KP*err + KI*integ;   // PI + feedforward proportionnel
      float outC = out;                             // butee absolue
      if (outC < IDLE_V) outC = IDLE_V; if (outC > MAX_V) outC = MAX_V;
      float outS = outC;                            // limite de slew
      if (outS > curV + SLEW) outS = curV + SLEW; else if (outS < curV - SLEW) outS = curV - SLEW;
      // ANTI-WINDUP propre : on n'integre QUE si la sortie n'a PAS ete limitee (ni butee ni slew)
      //  -> corrige l'erreur a TOUTES les vitesses, mais pas de windup pendant la rampe
      if (outS == out) {
        integ += err * dt;
        if (integ > INTEG_CLAMP) integ = INTEG_CLAMP; if (integ < -INTEG_CLAMP) integ = -INTEG_CLAMP;
      }
      curV = outS; applyV(curV);
    }
  }
  if (now - lastTick >= 20) {
    lastTick = now;
    if (mode == MANUAL) {
      float st = 0.5 * 0.020;
      if (curV < targetV) { curV += st; if (curV > targetV) curV = targetV; }
      else if (curV > targetV) { curV -= st; if (curV < targetV) curV = targetV; }
      applyV(curV);
    }
  }
  if (now - lastPrint >= 500) {
    lastPrint = now;
    float vnode = analogRead(PA0) * 3.3 / 4095.0;
    Serial.print(mode==LOOP?"LOOP set=":"MAN  set="); Serial.print(setMph,1);
    Serial.print(" meas="); Serial.print(measF,1); Serial.print("mph thr="); Serial.print(curV,2);
    Serial.print("V noeud="); Serial.print(vnode,2); Serial.println("V");
  }
}
