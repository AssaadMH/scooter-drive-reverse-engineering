/*
 * BUS WATCH (STM32 F401RE) — lit VIOLET + BLEU (taps) + injecte THROTTLE. Violet reste NATIF (joint direct).
 * But : voir ce qui se passe sur les 2 bus pendant que E02 clignote, et tenter de faire tourner la roue.
 *
 *   PC7 (= D9) <- TAP violet natif   (USART6 RX, TX=PA11 NC)
 *   PA9 (= D8) <- TAP bleu           (USART1 half-duplex RX)
 *   PB10(= D6) -> R1k -> gris/blanc throttle   PA0(=A0) <- noeud   GND <- noir
 *   roue surelevee. jamais rouge/orange 52V. repos throttle 0.8V (jamais 0).
 *
 * Affiche chaque seconde : taux violet/bleu + decode. ET imprime toute trame qui CHANGE (horodatee)
 * -> le clignotement E02 doit correspondre a un changement (violet qui tombe ? octet bleu qui bascule ?).
 * COMMANDES : nombre=cible(V noeud) | n=repos 0.8 | x=repos | u/d=+-0.1
 */

HardwareSerial violetU(PC7, PA11);   // tap violet
HardwareSerial blueU(PA9);           // tap bleu (half-duplex RX)

#define PWM_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3
#define RATIO 0.85
#define IDLE_V 0.80
#define MAX_V 2.20

float targetV=IDLE_V, curV=IDLE_V;
unsigned long lastStep=0, lastPrint=0;

struct Bus { const char* tag; HardwareSerial* u; uint8_t buf[40],len; uint8_t shown[40],slen; unsigned long lastUs,cnt; };
Bus V={"VIOLET",&violetU,{0},0,{0},0,0,0};
Bus B={"BLEU  ",&blueU,  {0},0,{0},0,0,0};

void applyNode(float v){ if(v<0)v=0; if(v>3.2)v=3.2; float p=v/RATIO; if(p>VREF)p=VREF; analogWrite(PWM_PIN,(int)(p/VREF*255.0+0.5)); }
void setTarget(float v){ if(v<IDLE_V)v=IDLE_V; if(v>MAX_V)v=MAX_V; targetV=v; Serial.print(F("Cible=")); Serial.print(targetV,2); Serial.println(F("V")); }

bool diff(Bus& s){ if(s.len!=s.slen) return true; for(uint8_t i=0;i<s.len;i++) if(s.buf[i]!=s.shown[i]) return true; return false; }
void onFrame(Bus& s){
  s.cnt++;
  if(diff(s)){
    Serial.print(F("[")); Serial.print(millis()); Serial.print(F("] ")); Serial.print(s.tag); Serial.print(F(" chg:"));
    for(uint8_t i=0;i<s.len;i++){ Serial.print(s.buf[i]<16?" 0":" "); Serial.print(s.buf[i],HEX); }
    Serial.println();
    memcpy(s.shown,s.buf,s.len); s.slen=s.len;
  }
  s.len=0;
}
void pump(Bus& s){
  unsigned long now=micros();
  while(s.u->available()){
    uint8_t b=s.u->read();
    if(s.len && (now-s.lastUs)>4000) onFrame(s);
    if(s.len<40) s.buf[s.len++]=b;
    s.lastUs=now; now=micros();
  }
  if(s.len && (micros()-s.lastUs)>4000) onFrame(s);
}

void setup(){
  Serial.begin(115200);
  violetU.begin(9600);
  blueU.setHalfDuplex(); blueU.begin(9600); blueU.enableHalfDuplexRx();
  analogReadResolution(12);
  pinMode(PWM_PIN,OUTPUT); applyNode(IDLE_V);
  delay(500);
  Serial.println(F("\n=== BUS WATCH : violet(PC7)+bleu(PA9)+throttle. repos 0.8V ==="));
  Serial.println(F("nombre=cible | n=repos | x=repos | u/d"));
}

void loop(){
  pump(V); pump(B);
  if(Serial.available()){
    String s=Serial.readStringUntil('\n'); s.trim();
    if(s=="x"||s=="n") setTarget(IDLE_V);
    else if(s=="u") setTarget(targetV+0.1);
    else if(s=="d") setTarget(targetV-0.1);
    else if(s.length()){ float v=s.toFloat(); if(v>0) setTarget(v); }
  }
  unsigned long now=millis();
  if(now-lastStep>=20){ float st=0.5*0.020; if(curV<targetV){curV+=st; if(curV>targetV)curV=targetV;} else if(curV>targetV){curV-=st; if(curV<targetV)curV=targetV;} applyNode(curV); lastStep=now; }
  if(now-lastPrint>=1000){
    lastPrint=now;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    unsigned int per=(B.slen>=10&&B.shown[0]==0x02)?((B.shown[8]<<8)|B.shown[9]):0;
    uint8_t vf=(V.slen>=5&&V.shown[0]==0x01)?V.shown[4]:0;
    Serial.print(F(">> violet=")); Serial.print(V.cnt); Serial.print(F("/s(frein="));
    Serial.print((vf&0x80)?"ON":"off"); Serial.print(F(" gear=")); Serial.print(vf&0x0F);
    Serial.print(F(") bleu=")); Serial.print(B.cnt); Serial.print(F("/s(periode=")); Serial.print(per);
    Serial.print(F(") thr=")); Serial.print(curV,2); Serial.print(F(" node=")); Serial.print(node,2); Serial.println(F("V"));
    V.cnt=0; B.cnt=0;
  }
}
