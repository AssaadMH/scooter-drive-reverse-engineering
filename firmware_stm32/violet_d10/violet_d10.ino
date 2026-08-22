/*
 * VIOLET D10 (STM32 F401RE) — injecte la trame violet sur le BON pin : D10 = PB6 (PAS PC6 !).
 * + throttle sur D6. Commandes frein/feu/mode/gear. Trame synthetique 10/s open-drain 5V.
 *
 *   D10 (PB6) -> violet CONTROLEUR : USART1 half-duplex 1-fil = TX OPEN-DRAIN (haut=pull-up 5V ctrl)
 *   D6  (PB10)-> R1k -> gris/blanc throttle    A0 (PA0) <- noeud    GND <- noir
 *   afficheur-violet DEBRANCHE du controleur (on est seul a parler sur D10).
 *   batterie allumee, roue surelevee. jamais rouge/orange 52V. repos throttle 0.8V.
 *
 * COMMANDES : f/r=frein  L/l=feu  D/S=mode  1/2/3=gear  m<v>=throttle  x=repos
 */

HardwareSerial ctrlUart(PB6);   // D10, open-drain vers le controleur

#define PWM_PIN PB10            // D6
#define NODE_PIN PA0           // A0
#define VREF 3.3
#define RATIO 0.85
#define IDLE_V 0.80

bool brake=false, feu=false, modeD=false, cligG=false, cligD=false;
int gear=3;
float curV=IDLE_V, targetV=IDLE_V;
unsigned long lastFrame=0, lastStep=0, lastPrint=0;

void applyNode(float v){ if(v<0)v=0; if(v>3.2)v=3.2; float p=v/RATIO; if(p>VREF)p=VREF; analogWrite(PWM_PIN,(int)(p/VREF*255.0+0.5)); }

void sendFrame(){
  uint8_t f[20]={0x01,0x14,0x01,0x00,0x0F,0x80,0x1E,0x00,0x91,0x01,0x05,0x00,0x64,0x0C,0x01,0xAE,0x00,0x00,0x05,0x00};
  uint8_t g=(gear==1)?0x05:(gear==2)?0x0A:0x0F;
  f[4]=g | (brake?0x80:0) | (modeD?0x10:0);
  f[5]=0x80 | (feu?0x20:0);
  f[18]=0x05 | (cligD?0x08:0) | (cligG?0x10:0);
  uint8_t x=0; for(uint8_t i=0;i<19;i++) x^=f[i]; f[19]=x;
  for(uint8_t i=0;i<20;i++) ctrlUart.write(f[i]);
}

void setup(){
  Serial.begin(115200);
  ctrlUart.setHalfDuplex(); ctrlUart.begin(9600);   // open-drain sur D10/PB6
  analogReadResolution(12);
  pinMode(PWM_PIN,OUTPUT); applyNode(IDLE_V);
  delay(300);
  Serial.println(F("\n=== VIOLET D10 (PB6) : injection sur le BON pin + throttle ==="));
  Serial.println(F("f/r=frein L/l=feu D/S=mode 1/2/3=gear m<v>=throttle x=repos"));
}

void loop(){
  unsigned long now=millis();
  if(now-lastFrame>=100){ lastFrame=now; sendFrame(); }
  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if      (s=="f") brake=true;  else if(s=="r") brake=false;
    else if (s=="L") feu=true;    else if(s=="l") feu=false;
    else if (s=="D") modeD=true;  else if(s=="S") modeD=false;
    else if (s=="1") gear=1; else if(s=="2") gear=2; else if(s=="3") gear=3;
    else if (s=="g") cligG=true; else if(s=="G") cligG=false;
    else if (s=="h") cligD=true; else if(s=="H") cligD=false;
    else if (s=="x"||s=="n") targetV=IDLE_V;
    else if (s.length()&&s[0]=='m'){ targetV=s.substring(1).toFloat(); if(targetV<IDLE_V)targetV=IDLE_V; if(targetV>2.2)targetV=2.2; }
    else if (s.length()){ float v=s.toFloat(); if(v>0){ targetV=v; if(targetV>2.2)targetV=2.2; } }
    Serial.print(F("[cmd] frein=")); Serial.print(brake?"ON":"off"); Serial.print(F(" feu=")); Serial.print(feu?"ON":"off");
    Serial.print(F(" mode=")); Serial.print(modeD?"D":"S"); Serial.print(F(" gear=")); Serial.print(gear); Serial.print(F(" cible=")); Serial.print(targetV,2); Serial.println(F("V"));
  }
  if(now-lastStep>=20){ float st=0.5*0.020; if(curV<targetV){curV+=st;if(curV>targetV)curV=targetV;} else if(curV>targetV){curV-=st;if(curV<targetV)curV=targetV;} applyNode(curV); lastStep=now; }
  if(now-lastPrint>=1000){ lastPrint=now; float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print(F(">> frein=")); Serial.print(brake?"ON ":"off"); Serial.print(F("feu=")); Serial.print(feu?"ON ":"off");
    Serial.print(F("mode=")); Serial.print(modeD?"D":"S"); Serial.print(F(" gear=")); Serial.print(gear);
    Serial.print(F(" cligG=")); Serial.print(cligG?"ON":"--"); Serial.print(F(" cligD=")); Serial.print(cligD?"ON":"--");
    Serial.print(F(" thr=")); Serial.print(curV,2); Serial.print(F(" node=")); Serial.print(node,2); Serial.println(F("V")); }
}
