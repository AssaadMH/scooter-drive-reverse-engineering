/*
 * STM32 F401RE — RELAIS "homme du milieu" sur le bus VIOLET
 *
 * Le violet (série 9600, trame 20 o, checksum XOR [0..18]) = commandes afficheur->contrôleur.
 * Couper le violet -> afficheur E07 -> contrôleur en sécurité (la roue ne tourne plus).
 * SOLUTION : on s'intercale. On LIT la trame de l'afficheur, on la RÉÉMET vers le contrôleur,
 *            en forçant le bit frein (0x80 de l'octet[4]) à la demande. L'afficheur reste content.
 *
 * CABLAGE (violet COUPÉ, les 2 bouts vont au STM32) :
 *   PC7 (= D9)  <- VIOLET côté AFFICHEUR   : USART6 RX (on lit la trame d'origine)
 *   PC6 (= D10) -> VIOLET côté CONTRÔLEUR  : USART6 TX (on réémet, frein modifié si besoin)
 *   PA9 (= D8)  <- fil BLEU                : télémétrie (vitesse/frein). Tap parallèle.
 *   PB10(= D6)  -> R1k -> gris/blanc       : PWM throttle (lancer la roue). 3,3V.
 *   PA0 (= A0)  <- nœud throttle           : relecture.
 *   GND <- noir.  (roue SURÉLEVÉE ; jamais Arduino sur rouge/orangé 52V)
 *
 * COMMANDES (Serial 115200) :
 *   f = forcer FREIN ON   r = relâcher (laisser passer le frein réel)
 *   m<v> = throttle volts   x/0 = throttle 0
 *
 * SORTIE 1x/s : spd, brk(bleu), frein forcé, dernière trame relayée, nb trames/s.
 */

HardwareSerial relay(PC7, PC6);   // USART6 : RX=PC7 (afficheur), TX=PC6 (contrôleur)
HardwareSerial busSerial(PA9);    // bus bleu (télémétrie)

#define SPEED_K  2520.0
#define THR_PIN  PB10
#define NODE_PIN PA0
#define VREF     3.3

bool forceBrake = false;
float thrCmd = 0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

uint8_t outLast[20]; unsigned long framesFwd = 0, framesAccum = 0;

// relais : assemble une trame 20o de l'afficheur, edite le frein, recalcule le checksum, renvoie
void doRelay() {
  static uint8_t buf[20]; static uint8_t idx = 0;
  while (relay.available()) {
    uint8_t b = relay.read();
    if (idx == 0)      { if (b == 0x01) buf[idx++] = b; }       // header
    else if (idx == 1) { if (b == 0x14) buf[idx++] = b; else idx = 0; }  // longueur 20
    else {
      buf[idx++] = b;
      if (idx >= 20) {
        idx = 0;
        uint8_t x = 0; for (uint8_t i = 0; i < 19; i++) x ^= buf[i];
        if (x == buf[19]) {                       // trame valide de l'afficheur
          if (forceBrake) buf[4] |= 0x80; // sinon : laisse le frein réel tel quel
          uint8_t c = 0; for (uint8_t i = 0; i < 19; i++) c ^= buf[i]; buf[19] = c;  // recalcul checksum
          relay.write(buf, 20);                   // -> contrôleur
          memcpy(outLast, buf, 20); framesAccum++;
        }
      }
    }
  }
}

// ---- bus bleu (feedback) ----
uint8_t lastFrame[32]; uint8_t lastLen = 0;
void readBus() {
  static uint8_t buf[32]; static uint8_t idx=0, len=0;
  while (busSerial.available()) {
    uint8_t b = busSerial.read();
    if (idx==0)      { if (b==0x02) buf[idx++]=b; }
    else if (idx==1) { if (b>=4 && b<=32){ buf[idx++]=b; len=b; } else idx=0; }
    else { buf[idx++]=b; if (idx>=len){ uint8_t x=0; for(uint8_t i=0;i<len-1;i++)x^=buf[i];
             if(x==buf[len-1]){memcpy(lastFrame,buf,len);lastLen=len;} idx=0;len=0; } }
  }
}

void setup() {
  Serial.begin(115200);
  relay.begin(9600);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  analogReadResolution(12);
  pinMode(THR_PIN, OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== VIOLET RELAY (RX PC7/D9 -> TX PC6/D10) === f/r=frein m<v>=thr x=stop");
}

unsigned long lastPrint = 0;
void loop() {
  doRelay();
  readBus();

  if (Serial.available()) {
    String c = Serial.readStringUntil('\n'); c.trim();
    if      (c=="f"){ forceBrake=true;  Serial.println("[cmd] FREIN forcé ON"); }
    else if (c=="r"){ forceBrake=false; Serial.println("[cmd] frein relâché (réel)"); }
    else if (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }

  unsigned long now = millis();
  if (now - lastPrint >= 1000) {
    lastPrint = now;
    framesFwd = framesAccum; framesAccum = 0;
    float mph=0; bool brake=false;
    if (lastLen>=13){ brake=(lastFrame[4]==0xA0);
      if(lastFrame[8]!=0xEA){ uint16_t per=((uint16_t)lastFrame[8]<<8)|lastFrame[9]; if(per>=10) mph=SPEED_K/(float)per; } }
    float node = analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("spd="); Serial.print(mph,1); Serial.print(" brk="); Serial.print(brake?"O":"N");
    Serial.print(" | frein_force="); Serial.print(forceBrake?"ON":"off");
    Serial.print(" relais="); Serial.print(framesFwd); Serial.print("/s [4]=0x"); Serial.print(outLast[4],HEX);
    Serial.print(" thr="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.print("V");
    if (framesFwd==0) Serial.print("  [AUCUNE trame relayee : verifie PC7<-afficheur]");
    Serial.println();
  }
}
