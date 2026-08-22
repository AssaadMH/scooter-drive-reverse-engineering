/*
 * STM32 F401RE — TEST VIOLET NATIF : violet rejoint en DIRECT (afficheur<->controleur),
 * le STM est HORS de la ligne violet. On TAP le violet sur PC7 pour confirmer qu'il circule,
 * et on injecte le throttle (gris/blanc). But : voir si le violet natif debloque le controleur.
 *
 *   PC7 (= D9)  <- TAP violet natif (lecture). USART6 RX=PC7, TX=PA11 non connecte.
 *   PB10(= D6)  -> R1k -> gris/blanc throttle   PA0(=A0) <- noeud   GND <- noir
 *   PA9/PC6 : plus rien (STM hors du violet). roue surelevee. jamais rouge/orange 52V.
 *
 * COMMANDES : m<v>=throttle  x=stop
 */

HardwareSerial violetRead(PC7, PA11);   // lecture du violet natif (tap)

#define THR_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3

float thrCmd=0;
uint8_t buf[40], len=0; uint8_t frame[40], flen=0; unsigned long lastByteUs=0; unsigned long vCnt=0;
uint8_t lastFlags=0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

void pumpViolet(){
  unsigned long now=micros();
  while(violetRead.available()){
    uint8_t b=violetRead.read();
    if(len && (now-lastByteUs)>4000){ memcpy(frame,buf,len); flen=len; if(flen>=5&&frame[0]==0x01){ lastFlags=frame[4]; vCnt++; } len=0; }
    if(len<40) buf[len++]=b;
    lastByteUs=now; now=micros();
  }
  if(len && (micros()-lastByteUs)>4000){ memcpy(frame,buf,len); flen=len; if(flen>=5&&frame[0]==0x01){ lastFlags=frame[4]; vCnt++; } len=0; }
}

void setup(){
  Serial.begin(115200);
  violetRead.begin(9600);
  analogReadResolution(12);
  pinMode(THR_PIN,OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== TEST VIOLET NATIF (violet direct + tap PC7 + throttle STM) ===");
  Serial.println("m<v>=throttle x=stop");
}

unsigned long lastPrint=0;
void loop(){
  pumpViolet();
  if(Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if(c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if(c.length()&&c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }
  unsigned long now=millis();
  if(now-lastPrint>=1000){
    lastPrint=now;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("violet_natif="); Serial.print(vCnt); Serial.print("/s frein=");
    Serial.print((lastFlags&0x80)?"ON":"off"); Serial.print(" gear="); Serial.print(lastFlags&0x0F);
    Serial.print(" | thr="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.print("V");
    if(vCnt==0) Serial.print("  [pas de violet sur PC7 : verifie le tap D9]");
    Serial.println();
    vCnt=0;
  }
}
