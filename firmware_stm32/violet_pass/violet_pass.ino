/*
 * STM32 F401RE — RELAIS TRANSPARENT (passe-tout) + DECODE frein/gear/feux, TX OPEN-DRAIN 5V
 *
 * Recopie chaque octet afficheur->controleur SANS filtrer ni editer (la sequence d'armement
 * du boot passe intacte), ET decode au passage octet[4] (frein/gear/mode) et octet[5] (feux)
 * pour AFFICHER l'etat de commande reel que recoit le controleur.
 *
 * Option "FORCE OK" (commande 'k') : force frein OFF (bit 0x80=0) + gear3 (nibble 0x0F) dans
 * la trame, pour tester si un frein engage / gear neutre cote afficheur bloque le moteur.
 * 'p' = passe-tout pur (defaut, n'edite rien).
 *
 * CABLAGE :
 *   PA9 (=D8) <- VIOLET cote AFFICHEUR (RX)   PC6 (=D10) -> VIOLET cote CONTROLEUR (TX open-drain)
 *   PB10(=D6) -> R1k -> gris/blanc throttle    PA0(=A0) <- noeud    GND <- noir
 *   roue SURELEVEE. jamais rouge/orange 52V.
 *
 * COMMANDES : m<v>=throttle  x=stop  k=force(frein off+gear3)  p=passe-tout pur
 */

HardwareSerial dashUart(PA9);
HardwareSerial ctrlUart(PC6);

#define THR_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3

float thrCmd=0;
bool forceOk=false;            // k = force frein off + gear3
unsigned long bytesAccum=0;
volatile uint8_t lastFlags=0, lastLights=0;   // octet[4], octet[5] vus
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

// passe-tout avec parsing leger : on suit la position dans la trame 01 14 .. (20 o), checksum recalcule
void pump(){
  static uint8_t pos=0, x=0;
  while (dashUart.available()){
    uint8_t b = dashUart.read(); bytesAccum++;
    if (pos==0){ if (b==0x01){ ctrlUart.write(b); x=b; pos=1; } continue; }
    if (pos==1){ if (b==0x14){ ctrlUart.write(b); x^=b; pos=2; } else { pos=0; } continue; }
    if (pos==4){ lastFlags=b; if(forceOk){ b=(b&0x70)|0x0F; } }   // efface frein(0x80), gear=3 ; garde mode(0x10)
    if (pos==5){ lastLights=b; }
    if (pos<19){ ctrlUart.write(b); x^=b; pos++; }
    else { ctrlUart.write(x); pos=0; }
  }
}

void setup(){
  Serial.begin(115200);
  dashUart.setHalfDuplex(); dashUart.begin(9600); dashUart.enableHalfDuplexRx();
  ctrlUart.setHalfDuplex(); ctrlUart.begin(9600);
  analogReadResolution(12);
  pinMode(THR_PIN,OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== VIOLET PASSE-TOUT + DECODE (PA9->PC6) ===");
  Serial.println("m<v>=throttle x=stop k=force(frein off+gear3) p=passe-tout");
}

unsigned long lastPrint=0;
void loop(){
  pump();
  if (Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if      (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c=="k"){ forceOk=true;  Serial.println("[cmd] FORCE frein off + gear3"); }
    else if (c=="p"){ forceOk=false; Serial.println("[cmd] passe-tout pur"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }
  unsigned long now=millis();
  if (now-lastPrint>=1000){
    lastPrint=now; unsigned long bps=bytesAccum; bytesAccum=0;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    uint8_t f=lastFlags;
    Serial.print("pass="); Serial.print(bps); Serial.print(" o/s | AFFICHEUR: frein=");
    Serial.print((f&0x80)?"ON":"off"); Serial.print(" gear="); Serial.print(f&0x0F);
    Serial.print(" mode="); Serial.print((f&0x10)?"D":"S"); Serial.print(" feu=");
    Serial.print((lastLights&0x20)?"ON":"off");
    Serial.print(" | force="); Serial.print(forceOk?"OUI":"non");
    Serial.print(" thr="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.print("V");
    if(bps==0) Serial.print("  [rien afficheur: verifie PA9/D8]");
    Serial.println();
  }
}
