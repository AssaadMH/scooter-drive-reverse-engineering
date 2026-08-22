# 03 — La démarche de rétro-ingénierie (étape par étape)

Ce document raconte **comment** on est passé d'un câble inconnu à un pilotage en boucle fermée.
Utile pour comprendre la logique, et pour reproduire la méthode sur un **autre modèle** de trottinette.

---

## Étape 1 — Sniffer le bus (`shadow_uart_sniffer_uno`)

- Branchement minimal : **D10 ← fil data**, **GND ← masse commune**. Lecture seule, sans risque.
- Le firmware **auto-détecte le baudrate** (mesure de la largeur d'impulsion minimale via `pulseIn`)
  → **9600 bauds**.
- Il dumpe les octets en hexa, en délimitant les trames sur les silences.

**Résultat** : trames régulières de 14 octets commençant par `02 0E ...`.

---

## Étape 2 — Décoder la trame

En faisant varier le guidon (accélérer / freiner) et en observant les octets qui changent :

- `B0=02` (STX), `B1=0E` (longueur = 14).
- **`B13 = XOR(B0..B12)`** → checksum confirmé sur 100 % des trames.
- Un drapeau `0x80↔0xA0` (frein), un champ valeur 16 bits `B8B9`, des bits d'état.

➡️ Détails complets : `02-protocol.md`.

---

## Étape 3 — Comprendre que ce bus n'est PAS la commande moteur

Tentative d'**injection UART** (firmware `shadow_uart_injector_uno`, qui émule le guidon en émettant
des trames valides sur D11) : **la roue ne bouge pas**.

Test décisif : couper le fil comm →
- afficheur en **E-06** (erreur de communication), vitesse 0,
- **mais la roue tourne** quand on accélère.

➡️ Conclusion : l'accélérateur arrive au contrôleur par un **fil analogique séparé**.
Le bus UART ne fait que de l'**affichage / télémétrie**. **Leçon clé : ne pas supposer que le bus
numérique = la commande.** Toujours vérifier par un test physique.

---

## Étape 4 — Cartographier le connecteur, trouver le throttle

Au **multimètre** (jamais l'Arduino sur de l'inconnu, à cause du 52 V) :

| Fil | Mesure | Rôle |
|---|---|---|
| rouge | 52 V | batterie + ☠️ |
| orangé | 52 V commuté | power/enable ☠️ |
| noir | 0 V | GND |
| bleu | data instable | comm UART (E-06 si coupé) |
| **gris/blanc** | **0,8 V → 3,2 V** en tournant la poignée | **THROTTLE** 🎯 |
| violet | 3–5 V variable | frein |

---

## Étape 5 — Injecter le throttle (`throttle_injector_uno`)

- **PWM (D11) → filtre RC (1 kΩ + 10 µF) → fil gris/blanc** (côté contrôleur, fil coupé).
- **GND → fil noir.**
- On laisse tout le reste branché → contrôleur alimenté, pas d'E-06.

**Résultat** : on commande une tension (0,8 V repos → ~2 V) → **la roue accélère** et l'afficheur
indique la vitesse. Seuil de démarrage observé ≈ **1,3–1,4 V**.

Le firmware propose un **pilotage manuel** (taper une tension cible, `u`/`d`, `n`, `x`) et des
**scénarios scriptés** (`s1` rampe douce, `s2` paliers, `s3` cycle conduite).

---

## Étape 6 — Décoder la vitesse (`throttle_loop_uno`)

Firmware qui **injecte le throttle ET écoute le bus** simultanément (D11 PWM + D10 tap),
en affichant `thr=<V> | trame hex`.

En tenant deux paliers de throttle et en lisant le km/h de l'afficheur :
- le champ `B8B9` **diminue** quand la vitesse augmente, et vaut `EA60` à l'arrêt
  → c'est une **période** (inverse de la vitesse).
- calibration : `mph = 2520 / B8B9` (recalibré 2026-06-03 ; l'ancien 667 était faux).

➡️ `02-protocol.md` §3.

---

## Étape 7 — Boucle fermée (`throttle_pi_uno`)

Régulateur **PI** :
- lit la période `B8B9`, la convertit en mph (avec filtre médian) ;
- compare à la consigne, ajuste la tension throttle ;
- feedforward à ~1,35 V pour amorcer le mouvement, plafond `MAX_V`, slew-rate anti-à-coup,
  arrêt si télémétrie perdue.

Commande **`c8`** = « tenir 8 mph ». **Résultat** : la roue accélère puis le throttle se stabilise
pour tenir la consigne (à vide : oscillation ±1,5 mph car la vitesse sature ~6–8 mph et la mesure est
bruitée ; bien plus lisse en charge / sur route).

➡️ Réglages : `04-closed-loop-tuning.md`.

---

## Méthode généralisable (autre trottinette)

1. **Sniffer** d'abord (D10 + GND), auto-détecter le baud, décoder le format + checksum.
2. **Ne pas supposer** que le bus = la commande. **Tester** : couper la comm, voir si ça roule encore.
3. **Multimètre** sur chaque fil du connecteur de commande : repérer +V (danger), GND, throttle
   (0,8→~4 V à la poignée), frein, comm.
4. **Injecter** sur le throttle analogique (PWM+RC ou DAC), fil coupé, côté contrôleur, GND commun.
5. **Lire** la télémétrie pour retrouver le champ vitesse (corréler avec l'afficheur).
6. **Boucler** avec un PI.
