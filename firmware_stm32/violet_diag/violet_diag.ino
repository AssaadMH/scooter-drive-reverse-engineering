/*
 * STM32 F401RE — DIAGNOSTIC COMPLET : relais violet (open-drain 5V) + lecture BLEU + throttle
 *
 * But : voir le controleur REAGIR. On relaie le violet de l'afficheur vers le controleur (open-drain,
 * la methode qui passe), on injecte le throttle, ET on lit la telemetrie BLEU du controleur pour
 * reperer quel octet change (vitesse, etat, faute) selon throttle/relais.
 *
 *   PA9 (= D8)  <- VIOLET afficheur  : USART1 half-duplex RX
 *   PC6 (= D10) -> VIOLET controleur : USART6 half-duplex 1-fil = TX OPEN-DRAIN (niveau haut=pull-up 5V)
 *   PC7 (= D9)  <- BLEU controleur   : SoftwareSerial RX (ecoute)
 *   PB10(= D6)  -> R1k -> gris/blanc throttle   PA0(=A0) <- noeud   GND <- noir
 *   roue surelevee. jamais rouge/orange 52V.
 *
 * COMMANDES : m<v>=throttle  x=stop   r=relais ON (defaut)  s=relais OFF (stop d'emettre le violet)
 */

#include <SoftwareSerial.h>

HardwareSerial violetUart(PA9);   // <- afficheur
HardwareSerial ctrlUart(PC6);     // -> controleur (open-drain)
SoftwareSerial blueSer(PC7, PC8); // RX=PC7 (bleu) ; TX=PC8 non connecte

#define THR_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3

float thrCmd=0; bool relayOn=true;
unsigned long vFwd=0;
uint8_t blueFrame[32]; uint8_t blueLen=0; bool blueNew=false;
unsigned long blueLastUs=0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

// relais violet afficheur->controleur (cut-through open-drain, checksum recalcule)
void pumpViolet(){
  static uint8_t pos=0, x=0;
  while (violetUart.available()){
    uint8_t b=violetUart.read();
    if(!relayOn){ continue; }
    if (pos==0){ if(b==0x01){ ctrlUart.write(b); x=b; pos=1; } continue; }
    if (pos==1){ if(b==0x14){ ctrlUart.write(b); x^=b; pos=2; } else pos=0; continue; }
    if (pos<19){ ctrlUart.write(b); x^=b; pos++; }
    else { ctrlUart.write(x); pos=0; vFwd++; }
  }
}

// lecture bleu (SoftwareSerial), framing par silence
void pumpBlue(){
  static uint8_t buf[32], len=0;
  while (blueSer.available()){
    uint8_t b=blueSer.read();
    unsigned long now=micros();
    if (len && (now-blueLastUs)>4000){ memcpy(blueFrame,buf,len); blueLen=len; blueNew=true; len=0; }
    if (len<32) buf[len++]=b;
    blueLastUs=now;
  }
  if (len && (micros()-blueLastUs)>4000){ memcpy(blueFrame,buf,len); blueLen=len; blueNew=true; len=0; }
}

void setup(){
  Serial.begin(115200);
  violetUart.setHalfDuplex(); violetUart.begin(9600); violetUart.enableHalfDuplexRx();
  ctrlUart.setHalfDuplex();   ctrlUart.begin(9600);
  blueSer.begin(9600);
  analogReadResolution(12);
  pinMode(THR_PIN,OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== VIOLET_DIAG : relais violet + lecture BLEU + throttle ===");
  Serial.println("m<v>=throttle x=stop r=relais on s=relais off");
}

unsigned long lastPrint=0;
void loop(){
  pumpViolet(); pumpBlue();
  if (Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if      (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c=="r"){ relayOn=true;  Serial.println("[cmd] relais ON"); }
    else if (c=="s"){ relayOn=false; Serial.println("[cmd] relais OFF"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }
  unsigned long now=millis();
  if (now-lastPrint>=1000){
    lastPrint=now;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("relais="); Serial.print(relayOn?"on ":"off"); Serial.print(vFwd); Serial.print("/s thr=");
    Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.print("V | BLEU ");
    Serial.print(blueLen); Serial.print("o:");
    for(uint8_t i=0;i<blueLen;i++){ Serial.print(blueFrame[i]<16?" 0":" "); Serial.print(blueFrame[i],HEX); }
    if (blueLen>=10 && blueFrame[0]==0x02){
      unsigned int per=(blueFrame[8]<<8)|blueFrame[9];
      Serial.print("  | vitesse_periode="); Serial.print(per);
    }
    Serial.println();
    vFwd=0;
  }
}
