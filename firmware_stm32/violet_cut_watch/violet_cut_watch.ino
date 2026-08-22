/*
 * VIOLET CUT WATCH (STM32 F401RE) — violet COUPE, on ecoute les 2 bouts pour voir le "va-et-vient"
 * et decoder les trames. Pure observation (throttle tenu au repos 0.8V, ne touche a rien d'autre).
 *
 *   PC7 (= D9) <- bout AFFICHEUR du violet coupe   (USART6 RX, TX=PA11 NC)
 *   PA9 (= D8) <- bout CONTROLEUR du violet coupe  (USART1 half-duplex RX)
 *   PB10(= D6) -> throttle (repos 0.8V)            PA0(=A0) <- noeud   GND <- noir
 *   roue surelevee. jamais rouge/orange 52V.
 *
 * Affiche chaque seconde le taux de chaque bout + decode (frein/gear/feu) ; imprime toute trame
 * qui CHANGE, taggee AFFICH / CTRL, horodatee -> montre qui parle et quoi.
 *
 * RECUP : rejoindre les 2 bouts du violet en direct -> scooter remarche.
 */

HardwareSerial vAff(PC7, PA11);   // bout afficheur
HardwareSerial vCtrl(PA9);        // bout controleur (half-duplex RX)

#define PWM_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3
#define RATIO 0.85
#define IDLE_V 0.80

struct Bus { const char* tag; HardwareSerial* u; uint8_t buf[40],len; uint8_t shown[40],slen; unsigned long lastUs,cnt; };
Bus A={"AFFICH",&vAff, {0},0,{0},0,0,0};
Bus C={"CTRL  ",&vCtrl,{0},0,{0},0,0,0};

void applyNode(float v){ if(v<0)v=0; if(v>3.2)v=3.2; float p=v/RATIO; if(p>VREF)p=VREF; analogWrite(PWM_PIN,(int)(p/VREF*255.0+0.5)); }

bool diff(Bus& s){ if(s.len!=s.slen) return true; for(uint8_t i=0;i<s.len;i++) if(s.buf[i]!=s.shown[i]) return true; return false; }
void onFrame(Bus& s){
  s.cnt++;
  if(diff(s)){
    Serial.print(F("[")); Serial.print(millis()); Serial.print(F("] ")); Serial.print(s.tag); Serial.print(F(" :"));
    for(uint8_t i=0;i<s.len;i++){ Serial.print(s.buf[i]<16?" 0":" "); Serial.print(s.buf[i],HEX); }
    if(s.len>=20&&s.buf[0]==0x01){ uint8_t f=s.buf[4];
      Serial.print(F("  frein=")); Serial.print((f&0x80)?"ON":"off");
      Serial.print(F(" gear=")); Serial.print((f&0x0F)/5); Serial.print(F(" feu=")); Serial.print((s.buf[5]&0x20)?"ON":"off"); }
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
  vAff.begin(9600);
  vCtrl.setHalfDuplex(); vCtrl.begin(9600); vCtrl.enableHalfDuplexRx();
  analogReadResolution(12);
  pinMode(PWM_PIN,OUTPUT); applyNode(IDLE_V);
  delay(400);
  Serial.println(F("\n=== VIOLET CUT WATCH : bout AFFICH(PC7) + bout CTRL(PA9) ==="));
  Serial.println(F("coupe le violet, ecoute les 2 bouts. throttle repos 0.8V."));
}

unsigned long lastPrint=0;
void loop(){
  pump(A); pump(C);
  unsigned long now=millis();
  if(now-lastPrint>=1000){
    lastPrint=now;
    Serial.print(F(">> AFFICH=")); Serial.print(A.cnt); Serial.print(F("/s  CTRL=")); Serial.print(C.cnt); Serial.println(F("/s"));
    A.cnt=0; C.cnt=0;
  }
}
