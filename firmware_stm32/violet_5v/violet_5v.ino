/*
 * STM32 F401RE — RELAIS CUT-THROUGH sur le violet, TX en OPEN-DRAIN 5V
 *
 * MESURE 2026-06-04 : la ligne violet côté CONTRÔLEUR idle à 5,05V (= pull-up vers le 5V interne
 * du contrôleur), l'afficheur émet en logique 5V (2,8-4,5V au multimètre). Notre TX push-pull 3,3V
 * (firmware violet_cutthrough) ne dépasse pas 3,3V => sous le seuil V_IH (~3,5V) d'une entrée 5V
 * => trames corrompues => E07 + moteur coupé.
 *
 * FIX : piloter PC6 en OPEN-DRAIN (mode half-duplex 1-fil du STM32). On tire à 0V pour un bit bas,
 * on RELÂCHE (haute-Z) pour un bit haut -> le pull-up 5V du contrôleur ramène à 5,05V tout propre,
 * exactement comme l'afficheur. PC6 est 5V-tolérant (FT) -> sûr d'être tiré à 5V par le contrôleur.
 * On garde la latence ~1 octet (cut-through) qui a éliminé l'hypothèse "synchro bleu".
 *
 * CABLAGE (inchangé) :
 *   PA9  (= D8)  <- VIOLET côté AFFICHEUR  : USART1 half-duplex RX (écoute l'afficheur)
 *   PC6  (= D10) -> VIOLET côté CONTRÔLEUR : USART6 half-duplex 1-fil = TX OPEN-DRAIN
 *   PB10 (= D6)  -> R1k -> gris/blanc : PWM throttle.   PA0 (= A0) <- nœud.   GND <- noir.
 *   (bleu direct afficheur<->contrôleur, non tappé)  roue SURÉLEVÉE.  Jamais sur rouge/orangé 52V.
 *
 * COMMANDES : f/r=frein  1/2/3=gear g=auto  m<v>=throttle x=stop
 */

HardwareSerial dashUart(PA9);   // USART1 1-fil <- afficheur (RX)
HardwareSerial ctrlUart(PC6);   // USART6 1-fil OPEN-DRAIN -> contrôleur (le pull-up 5V fait le niveau haut)

#define THR_PIN PB10
#define NODE_PIN PA0
#define VREF 3.3

bool forceBrake=false; int forceGear=0; float thrCmd=0;
unsigned long framesAccum=0, framesFwd=0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

// cut-through afficheur -> contrôleur, edition octet[4] + checksum au vol (TX open-drain)
void pump(){
  static uint8_t pos=0, x=0;
  while (dashUart.available()){
    uint8_t b = dashUart.read();
    if (pos==0){ if (b==0x01){ ctrlUart.write(b); x=b; pos=1; } continue; }      // header
    if (pos==1){ if (b==0x14){ ctrlUart.write(b); x^=b; pos=2; } else pos=0; continue; } // longueur
    if (pos==4){                                                                  // octet de flags
      if (forceBrake) b|=0x80;
      if (forceGear){ uint8_t nib=(forceGear==1)?0x05:(forceGear==2)?0x0A:0x0F; b=(b&0xF0)|nib; }
    }
    if (pos<19){ ctrlUart.write(b); x^=b; pos++; }                                // data
    else { ctrlUart.write(x); pos=0; framesAccum++; }                             // pos19 = checksum -> le notre
  }
}

void setup(){
  Serial.begin(115200);
  dashUart.setHalfDuplex(); dashUart.begin(9600); dashUart.enableHalfDuplexRx();
  ctrlUart.setHalfDuplex(); ctrlUart.begin(9600);   // 1-fil = open-drain ; on n'écoute pas (contrôleur muet)
  analogReadResolution(12);
  pinMode(THR_PIN,OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== VIOLET 5V open-drain (afficheur PA9/D8 -> contrôleur PC6/D10, niveau haut=pull-up 5V) ===");
  Serial.println("f/r=frein 1/2/3=gear g=auto m<v>=throttle x=stop");
}

unsigned long lastPrint=0;
void loop(){
  pump();
  if (Serial.available()){
    String c=Serial.readStringUntil('\n'); c.trim();
    if      (c=="f"){ forceBrake=true;  Serial.println("[cmd] FREIN forcé ON"); }
    else if (c=="r"){ forceBrake=false; Serial.println("[cmd] frein relâché"); }
    else if (c=="1"||c=="2"||c=="3"){ forceGear=c.toInt(); Serial.print("[cmd] gear "); Serial.println(c); }
    else if (c=="g"){ forceGear=0; Serial.println("[cmd] gear auto"); }
    else if (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }
  unsigned long now=millis();
  if (now-lastPrint>=1000){
    lastPrint=now; framesFwd=framesAccum; framesAccum=0;
    float node=analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("relay="); Serial.print(framesFwd); Serial.print("/s | frein_force="); Serial.print(forceBrake?"ON":"off");
    Serial.print(" gear="); Serial.print(forceGear); Serial.print(" thr="); Serial.print(thrCmd,2);
    Serial.print(" node="); Serial.print(node,2); Serial.print("V");
    if(framesFwd==0) Serial.print("  [rien de l'afficheur : verifie PA9/D8]");
    Serial.println();
  }
}
