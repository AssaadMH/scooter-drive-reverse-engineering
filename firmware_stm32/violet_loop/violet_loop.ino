/*
 * VIOLET LOOP v4 (STM32 F401RE) — pins HEADER Arduino du Nucleo.
 * D10 = PB6 (USART1_TX) ; D9 = PC7 (USART6_RX). PC6 n'existe PAS sur le header -> on utilise D10.
 *
 *   D10 (PB6) -> TX trame synthetique (push-pull, auto-test)
 *   D9  (PC7) <- RX loopback (half-duplex)        CAVALIER : D10 -> D9
 *
 * Si OK -> notre TX sort bien sur D10 (= la ou est branche le fil controleur).
 */

HardwareSerial txU(PB7, PB6);  // USART1 : tx=PB6=D10 push-pull, rx=PB7 NC
HardwareSerial rxU(PC7);       // USART6 half-duplex RX = D9

uint8_t sent[20];
uint8_t rbuf[40], rlen=0; unsigned long rLastUs=0;
unsigned long okCnt=0, badCnt=0, lastTx=0, lastPrint=0;

void buildFrame(){
  uint8_t f[20]={0x01,0x14,0x01,0x00,0x0F,0x80,0x1E,0x00,0x91,0x01,0x05,0x00,0x64,0x0C,0x01,0xAE,0x00,0x00,0x05,0x00};
  uint8_t x=0; for(uint8_t i=0;i<19;i++) x^=f[i]; f[19]=x; memcpy(sent,f,20);
}
void checkFrame(){
  if(rlen==20){ bool ok=true; for(uint8_t i=0;i<20;i++) if(rbuf[i]!=sent[i]){ok=false;break;}
    if(ok) okCnt++; else { badCnt++; Serial.print(F("[BAD]:")); for(uint8_t i=0;i<rlen;i++){Serial.print(rbuf[i]<16?" 0":" ");Serial.print(rbuf[i],HEX);} Serial.println(); }
  } else { badCnt++; Serial.print(F("[len=")); Serial.print(rlen); Serial.print(F("]:")); for(uint8_t i=0;i<rlen;i++){Serial.print(rbuf[i]<16?" 0":" ");Serial.print(rbuf[i],HEX);} Serial.println(); }
  rlen=0;
}
void setup(){
  Serial.begin(115200);
  txU.begin(9600);
  rxU.setHalfDuplex(); rxU.begin(9600); rxU.enableHalfDuplexRx();
  buildFrame(); delay(300);
  Serial.println(F("\n=== VIOLET LOOP v4 : TX=D10(PB6) -> RX=D9(PC7). cavalier D10->D9 ==="));
  Serial.print(F("envoyee:")); for(uint8_t i=0;i<20;i++){Serial.print(sent[i]<16?" 0":" ");Serial.print(sent[i],HEX);} Serial.println();
}
void loop(){
  unsigned long now=millis();
  if(now-lastTx>=100){ lastTx=now; for(uint8_t i=0;i<20;i++) txU.write(sent[i]); }
  unsigned long us=micros();
  while(rxU.available()){ uint8_t b=rxU.read(); if(rlen && (us-rLastUs)>4000) checkFrame(); if(rlen<40) rbuf[rlen++]=b; rLastUs=us; us=micros(); }
  if(rlen && (micros()-rLastUs)>4000) checkFrame();
  if(now-lastPrint>=1000){ lastPrint=now; Serial.print(F(">> OK=")); Serial.print(okCnt); Serial.print(F(" BAD=")); Serial.println(badCnt); okCnt=0; badCnt=0; }
}
