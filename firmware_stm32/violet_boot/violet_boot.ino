/*
 * STM32 F401RE — SNIFFER VIOLET AU BOOT : capture la sequence d'ARMEMENT de l'afficheur
 *
 * Ecoute le violet cote afficheur (PA9) et N'AFFICHE QUE LES TRAMES QUI CHANGENT (vs la precedente
 * affichee). En regime etabli tout est la trame de repos (01 14 ... D2) -> 1 ligne puis silence.
 * Au boot, si l'afficheur emet une SEQUENCE D'ARMEMENT (trames differentes), elles ressortent toutes,
 * horodatees. But : trouver ce que le vrai afficheur envoie au demarrage que notre relais/emul ne fait pas.
 *
 *   PA9 (= D8) <- VIOLET cote afficheur (USART1 half-duplex RX). GND <- noir.
 *
 * MODE D'EMPLOI : STM deja branche/tourne -> couper batterie -> rallumer batterie + bouton afficheur
 *   PENDANT la capture. Les 1res trames du boot sont capturees.
 */

HardwareSerial violetUart(PA9);
#define GAP_US 4000
#define MAXF 40

uint8_t buf[MAXF], len=0;
uint8_t lastShown[MAXF]; uint8_t lastLen=0;
unsigned long lastByteUs=0;
const uint8_t REST[20]={0x01,0x14,0x01,0x00,0x0F,0x80,0x1E,0x00,0x91,0x01,0x05,0x00,0x64,0x0C,0x01,0xAE,0x00,0x00,0x05,0xD2};

bool sameAsLast(){ if(len!=lastLen) return false; for(uint8_t i=0;i<len;i++) if(buf[i]!=lastShown[i]) return false; return true; }
bool isRest(){ if(len!=20) return false; for(uint8_t i=0;i<20;i++) if(buf[i]!=REST[i]) return false; return true; }

void showFrame(){
  if(len==0) return;
  if(sameAsLast()){ len=0; return; }                 // identique a la derniere affichee -> on tait
  Serial.print("["); Serial.print(millis()); Serial.print("] ");
  Serial.print(len); Serial.print("o:");
  for(uint8_t i=0;i<len;i++){ Serial.print(buf[i]<16?" 0":" "); Serial.print(buf[i],HEX); }
  if(isRest()) Serial.print("   <= trame de REPOS");
  else         Serial.print("   *** DIFFERENTE (armement?) ***");
  Serial.println();
  memcpy(lastShown,buf,len); lastLen=len; len=0;
}

void setup(){
  Serial.begin(115200);
  violetUart.setHalfDuplex(); violetUart.begin(9600); violetUart.enableHalfDuplexRx();
  delay(100);
  Serial.println("\n=== SNIFFER VIOLET BOOT (PA9) : n'affiche que les CHANGEMENTS ===");
  Serial.println("power-cycle batterie + bouton afficheur PENDANT la capture.");
}

void loop(){
  unsigned long now=micros();
  while(violetUart.available()){
    uint8_t b=violetUart.read();
    if(len && (now-lastByteUs)>GAP_US) showFrame();
    if(len<MAXF) buf[len++]=b;
    lastByteUs=now; now=micros();
  }
  if(len && (micros()-lastByteUs)>GAP_US) showFrame();
}
