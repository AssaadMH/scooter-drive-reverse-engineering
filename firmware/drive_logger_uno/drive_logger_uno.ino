/*
 * DRIVE + LOGGER — pilote le throttle ET log (vitesse UART + flag frein + violet)
 * Scooter Ecoxtrem M41 Tank Dual / ESC CHK2-K1-03   |  Arduino Uno R3 (5V)
 *
 * BUT : faire TOURNER la roue (throttle commandé par l'Arduino) puis observer le FREIN
 *       en dynamique : chute de vitesse / régen, flag UART, comportement du fil violet.
 *
 * CABLAGE (A0/violet retiré — config simplifiée)
 *   D11 --[R 1k]--+-- fil GRIS/BLANC (cote controleur, COUPÉ)   [throttle, PWM] -> fait tourner la roue
 *                 +--[C 10uF]-- GND
 *   D10 --------- fil BLEU      (UART 9600, tap // — lecture vitesse)
 *   D8  --------- non connecté  (TX factice SoftwareSerial ; PAS D11 qui sert au PWM)
 *   A1  --------- nœud GRIS/BLANC (relecture de notre tension throttle injectée)
 *   GND --------- fil NOIR
 *   ☠️ rien sur rouge/orangé (52 V).  ROUE SURÉLEVÉE.
 *
 * COMMANDES (115200) :
 *   un nombre ex "1.5" -> throttle = 1.5 V (rampe douce) ; n = repos ; x = ARRÊT
 *
 * SORTIE (toutes ~150 ms) :
 *   t=12345 | thrCmd=1.50V A1=1.48V | spd=6.2mph | FREIN=NON | trame= ...
 */

#include <SoftwareSerial.h>

#define RX_PIN   10
#define TX_DUMMY 8
#define PWM_PIN  11
#define PIN_THR  A1
#define BUS_BAUD 9600
#define VREF     5.0
#define IDLE_V   0.80
#define MAX_V    2.20
#define ABS_MAX  3.40
#define CAL      1.00
#define SPEED_K  2520.0   // recalibre 2026-06-03 : 126->20mph, 360->7mph (afficheur). (ex-667, faux)
#define RAMP_VPS 0.50
#define PRINT_MS 150

SoftwareSerial busSerial(RX_PIN, TX_DUMMY);

float targetV = IDLE_V, curV = IDLE_V;
unsigned long lastStep = 0, lastPrint = 0;
uint8_t lastFrame[32]; uint8_t lastLen = 0;

void applyV(float v){
  if(v<0)v=0; if(v>ABS_MAX)v=ABS_MAX;
  int d=(int)((v/CAL)/VREF*255.0+0.5); if(d<0)d=0; if(d>255)d=255;
  analogWrite(PWM_PIN,d);
}
void setTarget(float v){ if(v<IDLE_V)v=IDLE_V; if(v>MAX_V)v=MAX_V; targetV=v;
  Serial.print(F("Cible=")); Serial.print(targetV,2); Serial.println(F("V")); }

void readBus(){
  static uint8_t buf[32]; static uint8_t idx=0,len=0;
  while(busSerial.available()){
    uint8_t b=busSerial.read();
    if(idx==0){ if(b==0x02) buf[idx++]=b; }
    else if(idx==1){ if(b>=4&&b<=32){buf[idx++]=b; len=b;} else idx=0; }
    else { buf[idx++]=b;
      if(idx>=len){ uint8_t x=0; for(uint8_t i=0;i<len-1;i++)x^=buf[i];
        if(x==buf[len-1]){ memcpy(lastFrame,buf,len); lastLen=len; } idx=0; len=0; } }
  }
}

void setup(){
  Serial.begin(115200);
  pinMode(PWM_PIN,OUTPUT); applyV(IDLE_V);
  busSerial.begin(BUS_BAUD);
  delay(800);
  Serial.println(F("\n=== DRIVE + LOGGER === (commande throttle + log vitesse/frein/violet)"));
  Serial.println(F("nombre=throttle V | n=repos | x=ARRET. ROUE EN L'AIR."));
}

void loop(){
  readBus();

  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if(s=="x"){ targetV=curV=IDLE_V; applyV(IDLE_V); Serial.println(F("*** ARRET ***")); }
    else if(s=="n") setTarget(IDLE_V);
    else if(s.length()){ float f=s.toFloat(); if(f>0) setTarget(f); }
  }

  unsigned long now=millis();
  if(now-lastStep>=20){ lastStep=now;
    float st=RAMP_VPS*0.020;
    if(curV<targetV){curV+=st; if(curV>targetV)curV=targetV;}
    else if(curV>targetV){curV-=st; if(curV<targetV)curV=targetV;}
    applyV(curV);
  }

  if(now-lastPrint>=PRINT_MS){ lastPrint=now;
    int thr=analogRead(PIN_THR); float thrv=thr*VREF/1023.0;
    float mph=0; bool brake=false;
    if(lastLen>=13){ brake=(lastFrame[4]==0xA0);
      if(lastFrame[8]!=0xEA){ uint16_t per=((uint16_t)lastFrame[8]<<8)|lastFrame[9]; if(per>=10) mph=SPEED_K/(float)per; } }
    Serial.print(F("t=")); Serial.print(now);
    Serial.print(F(" | thrCmd=")); Serial.print(curV,2); Serial.print(F("V A1=")); Serial.print(thrv,2); Serial.print('V');
    Serial.print(F(" | spd=")); Serial.print(mph,1); Serial.print(F("mph"));
    Serial.print(F(" | FREIN=")); Serial.print(brake?F("OUI"):F("NON"));
    Serial.print(F(" | trame="));
    if(lastLen){ for(uint8_t i=0;i<lastLen;i++){ if(lastFrame[i]<0x10)Serial.print('0'); Serial.print(lastFrame[i],HEX); Serial.print(' '); } } else Serial.print(F("(aucune)"));
    Serial.println();
  }
}
