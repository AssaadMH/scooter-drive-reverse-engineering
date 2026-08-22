/*
 * THROTTLE PI — Phase 2 : BOUCLE FERMEE de vitesse
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * Lit la vitesse renvoyee par le controleur (fil bleu, UART 9600) et ajuste le
 * throttle (PWM D11 -> RC -> fil gris/blanc) pour tenir une vitesse cible (mph).
 *
 * MESURE : champ B8B9 des trames = PERIODE (inverse vitesse).
 *          stoppe -> sentinelle EA60 (=60000). En mouvement : vitesse = SPEED_K / B8B9.
 *          Calibre 2026-06-03 : B8B9=126 -> 20 mph, B8B9=360 -> 7 mph => SPEED_K ~ 2520 (ex-667 faux).
 *
 * CABLAGE (identique phase 1) :
 *   D11 --[R 1k]--+-- gris/blanc (cote controleur, coupe)   [throttle PWM]
 *                 +--[C 10uF]-- GND
 *   D10 --------- fil BLEU (tap parallele, NE PAS couper)    [ecoute vitesse]
 *   D12 --------- non connecte                                [TX factice]
 *   GND --------- fil NOIR                                    [masse commune]
 *   ☠️ Jamais rouge/orange (52V).
 *
 * COMMANDES (115200) :
 *   c8        -> BOUCLE FERMEE : tenir 8 mph (c0 ou n = repos, boucle off)
 *   un nombre -> mode MANUEL : throttle = X volts (coupe la boucle)
 *   x         -> ARRET d'urgence (repos, boucle off)
 *   ?         -> etat (mode, vitesse mesuree, throttle)
 *
 * /!\ ROUE SURELEVEE. Demarre au repos. MAX_V plafonne. 'x' coupe tout.
 */

#include <SoftwareSerial.h>

#define RX_PIN     10
#define TX_DUMMY   12
#define PWM_PIN    11
#define BUS_BAUD   9600

#define VCC        5.0
#define IDLE_V     0.80
#define THRESH_V   1.35     // throttle "feedforward" pour amorcer le mouvement
#define MAX_V      2.20     // plafond de securite (relever progressivement si besoin)
#define ABS_MAX_V  3.40
#define CAL        1.00
#define SPEED_K    2520.0   // mph = SPEED_K / B8B9   (recalibre 2026-06-03 : 126->20mph, 360->7mph ; ex-667 faux)

// --- regulateur PI ---
#define KP         0.060    // V par mph d'erreur
#define KI         0.030    // V par (mph.s)
#define CTRL_MS    150      // periode de regulation
#define SLEW       0.12     // V max de variation par cycle (anti a-coup)
#define INTEG_CLAMP 8.0     // anti-windup

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);

// modes
enum { MANUAL, LOOP };
int mode = MANUAL;
float curV = IDLE_V, targetV = IDLE_V;   // manuel
float setMph = 0, integ = 0;             // boucle
float measMph = 0;
unsigned long lastTick=0, lastCtrl=0, lastPrint=0, lastFrameMs=0;

// filtre median des periodes
#define NPER 8
uint16_t pbuf[NPER]; uint8_t pcount=0, ppos=0;

void applyVoltage(float v){
  if(v<0)v=0; if(v>ABS_MAX_V)v=ABS_MAX_V;
  int duty=(int)((v/CAL)/VCC*255.0+0.5);
  if(duty<0)duty=0; if(duty>255)duty=255;
  analogWrite(PWM_PIN,duty);
}

uint16_t medianPeriod(){
  uint16_t t[NPER]; uint8_t n=pcount<NPER?pcount:NPER;
  for(uint8_t i=0;i<n;i++) t[i]=pbuf[i];
  for(uint8_t i=1;i<n;i++){ uint16_t k=t[i]; int j=i-1; while(j>=0&&t[j]>k){t[j+1]=t[j];j--;} t[j+1]=k; }
  return n? t[n/2] : 0;
}

void pushPeriod(uint16_t p){
  pbuf[ppos]=p; ppos=(ppos+1)%NPER; if(pcount<NPER)pcount++;
}

