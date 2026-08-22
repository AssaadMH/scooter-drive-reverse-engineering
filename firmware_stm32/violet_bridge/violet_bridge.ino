/*
 * STM32 F401RE — RELAIS BIDIRECTIONNEL "homme du milieu" sur le bus VIOLET
 *
 * Le violet est un bus 9600 1-fil HALF-DUPLEX, INTERACTIF : l'afficheur envoie une trame
 * (0x01 0x14 ... 20o, checksum XOR [0..18]) et ATTEND une réponse du contrôleur. Couper le
 * violet -> l'afficheur ne reçoit plus la réponse -> E07 après ~quelques sec + contrôleur en sécurité.
 * Un relais à SENS UNIQUE ne suffit pas (confirmé). On relaie donc LES DEUX SENS, en éditant
 * juste le frein (et le gear) dans le sens afficheur->contrôleur.
 *
 * CABLAGE (violet COUPÉ, les 2 bouts au STM32, chacun sur un UART 1-fil) :
 *   PA9  (= D8)  <-> VIOLET côté AFFICHEUR  : USART1 half-duplex (on joue le "contrôleur")
 *   PC6  (= D10) <-> VIOLET côté CONTRÔLEUR : USART6 half-duplex (on joue l'"afficheur")
 *   PB10 (= D6)  -> R1k -> gris/blanc       : PWM throttle (lancer la roue). 3,3V.
 *   PA0  (= A0)  <- nœud throttle           : relecture.
 *   GND  <- noir.   (le BLEU reste branché afficheur<->contrôleur en direct, on n'y touche plus)
 *   >>> DÉPLACER le bout AFFICHEUR du violet de D9 vers D8. Le tap bleu sur D8 : on l'enlève.
 *   roue SURÉLEVÉE ; jamais Arduino sur rouge/orangé 52V.
 *
 * COMMANDES (Serial 115200) :
 *   f = forcer FREIN ON   r = relâcher (frein réel de l'afficheur)
 *   1/2/3 = forcer gear   g = ne plus forcer le gear (passe le réel)
 *   m<v> = throttle volts   x/0 = throttle 0
 */

HardwareSerial dashUart(PA9);   // USART1 1-fil <-> afficheur
HardwareSerial ctrlUart(PC6);   // USART6 1-fil <-> contrôleur

#define THR_PIN  PB10
#define NODE_PIN PA0
#define VREF     3.3
#define GAP_MS   3              // silence => fin de la réponse contrôleur

bool forceBrake = false;
int  forceGear  = 0;            // 0 = ne pas forcer ; 1/2/3 sinon
float thrCmd = 0;
void applyThrottle(){ float v=thrCmd; if(v<0)v=0; if(v>VREF)v=VREF; analogWrite(THR_PIN,(int)(v/VREF*255.0)); }

unsigned long dFrames=0, cBlobs=0, dAccum=0, cAccum=0;

// emet n octets sur un UART 1-fil puis repasse en RX en jetant l'echo (auto-reception)
void hdSend(HardwareSerial &u, uint8_t *b, int n){
  u.write(b, n); u.flush(); u.enableHalfDuplexRx();
  while (u.available()) u.read();           // l'echo est deja bufferise apres flush -> on le jette
}

// AFFICHEUR -> (edit) -> CONTRÔLEUR
void pumpDashToCtrl(){
  static uint8_t buf[20]; static uint8_t idx=0;
  while (dashUart.available()){
    uint8_t b = dashUart.read();
    if (idx==0)      { if (b==0x01) buf[idx++]=b; }
    else if (idx==1) { if (b==0x14) buf[idx++]=b; else idx=0; }
    else {
      buf[idx++]=b;
      if (idx>=20){
        idx=0;
        uint8_t x=0; for(uint8_t i=0;i<19;i++) x^=buf[i];
        if (x==buf[19]){                                  // trame afficheur valide
          if (forceBrake) buf[4]|=0x80;
          if (forceGear){ uint8_t nib=(forceGear==1)?0x05:(forceGear==2)?0x0A:0x0F; buf[4]=(buf[4]&0xF0)|nib; }
          uint8_t c=0; for(uint8_t i=0;i<19;i++) c^=buf[i]; buf[19]=c;   // recalcul checksum
          hdSend(ctrlUart, buf, 20);
          dAccum++;
        }
      }
    }
  }
}

// CONTRÔLEUR -> (brut) -> AFFICHEUR   (format réponse inconnu : on relaie tel quel, par rafale)
void pumpCtrlToDash(){
  static uint8_t buf[64]; static uint8_t n=0; static unsigned long last=0;
  while (ctrlUart.available()){
    uint8_t b = ctrlUart.read();
    if (n < sizeof(buf)) buf[n++]=b;
    last = millis();
  }
  if (n && millis()-last > GAP_MS){       // fin de rafale -> renvoyer a l'afficheur
    hdSend(dashUart, buf, n);
    cAccum++; n=0;
  }
}

void setup(){
  Serial.begin(115200);
  dashUart.setHalfDuplex(); dashUart.begin(9600); dashUart.enableHalfDuplexRx();
  ctrlUart.setHalfDuplex(); ctrlUart.begin(9600); ctrlUart.enableHalfDuplexRx();
  analogReadResolution(12);
  pinMode(THR_PIN, OUTPUT); applyThrottle();
  delay(300);
  Serial.println("\n=== VIOLET BRIDGE bidirectionnel (afficheur PA9/D8 <-> contrôleur PC6/D10) ===");
  Serial.println("f/r=frein  1/2/3=gear g=auto  m<v>=throttle  x=stop");
}

unsigned long lastPrint=0;
void loop(){
  pumpDashToCtrl();
  pumpCtrlToDash();

  if (Serial.available()){
    String c = Serial.readStringUntil('\n'); c.trim();
    if      (c=="f"){ forceBrake=true;  Serial.println("[cmd] FREIN forcé ON"); }
    else if (c=="r"){ forceBrake=false; Serial.println("[cmd] frein relâché (réel)"); }
    else if (c=="1"||c=="2"||c=="3"){ forceGear=c.toInt(); Serial.print("[cmd] gear forcé "); Serial.println(c); }
    else if (c=="g"){ forceGear=0; Serial.println("[cmd] gear auto (réel)"); }
    else if (c=="x"||c=="0"){ thrCmd=0; applyThrottle(); Serial.println("[cmd] throttle 0"); }
    else if (c.length() && c[0]=='m'){ thrCmd=c.substring(1).toFloat(); applyThrottle(); Serial.print("[cmd] throttle "); Serial.print(thrCmd,2); Serial.println("V"); }
  }

  unsigned long now=millis();
  if (now-lastPrint>=1000){
    lastPrint=now;
    dFrames=dAccum; cBlobs=cAccum; dAccum=0; cAccum=0;
    float node = analogRead(NODE_PIN)*VREF/4095.0;
    Serial.print("dash->ctrl="); Serial.print(dFrames); Serial.print("/s  ctrl->dash="); Serial.print(cBlobs); Serial.print("/s");
    Serial.print(" | frein_force="); Serial.print(forceBrake?"ON":"off");
    Serial.print(" gear_force="); Serial.print(forceGear);
    Serial.print(" thr="); Serial.print(thrCmd,2); Serial.print(" node="); Serial.print(node,2); Serial.print("V");
    if (dFrames==0) Serial.print("  [rien de l'afficheur : verifie PA9/D8]");
    else if (cBlobs==0) Serial.print("  [contrôleur muet : il ne repond pas (ou pas bidirectionnel)]");
    Serial.println();
  }
}
