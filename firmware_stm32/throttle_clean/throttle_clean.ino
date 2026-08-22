/*
 * THROTTLE CLEAN (STM32 F401RE) — port fidele de throttle_injector_uno
 * Scooter Ecoxtrem M41 / ESC CHK2-K1-03.  REPART DE LA BASE QUI MARCHE.
 *
 * TOPOLOGIE ORIGINALE (celle qui fait tourner la roue) :
 *   - gris/blanc THROTTLE : COUPE, injecte cote CONTROLEUR (PB10 -> R1k -> noeud ; C 10uF/GND ; PA0 lit le noeud)
 *   - bleu : TAPPE en parallele (PAS coupe) pour la telemetrie  [non lu ici]
 *   - VIOLET : LAISSE BRANCHE en direct afficheur<->controleur (NE PAS couper !) -> arme le controleur
 *   - rouge/orange(52V)/noir : laisses tels quels. GND Nucleo <-> noir.
 *   roue SURELEVEE. jamais rouge/orange 52V.
 *
 * IMPORTANT : repos = 0,80 V (jamais 0 V : une poignee Hall ne descend pas sous ~0,8 V, 0 V = faute possible).
 *             Les tensions sont en VOLTS AU NOEUD (= ce que voit le controleur). RATIO compense le diviseur RC.
 *
 * COMMANDES (115200) : nombre=cible(V) | u/d=+-0.1 | n=repos | x=ARRET(repos) | ?=etat | s1/s2/s3=scenarios
 */

#define PWM_PIN   PB10
#define NODE_PIN  PA0
#define VREF      3.3
#define RATIO     0.85      // noeud/PWM mesure (~0.82-0.88) ; ajuster si node affiche != cible
#define IDLE_V    0.80      // repos throttle (au noeud)
#define MAX_V     2.00      // plafond de securite (relever progressivement vers ~3.0)
#define ABS_MAX_V 3.20      // garde-fou dur
#define RAMP_VPS  0.50      // rampe (V/s)

float targetV=IDLE_V, curV=IDLE_V;
unsigned long lastStep=0, lastPrint=0;

struct Step { float v; unsigned long holdMs; };
Step SCN_RAMP[]  = {{1.6,3000},{IDLE_V,0}};
Step SCN_STEPS[] = {{1.0,2000},{1.4,2000},{1.8,2000},{IDLE_V,0}};
Step SCN_DRIVE[] = {{1.6,2500},{1.0,1500},{1.9,2500},{1.2,1500},{IDLE_V,0}};
Step* scn=0; int scnLen=0, scnIdx=0; bool holding=false; unsigned long holdStart=0;

void applyNode(float v){                       // v = tension voulue AU NOEUD
  if(v<0)v=0; if(v>ABS_MAX_V)v=ABS_MAX_V;
  float pwmV=v/RATIO; if(pwmV>VREF)pwmV=VREF;   // compense le diviseur
  analogWrite(PWM_PIN,(int)(pwmV/VREF*255.0+0.5));
}
void setTarget(float v){ if(v<IDLE_V)v=IDLE_V; if(v>MAX_V)v=MAX_V; targetV=v; Serial.print(F("Cible=")); Serial.print(targetV,2); Serial.println(F("V")); }
void startScn(Step* s,int len,const __FlashStringHelper* n){ scn=s; scnLen=len; scnIdx=0; holding=false; Serial.print(F(">>> SCN: ")); Serial.println(n); setTarget(scn[0].v); }
void stopScn(const __FlashStringHelper* w){ scn=0; curV=targetV=IDLE_V; applyNode(IDLE_V); Serial.print(F("*** ")); Serial.print(w); Serial.println(F(" -> repos ***")); }

void setup(){
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(PWM_PIN,OUTPUT);
  applyNode(IDLE_V);
  delay(800);
  Serial.println(F("\n=== THROTTLE CLEAN (STM32) — repos 0.80V, rampe douce ==="));
  Serial.println(F("nombre=cible | u/d | n=repos | x=ARRET | s1/s2/s3 | ?=etat"));
}

void loop(){
  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if      (s=="x")  stopScn(F("ARRET"));
    else if (s=="s1") startScn(SCN_RAMP, sizeof(SCN_RAMP)/sizeof(Step), F("rampe douce"));
    else if (s=="s2") startScn(SCN_STEPS,sizeof(SCN_STEPS)/sizeof(Step),F("paliers"));
    else if (s=="s3") startScn(SCN_DRIVE,sizeof(SCN_DRIVE)/sizeof(Step),F("cycle"));
    else if (s=="n")  { scn=0; setTarget(IDLE_V); }
    else if (s=="u")  { scn=0; setTarget(targetV+0.1); }
    else if (s=="d")  { scn=0; setTarget(targetV-0.1); }
    else if (s=="?")  { Serial.print(F("cur=")); Serial.print(curV,2); Serial.print(F("V cible=")); Serial.print(targetV,2); Serial.print(F("V scn=")); Serial.println(scn?F("oui"):F("non")); }
    else if (s.length()){ float v=s.toFloat(); if(v>0){ scn=0; setTarget(v); } }
  }
  unsigned long now=millis();
  if(now-lastStep>=20){
    float step=RAMP_VPS*0.020;
    if(curV<targetV){ curV+=step; if(curV>targetV)curV=targetV; }
    else if(curV>targetV){ curV-=step; if(curV<targetV)curV=targetV; }
    applyNode(curV); lastStep=now;
    if(scn){
      float diff=curV-scn[scnIdx].v; if(diff<0)diff=-diff;
      if(!holding){ if(diff<0.02){ holding=true; holdStart=now; } }
      else if(now-holdStart>=scn[scnIdx].holdMs){ scnIdx++; if(scnIdx>=scnLen){ scn=0; Serial.println(F(">>> SCN termine")); } else { holding=false; setTarget(scn[scnIdx].v); } }
    }
  }
  if(now-lastPrint>=1000){
    lastPrint=now;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print(F("cur=")); Serial.print(curV,2); Serial.print(F("V  node_mesure=")); Serial.print(node,2); Serial.print(F("V  cible=")); Serial.print(targetV,2); Serial.println(F("V"));
  }
}
