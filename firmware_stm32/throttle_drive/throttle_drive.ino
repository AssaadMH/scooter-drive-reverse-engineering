/*
 * STM32 F401RE — THROTTLE DRIVE + lecture vitesse (test de roue)
 *   D6 (PB10) : PWM throttle -> R1k -> noeud gris/blanc (0..3.3V, parfait pour 0.8-3.2V)
 *   PA9 (D8)  : bus bleu en half-duplex (lecture vitesse + flag frein)
 *   GND       : noir (masse commune)
 * Pull-down 10k sur le noeud (fail-safe). ROUE EN L'AIR.
 *
 * COMMANDES (115200) :
 *   un nombre ex "1.5" -> throttle cible (rampe douce) ; n = repos ; x = ARRET
 */

HardwareSerial busSerial(PA9);

#define PWM_PIN   PB10        // = D6
#define VREF      3.3
#define IDLE_V    0.80
#define MAX_V     3.00        // plafond de securite (relever progressivement)
#define ABS_MAX   3.20
#define RAMP_VPS  0.50
#define SPEED_K   2520.0

float targetV = IDLE_V, curV = IDLE_V;
unsigned long lastStep = 0, lastPrint = 0;
uint8_t lastFrame[32]; uint8_t lastLen = 0;

void applyV(float v) {
  if (v < 0) v = 0; if (v > ABS_MAX) v = ABS_MAX;
  int d = (int)(v / VREF * 255.0 + 0.5); if (d < 0) d = 0; if (d > 255) d = 255;
  analogWrite(PWM_PIN, d);
}
void setTarget(float v) {
  if (v < IDLE_V) v = IDLE_V; if (v > MAX_V) v = MAX_V; targetV = v;
  Serial.print("Cible="); Serial.print(targetV, 2); Serial.println("V");
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
        if (x == buf[len - 1]) { memcpy(lastFrame, buf, len); lastLen = len; }
        idx = 0; len = 0;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PWM_PIN, OUTPUT);
  applyV(IDLE_V);                   // demarre au repos
  analogReadResolution(12);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  delay(500);
  Serial.println("\n=== STM32 THROTTLE DRIVE === nombre=V | n=repos | x=ARRET. ROUE EN L'AIR.");
}

void loop() {
  readBus();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); s.trim();
    if      (s == "x") { targetV = curV = IDLE_V; applyV(IDLE_V); Serial.println("*** ARRET ***"); }
    else if (s == "n") setTarget(IDLE_V);
    else if (s.length()) { float f = s.toFloat(); if (f > 0) setTarget(f); }
  }

  unsigned long now = millis();
  if (now - lastStep >= 20) {
    lastStep = now;
    float st = RAMP_VPS * 0.020;
    if (curV < targetV) { curV += st; if (curV > targetV) curV = targetV; }
    else if (curV > targetV) { curV -= st; if (curV < targetV) curV = targetV; }
    applyV(curV);
  }

  if (now - lastPrint >= 300) {
    lastPrint = now;
    float mph = 0; bool brake = false;
    if (lastLen >= 13) { brake = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) { uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9]; if (per >= 10) mph = SPEED_K / (float)per; } }
    float vnode = analogRead(PA0) * 3.3 / 4095.0;
    Serial.print("thr="); Serial.print(curV, 2); Serial.print("V noeud="); Serial.print(vnode,2); Serial.print("V | spd="); Serial.print(mph, 1);
    Serial.print(" | FREIN="); Serial.print(brake ? "OUI" : "NON"); Serial.print(" | ");
    if (lastLen) { for (uint8_t i=0;i<lastLen;i++){ if(lastFrame[i]<0x10)Serial.print('0'); Serial.print(lastFrame[i],HEX); Serial.print(' '); } } else Serial.print("(aucune)");
    Serial.println();
  }
}
