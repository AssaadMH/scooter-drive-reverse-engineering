/*
 * THROTTLE LOOP — Phase 1 : injecte le throttle ET ecoute le bus (decouverte octet vitesse)
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : trouver quel octet des trames (sur le fil bleu) = la VITESSE, en le correlant
 *       au km/h affiche. Une fois identifie -> on ajoute la regulation PI (phase 2).
 *
 * CABLAGE
 *   D11 --[R 1k]--+-- fil GRIS/BLANC (cote controleur, coupe)  [throttle, PWM]
 *                 +--[C 10uF]-- GND
 *   D10 --------- fil BLEU  (tap EN PARALLELE, NE PAS COUPER)   [ecoute UART 9600]
 *   D12 --------- NON CONNECTE (TX logiciel oblige par SoftwareSerial)
 *   GND --------- fil NOIR                                       [masse commune]
 *   ☠️ Jamais rouge/orange (52V).
 *
 * COMMANDES (115200) :
 *   nombre ex "1.5" -> throttle cible V | u/d | n=repos | x=ARRET
 *   s1/s2/s3 = scenarios | mon = affichage trames on/off | ?=etat
 */

#include <SoftwareSerial.h>

#define RX_PIN     10      // <- fil bleu (ecoute)
#define TX_DUMMY   12      // non connecte
#define PWM_PIN    11      // -> throttle (PWM + RC)
#define BUS_BAUD   9600

#define VCC        5.0
#define IDLE_V     0.80
#define MAX_V      2.00
#define ABS_MAX_V  3.40
#define CAL        1.00
#define RAMP_VPS   0.50

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);

float targetV = IDLE_V, curV = IDLE_V;
unsigned long lastStep = 0, lastPrint = 0;
bool monOn = true;

// dernier frame valide recu
uint8_t lastFrame[32]; uint8_t lastLen = 0;

struct Step { float v; unsigned long holdMs; };
Step SCN_RAMP[]  = {{1.6,3000},{IDLE_V,0}};
Step SCN_STEPS[] = {{1.0,2000},{1.4,2000},{1.8,2000},{IDLE_V,0}};
Step SCN_DRIVE[] = {{1.6,2500},{1.0,1500},{1.9,2500},{1.2,1500},{IDLE_V,0}};
Step* scn=0; int scnLen=0, scnIdx=0; bool holding=false; unsigned long holdStart=0;

void applyVoltage(float v){
  if(v<0)v=0; if(v>ABS_MAX_V)v=ABS_MAX_V;
  int duty=(int)((v/CAL)/VCC*255.0+0.5);
  if(duty<0)duty=0; if(duty>255)duty=255;
  analogWrite(PWM_PIN,duty);
}
void setTarget(float v){
  if(v<IDLE_V)v=IDLE_V; if(v>MAX_V)v=MAX_V; targetV=v;
  Serial.print(F("Cible = ")); Serial.print(targetV,2); Serial.println(F(" V"));
}
void startScenario(Step*s,int len,const __FlashStringHelper*name){
  scn=s; scnLen=len; scnIdx=0; holding=false;
  Serial.print(F(">>> SCENARIO: ")); Serial.println(name); setTarget(scn[0].v);
}
void stopScenario(const __FlashStringHelper*why){
  scn=0; curV=targetV=IDLE_V; applyVoltage(IDLE_V);
  Serial.print(F("*** ")); Serial.print(why); Serial.println(F(" -> repos ***"));
}

// Lit le bus et reconstruit une trame : 02 [LEN] ... avec checksum XOR sur LEN-1 octets.
void readBus(){
  static uint8_t buf[32]; static uint8_t idx=0; static uint8_t len=0;
  while(busSerial.available()){
    uint8_t b=busSerial.read();
    if(idx==0){ if(b==0x02){ buf[idx++]=b; } }
    else if(idx==1){ // octet longueur
      if(b>=4 && b<=32){ buf[idx++]=b; len=b; } else { idx=0; }
    } else {
      buf[idx++]=b;
      if(idx>=len){
        uint8_t x=0; for(uint8_t i=0;i<len-1;i++) x^=buf[i];
        if(x==buf[len-1]){ memcpy(lastFrame,buf,len); lastLen=len; } // trame valide
        idx=0; len=0;
      }
    }
  }
}

void setup(){
  Serial.begin(115200);
  pinMode(PWM_PIN,OUTPUT);
  applyVoltage(IDLE_V);
  busSerial.begin(BUS_BAUD);
  delay(1000);
  Serial.println(F("\n=== THROTTLE LOOP (Uno) — Phase 1 : decouverte vitesse ==="));
  Serial.println(F("Affiche: thr=<V> | trame hex. Correle l'octet qui monte avec le km/h."));
  Serial.println(F("nombre=cible | u/d | n | x | s1/s2/s3 | mon | ?"));
}

void loop(){
  readBus();

  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if      (s=="x")  stopScenario(F("ARRET"));
    else if (s=="s1") startScenario(SCN_RAMP ,sizeof(SCN_RAMP )/sizeof(Step),F("rampe douce"));
    else if (s=="s2") startScenario(SCN_STEPS,sizeof(SCN_STEPS)/sizeof(Step),F("paliers"));
    else if (s=="s3") startScenario(SCN_DRIVE,sizeof(SCN_DRIVE)/sizeof(Step),F("cycle conduite"));
    else if (s=="n")  { scn=0; setTarget(IDLE_V); }
    else if (s=="u")  { scn=0; setTarget(targetV+0.1); }
    else if (s=="d")  { scn=0; setTarget(targetV-0.1); }
    else if (s=="mon"){ monOn=!monOn; Serial.print(F("monitor=")); Serial.println(monOn?F("on"):F("off")); }
    else if (s=="?")  { Serial.print(F("cur=")); Serial.print(curV,2); Serial.print(F("V cible=")); Serial.print(targetV,2); Serial.println(F("V")); }
    else if (s.length()){ float v=s.toFloat(); if(v>0){ scn=0; setTarget(v); } }
  }

  unsigned long now=millis();
  if(now-lastStep>=20){
    float step=RAMP_VPS*0.020;
    if(curV<targetV){curV+=step; if(curV>targetV)curV=targetV;}
    else if(curV>targetV){curV-=step; if(curV<targetV)curV=targetV;}
    applyVoltage(curV);
    lastStep=now;
    if(scn){
      float diff=curV-scn[scnIdx].v; if(diff<0)diff=-diff;
      if(!holding){ if(diff<0.02){holding=true; holdStart=now;} }
      else if(now-holdStart>=scn[scnIdx].holdMs){
        scnIdx++;
        if(scnIdx>=scnLen){scn=0; Serial.println(F(">>> SCENARIO termine"));}
        else{holding=false; setTarget(scn[scnIdx].v);}
      }
    }
  }

  // Affichage periodique : throttle + derniere trame (pour correler la vitesse)
  if(monOn && now-lastPrint>=300){
    lastPrint=now;
    Serial.print(F("thr=")); Serial.print(curV,2); Serial.print(F("V | "));
    if(lastLen){
      for(uint8_t i=0;i<lastLen;i++){ if(lastFrame[i]<0x10)Serial.print('0'); Serial.print(lastFrame[i],HEX); Serial.print(' '); }
    } else Serial.print(F("(pas de trame)"));
    Serial.println();
  }
}
