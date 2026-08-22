/*
 * STM32 F401RE — SNIFFER 2 BUS (ecoute seule), horodate, decode
 *
 * BUT : observer SIMULTANEMENT les deux liaisons serie 9600 pour comprendre pourquoi le moteur
 * ne tourne pas. On NE DRIVE rien : pure ecoute.
 *   - VIOLET (afficheur -> controleur) = bus de COMMANDES (header 0x01, 20 o, frein/gear/feux + XOR)
 *   - BLEU   (controleur -> afficheur) = bus de TELEMETRIE/ETAT (header 0x02) : c'est le controleur
 *     qui parle -> s'il est en faute (Hall, surintensite, latch securite), ca se voit ICI.
 *
 * CABLAGE (ecoute, ne perturbe pas les bus) :
 *   PA9 (= D8) <- VIOLET cote afficheur   (USART1 half-duplex RX)
 *   PC7 (= D9) <- BLEU (derivation)        (USART6 half-duplex RX)
 *   GND <- noir (commun)
 *   roue surelevee. jamais rouge/orange 52V.
 *
 * Framing par SILENCE inter-octet (>4 ms = nouvelle trame). Sortie : [t_ms] BUS len: hex... (decode)
 * Commande : 'c' = efface les compteurs / re-synchronise l'affichage.
 */

HardwareSerial violetUart(PA9);   // USART1 1-fil <- afficheur (violet)
HardwareSerial blueUart(PC7, PA11); // USART6 RX=PC7 <- bleu ; TX=PA11 NON CONNECTE (evite de driver le violet-ctrl 5V)

#define GAP_US 4000               // silence = fin de trame
#define MAXF 64

struct Sniff {
  const char* tag;
  uint8_t buf[MAXF]; uint8_t len;
  unsigned long lastByteUs;
  HardwareSerial* u;
};
Sniff V = {"VIOLET", {0},0,0,&violetUart};
Sniff B = {"BLEU  ", {0},0,0,&blueUart};

void printFrame(Sniff& s){
  if (s.len==0) return;
  Serial.print("["); Serial.print(millis()); Serial.print("] ");
  Serial.print(s.tag); Serial.print(" "); Serial.print(s.len); Serial.print("o:");
  for (uint8_t i=0;i<s.len;i++){ Serial.print(s.buf[i]<16?" 0":" "); Serial.print(s.buf[i],HEX); }
  // decode violet connu
  if (s.buf[0]==0x01 && s.len>=20){
    uint8_t f=s.buf[4];
    Serial.print("  | frein="); Serial.print((f&0x80)?"ON":"off");
    Serial.print(" gear="); Serial.print(f&0x0F);
    Serial.print(" mode="); Serial.print((f&0x10)?"D":"S");
    Serial.print(" feu="); Serial.print((s.buf[5]&0x20)?"ON":"off");
    uint8_t x=0; for(uint8_t i=0;i<19;i++) x^=s.buf[i];
    Serial.print(" xor="); Serial.print(x==s.buf[19]?"ok":"BAD");
  }
  Serial.println();
  s.len=0;
}

void pump(Sniff& s){
  unsigned long now=micros();
  while (s.u->available()){
    uint8_t b=s.u->read();
    if (s.len && (now - s.lastByteUs) > GAP_US) printFrame(s);  // gap -> flush trame precedente
    if (s.len < MAXF) s.buf[s.len++]=b;
    s.lastByteUs=now; now=micros();
  }
  // flush si silence depuis la derniere trame
  if (s.len && (micros() - s.lastByteUs) > GAP_US) printFrame(s);
}

void setup(){
  Serial.begin(115200);
  violetUart.setHalfDuplex(); violetUart.begin(9600); violetUart.enableHalfDuplexRx();
  blueUart.begin(9600);   // lecture normale RX sur PC7
  delay(200);
  Serial.println("\n=== SNIFFER 2 BUS : VIOLET(PA9/D8 afficheur) + BLEU(PC7/D9 controleur) ===");
  Serial.println("ecoute seule. power-cycle le scooter pour capturer le boot. 'c'=clear");
}

void loop(){
  pump(V); pump(B);
  if (Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if (c=="c"){ Serial.println("--- clear ---"); }
  }
}
