/*
 * STM32 F401RE — DECODE du fil VIOLET (mesure fréquence + duty par interruption)
 *
 * But : déterminer la NATURE du violet (horloge ? data PWM ? Hall/tachy ?) en mesurant
 *       sa vraie fréquence et son duty cycle, et en les corrélant avec vitesse/throttle/frein.
 *       (L'ADC l'aliasait : on n'avait jamais f ni duty réels.)
 *
 * CABLAGE (écoute seule, AUCUNE alimentation injectée) :
 *   PA9  (= D8)  <- fil BLEU   : UART bus half-duplex HW (télémétrie vitesse/frein). GND commun.
 *   PB3  (= D3)  <- fil VIOLET : entrée NUMERIQUE 5V-tolérante (le violet monte à ~4,9V).
 *                                 >>> NE PAS utiliser un pin ADC (PA0...) : pas 5V-tolérant -> grille.
 *   PB10 (= D6)  -> R1k -> nœud gris/blanc : PWM throttle (pour tester en roulant). 3,3V.
 *   PA0  (= A0)  <- nœud : relecture tension throttle.
 *   GND  <- fil NOIR.
 *
 * COMMANDES (Serial 115200) :
 *   m<v>  throttle manuel en volts commandés (ex. m2.0)  | 0 / n = repos | x = ARRET
 *   (roue surélevée ! jamais l'Arduino sur rouge/orangé 52V)
 *
 * SORTIE 1x/s : spd=<mph> brk=<O/N> | violet f=<Hz> duty=<%> edges=<n> hi/lo(us) | thr_cmd thr_node
 */

HardwareSerial busSerial(PA9);   // single-wire USART1 (fil bleu)

#define SPEED_K   2520.0
#define VIOLET_PIN PB3           // D3, 5V-tolérant
#define THR_PIN    PB10          // D6 PWM throttle
#define NODE_PIN   PA0           // A0 relecture nœud
#define VREF       3.3

// ---- mesure du violet (ISR) ----
volatile unsigned long lastRise = 0, lastFall = 0;
volatile unsigned long sumPeriod = 0, sumHigh = 0;  // accumulés en us
volatile unsigned long edgeCount = 0;               // nb de fronts montants comptés
volatile unsigned long lastHigh = 0, lastPeriod = 0;

void violetISR() {
  unsigned long now = micros();
  if (digitalRead(VIOLET_PIN)) {            // front MONTANT
    if (lastRise) {
      unsigned long per = now - lastRise;
      lastPeriod = per; sumPeriod += per; edgeCount++;
    }
    lastRise = now;
  } else {                                   // front DESCENDANT
    if (lastRise) { lastHigh = now - lastRise; sumHigh += lastHigh; }
  }
}

// ---- bus (vitesse/frein) ----
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

// ---- throttle ----
float thrCmd = 0;
void applyThrottle() {
  // PWM 3,3V : duty = thrCmd/VREF (clamp). analogWrite 0-255.
  float v = thrCmd; if (v < 0) v = 0; if (v > VREF) v = VREF;
  analogWrite(THR_PIN, (int)(v / VREF * 255.0));
}

void setup() {
  Serial.begin(115200);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  pinMode(VIOLET_PIN, INPUT);
  analogReadResolution(12);
  pinMode(THR_PIN, OUTPUT); applyThrottle();
  attachInterrupt(digitalPinToInterrupt(VIOLET_PIN), violetISR, CHANGE);
  delay(300);
  Serial.println("\n=== VIOLET DECODE (PB3/D3) === m<v>=throttle  0/n=repos  x=ARRET");
}

unsigned long lastPrint = 0;
void loop() {
  readBus();

  if (Serial.available()) {
    String s = Serial.readStringUntil('\n'); s.trim();
    if (s == "x" || s == "0" || s == "n") { thrCmd = 0; applyThrottle(); Serial.println("[throttle] REPOS"); }
    else if (s.length() && s[0] == 'm') { thrCmd = s.substring(1).toFloat(); applyThrottle();
      Serial.print("[throttle] cmd="); Serial.print(thrCmd,2); Serial.println(" V"); }
  }

  unsigned long now = millis();
  if (now - lastPrint >= 1000) {
    lastPrint = now;

    // snapshot ISR (atomique court)
    noInterrupts();
    unsigned long sp = sumPeriod, sh = sumHigh, ec = edgeCount, lp = lastPeriod, lh = lastHigh;
    sumPeriod = 0; sumHigh = 0; edgeCount = 0;
    interrupts();

    float vf = 0, duty = 0;
    if (ec > 0 && sp > 0) {
      float avgPer = (float)sp / ec;          // us
      vf = 1000000.0 / avgPer;                // Hz
      if (sh > 0) duty = 100.0 * (float)sh / sp;
    }

    // vitesse + frein
    float mph = 0; bool brake = false;
    if (lastLen >= 13) { brake = (lastFrame[4] == 0xA0);
      if (lastFrame[8] != 0xEA) { uint16_t per = ((uint16_t)lastFrame[8] << 8) | lastFrame[9]; if (per >= 10) mph = SPEED_K / (float)per; } }

    float node = analogRead(NODE_PIN) * VREF / 4095.0;

    Serial.print("spd="); Serial.print(mph,1);
    Serial.print(" brk="); Serial.print(brake?"O":"N");
    Serial.print(" | violet f="); Serial.print(vf,1); Serial.print("Hz");
    Serial.print(" duty="); Serial.print(duty,1); Serial.print("%");
    Serial.print(" edges="); Serial.print(ec);
    Serial.print(" hi="); Serial.print(lh); Serial.print("us per="); Serial.print(lp); Serial.print("us");
    Serial.print(" | thr_cmd="); Serial.print(thrCmd,2);
    Serial.print(" thr_node="); Serial.print(node,2); Serial.print("V");
    if (ec == 0) Serial.print("  [violet FIGE / pas de fronts]");
    Serial.println();
  }
}
