/*
 * STM32 F401RE — ÉMULATEUR D'AFFICHEUR sur le violet (le STM32 remplace l'afficheur)
 *
 * BUT : commander frein + feux + gear DEPUIS LA CARTE, comme on commande déjà l'accélération.
 * Le violet est un bus numérique push-pull : impossible d'injecter en parallèle. La voie propre =
 * SUPPRIMER l'afficheur et faire générer les trames de commande par le STM32 (format connu).
 * Plus d'afficheur = plus de E07. On vérifie ici si le contrôleur tourne avec nos seules trames
 * violet (open-drain 5V, 10/s) + le throttle analogique, SANS afficheur.
 *
 * CABLAGE :
 *   PC6  (= D10) -> VIOLET côté CONTRÔLEUR : USART6 1-fil OPEN-DRAIN (pull-up 5V interne du contrôleur)
 *   PB10 (= D6)  -> R1k -> gris/blanc : PWM throttle (accélération).  PA0 (= A0) <- nœud.  GND <- noir.
 *   >>> DÉBRANCHER l'afficheur du contrôleur AUSSI sur le BLEU (on teste sans afficheur du tout).
 *   roue SURÉLEVÉE ; jamais sur rouge/orangé 52V.
 *
 * COMMANDES (Serial 115200) :
 *   f/r = frein ON/OFF (bit 0x80 octet[4])
 *   1/2/3 = gear (nibble bas octet[4] = 5/A/F)
 *   L/l = feu route ON/OFF (bit 0x20 octet[5])
 *   m<v> = throttle volts   x/0 = throttle 0
 */

HardwareSerial ctrlUart(PC6);   // USART6 1-fil OPEN-DRAIN -> contrôleur

#define THR_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3
#define PERIOD_MS 100           // 10 trames/s comme l'afficheur

// trame de repos observée de l'afficheur (octet[19]=checksum, recalculé à chaque envoi)
uint8_t frame[20] = {0x01,0x14,0x01,0x00,0x0F,0x80,0x1E,0x00,0x91,0x01,0x05,0x00,0x64,0x0C,0x01,0xAE,0x00,0x00,0x05,0x00};

bool brake=false, headlight=false; int gear=3; float thrCmd=0;
unsigned long framesSent=0, framesAccum=0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

void sendFrame(){
  frame[4] = 0x00;
  frame[4] |= (gear==1)?0x05:(gear==2)?0x0A:0x0F;   // nibble gear
  if (brake) frame[4] |= 0x80;
  frame[5] = headlight ? (0x80|0x20) : 0x80;
  uint8_t x=0; for(uint8_t i=0;i<19;i++) x^=frame[i]; frame[19]=x;  // checksum XOR [0..18]
  ctrlUart.write(frame, 20);
  framesAccum++;
}

void setup(){
  Serial.begin(115200);
  ctrlUart.setHalfDuplex(); ctrlUart.begin(9600);   // open-drain, niveau haut = pull-up 5V contrôleur
  analogReadResolution(12);
  pinMode(THR_PIN,OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== EMULATEUR AFFICHEUR (violet synthetique -> contrôleur PC6/D10, 10/s) ===");
  Serial.println("f/r=frein 1/2/3=gear L/l=feu m<v>=throttle x=stop");
}

unsigned long lastSend=0, lastPrint=0;
void loop(){
  unsigned long now=millis();
  if (now-lastSend>=PERIOD_MS){ lastSend=now; sendFrame(); }

  if (Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if      (c=="f"){ brake=true;  Serial.println("[cmd] FREIN ON"); }
    else if (c=="r"){ brake=false; Serial.println("[cmd] frein OFF"); }
    else if (c=="1"||c=="2"||c=="3"){ gear=c.toInt(); Serial.print("[cmd] gear "); Serial.println(c); }
    else if (c=="L"){ headlight=true;  Serial.println("[cmd] FEU ON"); }
    else if (c=="l"){ headlight=false; Serial.println("[cmd] feu OFF"); }
    else if (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }

  if (now-lastPrint>=1000){
    lastPrint=now; framesSent=framesAccum; framesAccum=0;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("emul="); Serial.print(framesSent); Serial.print("/s | frein="); Serial.print(brake?"ON":"off");
    Serial.print(" gear="); Serial.print(gear); Serial.print(" feu="); Serial.print(headlight?"ON":"off");
    Serial.print(" thr="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.println("V");
  }
}
