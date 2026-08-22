/*
 * STM32 F401RE — INJECTION sur le bus VIOLET (commandes afficheur->contrôleur)
 *
 * Le violet est un série 9600 8N1, trame 20 o, checksum = XOR des octets [0..18].
 *   [4] : bit 0x80=FREIN, bit 0x10=mode S/D, nibble bas=gear (1->5,2->A,3->F)
 *   [5] : bit 0x20=feu route   [18] : bit 0x08/0x10=clignotants
 * On COUPE le violet et on émule l'afficheur côté contrôleur (TX continu ~10 trames/s).
 *
 * CABLAGE (roue SURÉLEVÉE ; jamais Arduino sur rouge/orangé 52V) :
 *   PA9  (= D8)  <- fil BLEU            : télémétrie (vitesse/frein) half-duplex HW. Tap parallèle.
 *   PC6  (= D10) -> VIOLET côté CONTRÔLEUR : USART6 TX (on injecte ici). LE VIOLET DOIT ÊTRE COUPÉ.
 *   PB10 (= D6)  -> R1k -> gris/blanc   : PWM throttle (pour lancer la roue). 3,3V.
 *   PA0  (= A0)  <- nœud throttle       : relecture.
 *   GND  <- fil NOIR.
 *   (le bout DASHBOARD du violet coupé : laissé en l'air, sans effet)
 *
 * COMMANDES (Serial 115200) :
 *   f = FREIN ON (bit 0x80)   r = frein OFF
 *   1/2/3 = gear (5/A/F)      s = bascule mode S/D (bit 0x10)
 *   m<v> = throttle volts (ex m2.0)   x/0 = throttle 0
 *   p = afficher la trame courante
 *
 * PROTOCOLE : 1) injecter SANS rien changer -> l'afficheur ne doit PAS mettre d'erreur (trame OK).
 *             2) m2.0 pour lancer la roue, puis 'f' -> la roue doit RALENTIR/STOPPER (frein moteur).
 */

HardwareSerial busSerial(PA9);            // bus bleu (télémétrie)
HardwareSerial violetTx(PC7, PC6);        // USART6 : on n'utilise que TX=PC6 vers le contrôleur

#define SPEED_K   2520.0
#define THR_PIN   PB10
#define NODE_PIN  PA0
#define VREF      3.3
#define TX_MS     100                     // ~10 trames/s comme l'afficheur d'origine

// trame de base observée au repos (gear3, mode tel quel, frein off)
uint8_t frame[20] = {0x01,0x14,0x01,0x00,0x0F,0x80,0x1E,0x00,0x91,0x01,0x05,0x00,0x64,0x0C,0x01,0xAE,0x00,0x00,0x05,0x00};
float thrCmd = 0;

void setChecksum() { uint8_t x = 0; for (uint8_t i = 0; i < 19; i++) x ^= frame[i]; frame[19] = x; }

void setGear(uint8_t g) {                 // g=1/2/3 -> nibble bas 5/A/F, garde bits hauts ([4] flags)
  uint8_t nib = (g==1)?0x05 : (g==2)?0x0A : 0x0F;
  frame[4] = (frame[4] & 0xF0) | nib;
}

void applyThrottle() { float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

// ---- bus (feedback vitesse/frein) ----
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

void printFrame(){ Serial.print("trame: "); for(uint8_t i=0;i<20;i++){ if(frame[i]<0x10)Serial.print('0'); Serial.print(frame[i],HEX); Serial.print(' ');} Serial.println(); }

void setup() {
  Serial.begin(115200);
  busSerial.setHalfDuplex(); busSerial.begin(9600); busSerial.enableHalfDuplexRx();
  violetTx.begin(9600);                   // 8N1
  analogReadResolution(12);
  pinMode(THR_PIN, OUTPUT); applyThrottle();
  setGear(3); setChecksum();
  delay(300);
  Serial.println("\n=== VIOLET INJECT (TX PC6/D10 @9600) === f/r=frein 1/2/3=gear s=S/D m<v>=thr x=stop p=trame");
  printFrame();
}

unsigned long lastTx=0, lastPrint=0;
void loop() {
  readBus();

  if (Serial.available()) {
    String c = Serial.readStringUntil('\n'); c.trim();
    if      (c=="f"){ frame[4]|=0x80; setChecksum(); Serial.println("[cmd] FREIN ON"); }
    else if (c=="r"){ frame[4]&=~0x80; setChecksum(); Serial.println("[cmd] frein off"); }
    else if (c=="1"||c=="2"||c=="3"){ setGear(c.toInt()); setChecksum(); Serial.print("[cmd] gear "); Serial.println(c); }
    else if (c=="s"){ frame[4]^=0x10; setChecksum(); Serial.println("[cmd] bascule mode S/D"); }
    else if (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
    else if (c=="p"){ printFrame(); }
  }

  unsigned long now = millis();
  if (now - lastTx >= TX_MS) { lastTx = now; violetTx.write(frame, 20); }

  if (now - lastPrint >= 1000) {
    lastPrint = now;
    float mph=0; bool brake=false;
    if (lastLen>=13){ brake=(lastFrame[4]==0xA0);
      if(lastFrame[8]!=0xEA){ uint16_t per=((uint16_t)lastFrame[8]<<8)|lastFrame[9]; if(per>=10) mph=SPEED_K/(float)per; } }
    float node = analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("spd="); Serial.print(mph,1); Serial.print(" brk="); Serial.print(brake?"O":"N");
    Serial.print(" | frein_inj="); Serial.print((frame[4]&0x80)?"ON":"off");
    Serial.print(" gear[4]=0x"); Serial.print(frame[4],HEX);
    Serial.print(" thr_cmd="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.println("V");
  }
}
