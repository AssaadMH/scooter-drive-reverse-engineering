# 06 — Carte de contrôle : BOM & tenue aux vibrations/chocs

Objectif : transformer le montage prototype (Arduino + fils) en une **carte embarquée robuste**,
résistante aux **vibrations**, **chocs**, humidité et chaleur d'une trottinette.

> **Choix retenus (projet robot 2 trottinettes — cf. `07-robot-architecture.md`)** :
> - **Vernis tropicalisation** (conformal coating) — robuste **et** réparable (pas de potting).
> - **Conception modulaire** autour d'un **ESP32** (modules reliés par connecteurs à verrou sur backplane).
> - **×2 cartes identiques** (une par trottinette / côté du robot).
> - **Bus CAN** entre les 2 ESP + le cerveau (Jetson) → ajouté au BOM (ligne 13).
> - Courant moteur **non encore mesuré** → relais dimensionnés à **80 A** par défaut, à confirmer
>   (méthode de mesure : `07-robot-architecture.md` §5).

---

## 1. Architecture : 2 ensembles séparés

> ⚠️ **Ne PAS mettre les relais de puissance (phases, ~80 A) sur le PCB.** Trop de courant,
> de chaleur et d'EMI. On sépare :

| Ensemble | Contenu | Montage |
|---|---|---|
| **A. Carte de contrôle (PCB)** | MCU, alimentations, DAC throttle, tap UART, driver MOSFET, relais **signal** (Hall), protections, connecteurs | dans un boîtier IP, sur isolateurs |
| **B. Bloc puissance (hors PCB)** | 4× relais/contacteurs **phases ≥80 A** (2 par moteur), câblage gros section | sur platine métal, châssis, câbles courts |

La carte A **commande** le bloc B via un fil `REVERSE` (+ masse).

---

## 2. BOM — Carte de contrôle (PCB)

| # | Fonction | Composant (exemple) | Spéc / Notes |
|---|---|---|---|
| 1 | MCU | **ESP32-WROOM-32E** (module soudé) | 3,3 V, 2× UART, I²C, WiFi/BT. *Module soudé, pas sur socket.* |
| 2 | Alim étage 1 | Buck **60 V→12 V** (IC type **MP9486A** 100 V, ou module scellé auto) | bobines relais + ventil ; ⚠️ **jamais** LM2596 (40 V max) |
| 3 | Alim étage 2 | Buck **12 V→5 V** (**MP1584**) + LDO **3,3 V** (**AMS1117-3.3**) | logique. Ou buck 60 V→5 V direct + LDO |
| 4 | Throttle (sortie analogique) | DAC **MCP4725** (I²C) + buffer op-amp **MCP6002** | 0–~3,4 V propre vers fil gris/blanc. Mieux que PWM+RC |
| 5 | Tap vitesse (UART) | R série 1 kΩ + diode clamp / level-shifter | écoute fil bleu ; adapter 5 V↔3,3 V |
| 6 | Driver relais | MOSFET N logic-level **IRLZ44N** (ou **AOD4184**) | pilote les bobines ; **pulldown 10 kΩ** gate (fail-safe AVANT) |
| 7 | Roue libre | **1N4007** ×(nb bobines) | 1 par bobine, près de la bobine |
| 8 | Relais signal Hall | DPDT 12 V ×2 (par trottinette) | **déplacés HORS carte**, dans le boîtier relais avec les phases (moins de bruit) — cf. `08` |
| 9 | Protection entrée | TVS **SMCJ58A** + fusible blade + P-MOSFET anti-inversion | transitoires 52 V, polarité inversée |
| 10 | Filtrage | élec **100 µF/100 V** (low-ESR) + **MLCC 10 µF/100 V** ×plusieurs | bulk + découplage |
| 11 | Indicateurs | LED + R (état, REVERSE, power) | diagnostic |
| 12 | Connecteurs | **JST-XH/VH à verrou** (signaux), **bornier à vis ou cosses boulonnées** (puissance/12 V) | **verrouillables**, pas de header dupont |
| 13 | **Transceiver CAN** | **SN65HVD230** (3,3 V) ou **TJA1051T/3** + terminaison **120 Ω** | bus robuste ESP-L ↔ ESP-R ↔ Jetson (cf. `07`) |
| 14 | **Arrêt d'urgence** (niveau robot) | bouton coup-de-poing + contacteur principal | coupe 12 V bobines + throttle ; partagé par les 2 côtés |

## 3. BOM — Bloc puissance (hors PCB)

| # | Composant | Spéc |
|---|---|---|
| 13 | Relais/contacteur **inverseur SPDT** ×4 | contacts **≥80 A**, bobine 12 V (2 par moteur, phases) |
| 14 | Câble souple **≥6 mm²** + cosses à sertir | = section des phases moteur |
| 15 | Platine de montage métal + visserie | support rigide des relais |
| 16 | Porte-fusible + fusible 2 A (bobines) | protection alim relais |

