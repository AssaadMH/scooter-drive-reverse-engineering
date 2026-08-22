# 05 — Marche arrière par inversion de phases (relais)

> Le contrôleur (CHK2-K1-03) **n'a pas de marche arrière firmware** (confirmé par recherche :
> les ESC de cette famille n'ont que des rapports avant). On la crée **matériellement** en
> **inversant 2 phases moteur + 2 capteurs Hall** via des relais, **au connecteur moteur**
> (hors résine). Trottinette **dual-motor → à faire sur les 2 moteurs.**

---

## ⚠️ RÈGLES DE SÉCURITÉ (non négociables)

1. 🛑 **Ne JAMAIS commuter les relais en mouvement / sous charge.** Commuter le 52 V continu
   sous courant = **arc destructeur** (contacts soudés, MOSFET de l'ESC détruits).
   ➡️ On ne change de sens qu'à **roue arrêtée + throttle au repos** (interlock logiciel obligatoire).
2. ✅ **Fail-safe** : bobines **OFF = MARCHE AVANT** (contacts au repos = NC = avant).
   Une coupure laisse la trottinette en avant, jamais coincée en arrière.
3. 🔌 **Diodes de roue libre** (1N4007) sur **chaque** bobine — sinon le MOSFET grille.
4. 🔩 Câblage des phases : **même section que les fils moteur** (≥ 4 mm² / 12 AWG), cosses
   serties solides, gaine thermo. Pas de domino sous-dimensionné.
5. 🧯 **Fusible** sur l'alim 12 V des bobines.
6. 🧪 Tests **roue surélevée**, vitesse mini d'abord, coupe-circuit à portée.

---

## 1. Principe d'inversion (par moteur)

| Bobines | Sens | Phases | Hall |
|---|---|---|---|
| **OFF** | 🟢 AVANT | A→A, B→B, C→C | Ha→Ha, Hb→Hb, Hc→Hc |
| **ON** | 🔁 ARRIÈRE | **A→B, B→A**, C→C | **Ha→Hb, Hb→Ha**, Hc→Hc |

On échange **2 phases** (A et B) et **les 2 Hall correspondants** (Ha et Hb). La phase C et le
Hall Hc passent en direct.

> ⚙️ **Commissioning** : si en arrière le moteur « broute » au lieu de tourner rond, l'appairage
> phase/Hall n'est pas bon : essayer une autre paire de Hall (Ha↔Hc ou Hb↔Hc). Il existe une
> combinaison qui donne une rotation arrière **douce** — la trouver roue en l'air, à basse vitesse.

---

## 2. Schéma — inversion des PHASES (par moteur)

2 relais de **puissance** inverseurs (SPDT / contact inverseur), bobine 12 V :

```
        K1  (SPDT puissance ≥80A, bobine 12V)
 ESC_A ─────● COM
            ├── NC (87a) ─────────► MOTEUR_A     ← AVANT  (bobine OFF)
            └── NO (87)  ─────────► MOTEUR_B     ← ARRIÈRE (bobine ON)

        K2  (SPDT puissance ≥80A, bobine 12V)
 ESC_B ─────● COM
            ├── NC (87a) ─────────► MOTEUR_B     ← AVANT
            └── NO (87)  ─────────► MOTEUR_A     ← ARRIÈRE

 ESC_C ───────────────────────────► MOTEUR_C     (direct, non commuté)
```

K1 et K2 sont commandés **ensemble** (même signal REVERSE).
Alternative plus propre si dispo : **1 relais DPDT de puissance 80 A** remplace K1+K2.

---

## 3. Schéma — inversion des HALL (par moteur)

1 relais **DPDT signal** (faible courant), bobine 12 V (type MY2N-12VDC) :

```
        K3  (DPDT signal, bobine 12V)
 ESC_Ha ────● COM1 ── NC ─────────► MOTEUR_Ha    ← AVANT
                   └─ NO ─────────► MOTEUR_Hb    ← ARRIÈRE
 ESC_Hb ────● COM2 ── NC ─────────► MOTEUR_Hb
                   └─ NO ─────────► MOTEUR_Ha

 ESC_Hc ───────────────────────────► MOTEUR_Hc   (direct)
 +5V Hall ─────────────────────────► +5V Hall    (direct)
 GND Hall ─────────────────────────► GND Hall    (direct)
```

---

## 4. Vue d'ensemble DUAL-MOTOR

```
                 ┌─────────── signal REVERSE (commun) ───────────┐
                 │                                               │
  MOTEUR AVANT : K1 + K2 (phases)  + K3 (Hall)                   │
  MOTEUR ARR.  : K4 + K5 (phases)  + K6 (Hall)                   │
                 │                                               │
                 └──── toutes les bobines en parallèle ──────────┘
                                  (mêmes ON/OFF)
```

6 relais au total : **4 de puissance** (phases, 2 par moteur) + **2 de signal** (Hall, 1 par moteur).
Toutes les bobines sont pilotées par **un seul** signal → les 2 moteurs s'inversent ensemble.

---

## 5. Étage de commande (driver des bobines)

L'Arduino/ESP ne peut pas piloter des bobines 12 V directement → MOSFET canal N logic-level :

```
        +12V (depuis buck 60V→12V)  ──┬──────────┬─────── ... (+ de chaque bobine K1..K6)
                                      │          │
                                  [bobine]   [diode 1N4007]   ← 1 diode //chaque bobine
                                      │          │             (cathode vers +12V)
                                      └────┬─────┘
                                           │  (les "-" de toutes les bobines réunis)
                                           ▼
   Arduino D7 ──[220Ω]──┤ GATE             DRAIN
                        │   Q1 = IRLZ44N (N-MOSFET logic-level)
              [10kΩ]    │   SOURCE ──────── GND (commun)
                ▼       │
               GND  ────┘   (10kΩ gate→GND = pulldown : bobines OFF au repos = AVANT)

   Fusible 2A sur le +12V des bobines.
```

- **Arduino Uno (5 V)** : la grille à 5 V sature l'IRLZ44N → OK direct.
- **ESP32 (3,3 V)** : ajouter un étage (petit NPN 2N2222 en level-shifter, ou MOSFET à seuil < 1 V
  type AO3400), sinon la grille n'est pas pleinement saturée.
- **Masse commune obligatoire** : GND Arduino/ESP = GND sortie buck = négatif batterie (= GND Hall).

---

## 6. Interlock logiciel (firmware) — obligatoire

```
Pour changer de sens :
  1. forcer le throttle au REPOS (0,8 V)
  2. attendre vitesse == 0  (lue via la télémétrie) pendant > 0,5 s
  3. basculer la broche REVERSE (relais)
  4. petite pause (~300 ms, le temps que les contacts s'établissent)
  5. ré-autoriser le throttle
Jamais l'inverse. En cas de doute -> rester en AVANT.
```

---

## 7. Liste de courses (2betrading, Sousse)

| Qté | Composant | Caractéristiques |
|---|---|---|
| 4 | **Relais de puissance inverseur (SPDT)** | contacts **≥ 80 A**, bobine **12 V** (style relais auto haute intensité). *Ou 2 relais DPDT de puissance 80 A.* → phases |
| 2 | **Relais DPDT signal** | bobine **12 V**, 5 A (type **MY2N-12VDC**) → Hall |
| 1 | **MOSFET N logic-level** | **IRLZ44N** (ou équiv.) → driver |
| 6 | **Diode** | **1N4007** (roue libre, 1 par bobine) |
| 1 | **Résistance** 220 Ω | gate du MOSFET |
| 1 | **Résistance** 10 kΩ | pulldown gate (sûreté) |
| 1 | **Module buck** | entrée **≥ 60 V**, sortie **12 V / ≥ 1 A** (ex. XL7015 ; **PAS** de LM2596, max 40 V). *Ou une petite batterie 12 V dédiée aux bobines.* |
| 1 | **Porte-fusible + fusible 2 A** | protection alim 12 V bobines |
| — | **Fil souple** | section = phases moteur (**≥ 4 mm²**), + cosses à sertir, gaine thermo |
| — | **Borniers haute intensité** | ou soudure + gaine ; connexions solides |

> 💪 Pour du « **très solide** » : prends des contacts **80 A** (marge confortable même si le pic
> moteur est ~40–60 A), des fils de **6 mm²** si tu as le budget, et soigne les sertissages/soudures
> (la résistance de contact = échauffement). Monte les relais sur un support rigide, fils de phase
> **courts**.

---

## 8. Pourquoi pas autre chose ?

- **VESC** : reverse natif, télémétrie propre — mais remplace tout l'ESC (gros chantier).
- **Relais (ce doc)** : garde l'ESC d'origine, se fait à l'extérieur (résine OK), réversible.
  C'est le meilleur compromis ici.