void readBus(){
  static uint8_t buf[32]; static uint8_t idx=0, len=0;
  while(busSerial.available()){
    uint8_t b=busSerial.read();
    if(idx==0){ if(b==0x02) buf[idx++]=b; }
    else if(idx==1){ if(b>=4&&b<=32){buf[idx++]=b; len=b;} else idx=0; }
    else {
      buf[idx++]=b;
      if(idx>=len){
        uint8_t x=0; for(uint8_t i=0;i<len-1;i++) x^=buf[i];
        if(x==buf[len-1] && len>=10){               // trame valide
          lastFrameMs=millis();
          if(buf[8]==0xEA){ measMph=0; pcount=0; ppos=0; }   // sentinelle = arret
          else {
            uint16_t per=((uint16_t)buf[8]<<8)|buf[9];
            if(per>=30){ pushPeriod(per); uint16_t m=medianPeriod(); if(m) measMph=SPEED_K/(float)m; }
          }
        }
        idx=0; len=0;
      }
    }
  }
}

void idleAll(const __FlashStringHelper* why){
  mode=MANUAL; targetV=curV=IDLE_V; integ=0; setMph=0; applyVoltage(IDLE_V);
  Serial.print(F("*** ")); Serial.print(why); Serial.println(F(" -> repos ***"));
}

void setup(){
  Serial.begin(115200);
  pinMode(PWM_PIN,OUTPUT);
  applyVoltage(IDLE_V);
  busSerial.begin(BUS_BAUD);
  delay(1000);
  Serial.println(F("\n=== THROTTLE PI (Uno) — BOUCLE FERMEE vitesse ==="));
  Serial.println(F("c<mph>=tenir vitesse | nombre=manuel V | n=repos | x=ARRET | ?"));
}

void loop(){
  readBus();

  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if(s=="x") idleAll(F("ARRET"));
    else if(s=="n") idleAll(F("repos"));
    else if(s=="?"){
      Serial.print(F("mode=")); Serial.print(mode==LOOP?F("LOOP"):F("MANUEL"));
      Serial.print(F(" set=")); Serial.print(setMph,1);
      Serial.print(F("mph meas=")); Serial.print(measMph,1);
      Serial.print(F("mph thr=")); Serial.print(curV,2); Serial.println(F("V"));
    }
    else if(s.length() && (s[0]=='c'||s[0]=='C')){
      float v=s.substring(1).toFloat();
      if(v<=0) idleAll(F("cible 0"));
      else { mode=LOOP; setMph=v; integ=0; Serial.print(F(">>> BOUCLE: tenir ")); Serial.print(setMph,1); Serial.println(F(" mph")); }
    }
    else if(s.length()){ float v=s.toFloat(); if(v>0){ mode=MANUAL; targetV=(v<IDLE_V?IDLE_V:(v>MAX_V?MAX_V:v)); Serial.print(F("MANUEL cible=")); Serial.print(targetV,2); Serial.println(F("V")); } }
  }

  unsigned long now=millis();

  // securite : telemetrie perdue depuis >1.5s en boucle -> repos
  if(mode==LOOP && now-lastFrameMs>1500){ idleAll(F("TELEMETRIE PERDUE")); }

  // --- regulateur PI (mode LOOP) ---
  if(mode==LOOP && now-lastCtrl>=CTRL_MS){
    float dt=(now-lastCtrl)/1000.0; lastCtrl=now;
    float err=setMph-measMph;
    integ+=err*dt;
    if(integ>INTEG_CLAMP)integ=INTEG_CLAMP; if(integ<-INTEG_CLAMP)integ=-INTEG_CLAMP;
    float out=THRESH_V + KP*err + KI*integ;
    if(out<IDLE_V)out=IDLE_V; if(out>MAX_V)out=MAX_V;
    if(out>curV+SLEW)out=curV+SLEW; else if(out<curV-SLEW)out=curV-SLEW;  // slew
    curV=out;
    applyVoltage(curV);
  }

  // --- mode MANUEL : rampe douce ---
  if(now-lastTick>=20){
    lastTick=now;
    if(mode==MANUAL){
      float st=0.5*0.020;
      if(curV<targetV){curV+=st; if(curV>targetV)curV=targetV;}
      else if(curV>targetV){curV-=st; if(curV<targetV)curV=targetV;}
      applyVoltage(curV);
    }
  }

  // --- etat periodique ---
  if(now-lastPrint>=500){
    lastPrint=now;
    Serial.print(mode==LOOP?F("LOOP set="):F("MAN  set="));
    Serial.print(setMph,1); Serial.print(F(" meas=")); Serial.print(measMph,1);
    Serial.print(F("mph thr=")); Serial.print(curV,2); Serial.println(F("V"));
  }
}