> Réf. câblage relais : voir **`05-reverse-relay-wiring.md`**.

---

## 4. Conception anti-VIBRATIONS (le cœur de ta demande)

La vibration **fatigue les soudures et les connecteurs** ; les chocs **fissurent les composants
massifs**. Règles :

### Composants
- **Privilégier le CMS** (faible masse, bras de levier court) plutôt que du traversant haut.
- **Bannir les composants hauts/lourds non soutenus** : gros condensateurs électrolytiques,
  relais, modules verticaux. Préférer **MLCC / tantale / polymère** aux élec hauts.
- **Tout composant haut/lourd (relais, gros caps, connecteurs) = collé** au PCB (RTV silicone ou
  époxy) en plus de la soudure.
- **MLCC à terminaison souple** (« soft termination ») pour éviter les fissures de céramique au choc.
- **Pas de circuits intégrés sur support (socket)** — ça se déboîte. Tout **soudé**.

### Soudure & PCB
- FR4 **1,6 mm** min, **soudures à congé franc**, traversant soudé des **deux côtés**.
- **Soutenir le PCB en plusieurs points** (entretoises) pour **éviter les modes de résonance** des
  grandes portées ; ajouter un appui central sur les grandes cartes.
- Largeur de pistes adaptée au courant ; **cuivre 2 oz** pour les pistes 12 V/relais.

### Montage
- **Isolateurs anti-vibration** sur les fixations : entretoises avec **silentblocs / passe-fils
  caoutchouc**, ou la carte sur **plots élastomères** → découple la carte du châssis.
- **4 points de fixation** minimum, trous métallisés renforcés.

### Câblage (cause #1 de panne en vibration)
- **Fil souple toronné** (jamais rigide), longueurs avec **boucle de mou**.
- **Strain relief obligatoire** : ancrer chaque câble (collier + embase collée) **près du
  connecteur** pour que la vibration tire sur l'ancrage, **pas sur la soudure**.
- **Connecteurs à verrou positif** (JST-VH, Molex, ou auto **Deutsch/AMP Superseal** pour
  l'extérieur). **Zéro dupont/breadboard** dans la version finale.
- Câbles de phase **courts** et **bridés** au châssis.

---

## 5. Conception anti-CHOCS & environnement

- **Vernis tropicalisation (conformal coating)** acrylique/uréthane : humidité, poussière, sel.
  Sérieux et **réparable**.
- **Potting (résine)** pour le maximum vibration/choc/étanchéité (comme l'ESC d'origine) :
  - **Polyuréthane souple** recommandé (absorbe vibration + cycles thermiques) plutôt qu'**époxy
    rigide** (peut fissurer/contraindre les composants au cyclage).
  - ⚠️ **Irréversible** (plus de réparation) et **piège la chaleur** → **dérater** les composants,
    sortir la chaleur des régulateurs.
  - Compromis : **conformal coating + collage mécanique des pièces lourdes** = robuste **et**
    réparable. Potting seulement si IP67/usage sévère requis.
- **Boîtier IP65/67** avec **joint** et **presse-étoupes** ; carte montée dedans sur isolateurs.
- **Plage de température** : composants **grade auto −40…+125 °C** pour l'extérieur.

---

## 6. EMI / masses (moteur = très bruyant)

- **Masses séparées** : puissance (relais/12 V) vs signal (MCU/3,3 V), reliées en **un seul point
  (étoile)** près de l'entrée d'alim.
- Garder le **MCU et l'analogique loin** des pistes 12 V/relais et des câbles de phase.
- Plan de masse, découplage généreux, éventuellement **ferrites** sur les fils de signal.
- Diodes de roue libre **au plus près** des bobines.

---

## 7. Flux de fabrication

1. Schéma + routage sous **KiCad**.
2. Fab + assemblage CMS chez **JLCPCB / PCBWay** (livrent en Tunisie).
3. Composants traversants/connecteurs/relais : soudés + **collés**.
4. **Conformal coating** (ou potting PU si sévère).
5. **Tests** : table vibrante si dispo, sinon **roulage longue durée + nids-de-poule**, cycles
   thermiques, contrôle des connecteurs après essais.

---

## 8. Pièces dispo localement (2betrading Sousse) vs à commander

- **Local** : ESP32, MOSFET (IRLZ44N), diodes 1N4007, relais 12 V, modules buck, JST, borniers,
  fusibles, fil, TVS, LDO.
- **À commander (JLCPCB/PCBWay/LCSC)** : le **PCB custom**, MCP4725, MP9486A, MLCC soft-term,
  connecteurs auto étanches, résine de potting / vernis, boîtier IP.
