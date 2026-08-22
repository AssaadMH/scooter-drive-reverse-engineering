# 07 — Architecture robot : 2 trottinettes (skid-steer)

Le système final est un **robot** constitué de **2 trottinettes similaires** (chacune dual-motor)
reliées par un **châssis** → entraînement **différentiel / skid-steer**.

```
            ┌──────────── CHÂSSIS ────────────┐
            │                                 │
   CÔTÉ GAUCHE = Trottinette L        CÔTÉ DROIT = Trottinette R
   (ESC L, 2 moteurs)                 (ESC R, 2 moteurs)
   1 carte de contrôle ESP            1 carte de contrôle ESP
            │                                 │
            └────────── BUS CAN ──────────────┘
                          │
                    CERVEAU (Jetson)  ← navigation / décisions
```

- **2 côtés indépendants** (gauche/droite), chacun = 1 trottinette = **1 ESC piloté en throttle +
  marche arrière + retour vitesse**.
- **Déplacements** : avancer = 2 côtés avant ; tourner = vitesses différentes ; **tourner sur place
  = un côté avant, l'autre arrière** (→ relais reverse obligatoires des deux côtés).

---

## 1. Rôle de chaque unité de contrôle (1 par trottinette)

Carte **ESP32**, identique des deux côtés. Elle :
1. **injecte le throttle** (DAC → fil gris/blanc) ;
2. **lit la vitesse** (tap UART sur le bleu, `mph = K / B8B9`) ;
3. **pilote la marche arrière** (broche `REVERSE` → bloc relais phases+Hall, cf. `05`) ;
4. exécute la **boucle PI de vitesse** + l'**interlock reverse** (bascule seulement à l'arrêt) ;
5. parle au cerveau via **CAN** : reçoit `{sens, vitesse cible}`, renvoie `{vitesse, état, défauts}`.

> Chaque ESC (M41 Tank Dual) gère **2 moteurs ensemble** → on n'a pas le contrôle individuel des 2
> roues d'un même côté, mais on a bien **2 côtés indépendants** = suffisant pour le skid-steer.

---

## 2. Communication : bus CAN (recommandé)

| Pourquoi CAN | |
|---|---|
| Immunité bruit | différentiel, parfait près des moteurs/onduleurs |
| Multi-nœud | Jetson + ESP-L + ESP-R sur 2 fils |
| Robuste vibration | détection d'erreur, retransmission |
| Natif ESP32 | contrôleur TWAI/CAN intégré + transceiver externe |

- Transceiver par nœud : **SN65HVD230** (3,3 V) ou **TJA1051T/3**.
- **Terminaison 120 Ω** aux deux extrémités du bus.
- Alternative plus simple : **UART/RS-485**, mais CAN est le standard robuste.

### Modèle de commande (exemple)
```
Jetson → ESP (par côté) :  { direction: AVANT|ARRIERE, vitesse_cible_mph }
ESP    → Jetson         :  { vitesse_mph, sens_actuel, throttle_V, defauts, estop }
Cadence : ~20–50 Hz. Watchdog : si plus de trame CAN reçue > 200 ms -> STOP (throttle repos).
```

---

## 3. Sécurité robot (un robot autonome DOIT avoir ça)

- 🍄 **Arrêt d'urgence matériel** (bouton coup-de-poing) qui :
  - coupe l'alim **12 V des bobines de relais** (→ retour AVANT, throttle non commuté), **et**
  - force le **throttle au repos** (couper l'alim du DAC/buffer, ou un relais sur la sortie throttle),
  - idéalement coupe la **puissance** via un **contacteur principal**.
- 🐕 **Watchdog CAN** : perte du cerveau > 200 ms → chaque ESP met son côté au repos.
- 🟢 **Fail-safe** : relais reverse **OFF = AVANT** (déjà prévu), throttle **OFF = repos**.
- 🔒 **Interlock reverse** : on ne bascule le sens qu'à **vitesse 0** (lue par côté).
- Démarrage **toujours au repos**, jamais de mouvement sans commande explicite + heartbeat.

---

## 4. Comptage relais (par robot)

Par trottinette (dual-motor) : **4 relais puissance** (phases, 2 par moteur) + **2 relais signal**
(Hall, 1 par moteur). **Robot = ×2** → **8 relais puissance + 4 relais signal**.
Chaque côté a sa propre broche `REVERSE` (les 2 côtés peuvent reculer **indépendamment** → spin).

---

## 5. Mesurer le courant max (inconnu) — pour dimensionner les relais

Le PCB de contrôle **ne dépend pas** du courant moteur (seuls les relais hors-PCB le subissent),
donc on peut concevoir la carte maintenant et **mesurer le courant ensuite**.

Méthodes (du plus simple au plus précis) :
1. **Pince ampèremétrique DC** (ex. UNI-T UT210E) sur le **câble principal batterie**, à **pleine
   charge** (roues au sol / freinées contre un obstacle, **bref**). → courant batterie max.
2. **Fusible principal / BMS** : sa valeur donne une **borne haute** du courant continu.
3. Pince DC sur **un fil de phase** : courant de phase (souvent ≥ courant batterie en pic).

Dimensionner les relais à **≥ 1,5× le pic mesuré**. En l'absence de mesure, **80 A** est une marge
sûre pour un « Tank dual » ; **confirmer avant achat définitif**.

---

## 6. Conséquences pour le BOM / la carte

- **×2 cartes identiques** (une par trottinette).
- **Ajouter un transceiver CAN** (SN65HVD230) à chaque carte + terminaisons 120 Ω.
- **Vernis tropicalisation** (choisi) : robuste **et** réparable — adapté à de la R&D sur robot.
- **Modulaire** (choisi) : modules ESP32 / alim / DAC throttle / driver relais / CAN reliés par
  connecteurs à verrou sur une carte-mère (« backplane »).
- Prévoir l'**E-stop** et le **contacteur de puissance** au niveau robot (partagés).

Détails carte & anti-vibration : `06-control-board-and-ruggedization.md`.
Câblage relais : `05-reverse-relay-wiring.md`.
