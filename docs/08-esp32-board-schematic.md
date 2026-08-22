# 08 — Schéma électronique complet : carte de contrôle ESP32 (modulaire)

Carte **identique ×2** (une par trottinette/côté). Architecture **modulaire** : une **carte-mère
(backplane)** porte les connecteurs + la glu (driver, protections), et reçoit des **modules**
enfichables (ESP32, bucks). **Vernis tropicalisé** après assemblage.

> Convention : `[valeur]` = valeur de composant. Tensions batterie : 42–58,8 V (14S), nominal ~52 V.

---

## 1. Schéma-bloc

```
   52V IN ─[F1 3A]─[prot. inversion]─[TVS]─┬─► BUCK1 60→12V ─┬─► +12V (bobines relais, ventilo)
                                           │                 └─► BUCK2 12→5V ─► +5V ─► ESP32(VIN)
                                           │                                         └─►(LDO interne)─► +3V3
   +3V3 ─► ESP32-WROOM-32E ─► SN65HVD230 (CAN) ─► J(CAN)
                            ├─ DAC GPIO25 ─► [RC] ─► BUF op-amp(5V) ─► [R_out] ─► THROTTLE OUT ─► J(DASH)
                            ├─ GPIO27 ─► AO3400 ─► REVERSE_SW ─► J(COILS) ─► boîtier relais (phases+Hall)
                            ├─ UART2 RX GPIO16 ◄─ [clamp] ◄─ SPEED (fil bleu) ◄─ J(DASH)
                            ├─ GPIO34 ◄─ E-STOP sense
                            └─ GPIO33 ─► THROTTLE-KILL relay (sécurité)
```

Modules enfichables : **ESP32-DevKitC**, **BUCK1 (60→12 V)**, **BUCK2 (12→5 V)**.
Sur la carte-mère : protections, op-amp throttle, AO3400, SN65HVD230, relais throttle-kill, clamps,
LED, connecteurs.

---

## 2. Brochage ESP32 (définitif)

| GPIO | Net | Sens | Note |
|---|---|---|---|
| **GPIO25** | `THROTTLE_DAC` | sortie (DAC1) | DAC 8 bits interne 0–3,3 V → buffer |
| **GPIO27** | `REVERSE_DRIVE` | sortie | grille AO3400 (bobines relais) |
| **GPIO33** | `THROTTLE_KILL` | sortie | active le relais throttle (sécurité) |
| **GPIO5** | `CAN_TX` | sortie | → SN65HVD230 TXD |
| **GPIO4** | `CAN_RX` | entrée | ← SN65HVD230 RXD |
| **GPIO16** | `SPEED_RX` | entrée (UART2) | ← fil bleu (clampé 3,3 V) |
| **GPIO34** | `ESTOP_SENSE` | entrée (in-only) | lecture état arrêt d'urgence |
| **GPIO2** | `LED_RUN` | sortie | LED verte (alim/run) |
| **GPIO14** | `LED_REV` | sortie | LED jaune (marche arrière active) |
| (GPIO21/22) | I²C | — | réservé MCP4725 optionnel (DAC 12 bits) |

> **DAC** : on utilise le **DAC interne de l'ESP32** (GPIO25, 8 bits) → throttle 0,8–3,2 V piloté
> en ~185 pas (≈13 mV) = largement assez. Option précision : MCP4725 (12 bits) en I²C.

---

## 3. Étage d'entrée & protections

```
 BAT+ (52V) ──[F1 3A]──┬───────────────[Q_rp]───────┬────────► +52V_PROT
                       │  anti-inversion :           │
                      [TVS1]   Q_rp = P-MOSFET ≥80V   ├─[C1 100µF/100V]─┐
                   SMCJ60A     (ex. AOTF...) drain    │ [C2 10µF/100V]  │
                  (vers GND)   côté charge ; grille   │ [C3 100nF]      │
                       │       via 100k → source,     │                 │
 BAT− (GND) ───────────┴───────  10k + zener 12V ─────┴─────────────────┴── GND
```
- **F1** : fusible 3 A (côté électronique ; les bobines relais ont leur **propre fusible 2 A**).
- **Q_rp** : P-MOSFET anti-inversion ≥80 V (faibles pertes). *Alternative simple : Schottky 100 V/5 A
  en série (ex. SS510), ~0,4 W de pertes — acceptable.*
- **TVS1** : SMCJ60A (standoff 60 V > 58,8 V) — transitoires. *Surge fort : 1.5KE68A.*
- **C1/C2/C3** : réservoir + découplage haute tension.

---

## 4. Alimentations (power tree)

```
 +52V_PROT ─► BUCK1 (MP9486A / module 60→12V, 12V @ ≥1A) ─► +12V
                                                            │
 +12V ─► BUCK2 (MP1584 / module 12→5V, 5V @ ≥1A) ─► +5V ───► ESP32 VIN(5V) ─►(LDO)─► +3V3
                                                    └─► op-amp throttle (V+)
 +3V3 ─► SN65HVD230, clamps, pullups
```
- **BUCK1** doit accepter **≥60 V** en entrée (⚠️ **jamais LM2596**, 40 V max).
- **+12 V** : bobines relais (via E-stop) + ventilo éventuel.
- **+5 V** : ESP32 (VIN) + alim op-amp (rail-to-rail).
- **+3V3** : issu du LDO de l'ESP32-DevKitC (≤500 mA dispo) → CAN + logique.
- **Découplage** : 100 nF par broche d'alim de chaque IC ; 10–47 µF aux sorties bucks.

---

## 5. Sortie THROTTLE (DAC → buffer → fil gris/blanc)

```
 GPIO25(DAC) ──[R10 1k]──┬── BUF + (MCP6002, alim +5V)
                         │      │
                      [C10 100nF]  sortie ──[R11 100Ω]──┬───────────► THROTTLE OUT (J_DASH, fil gris/blanc)
                         │      (gain unité : sortie→entrée−)         │
                        GND                                          [D10 BAT54S] clamp vers +5V et GND
                                                                      │
                                            (relais THROTTLE-KILL en série, cf. §7)
```
- **R10+C10** : lissage du DAC (fc ≈ 1,6 kHz).
- **MCP6002** : buffer rail-to-rail. **Gain unité** par défaut (sortie reliée à l'entrée −).
- **Option pleine échelle >3,3 V** : non-inverseur gain 1,33 → `Rf=33k` (sortie→entrée −),
  `Rg=100k` (entrée −→GND) ⇒ 3,3 V → 4,4 V. (Inutile tant qu'on reste ≤3,2 V.)
- **R11** : protège l'op-amp ; **D10** clampe la sortie (sécurité entrée contrôleur).

---

## 6. Sortie REVERSE (driver de bobines)

```
 GPIO27 ──[R20 100Ω]── G │ AO3400 (N-MOSFET, Vgs(th)~0,65V → pleinement ON à 3,3V)
            [R21 100k]──┘ │ (pulldown G→GND : fail-safe AVANT)
                          S ── GND
                          D ──┬───────────────► REVERSE_SW (J_COILS) → boîtier relais (phases+Hall)
                              │
                          [D20 1N4007] (cathode → +12V)  + [TVS2] : roue libre / clamp inductif
 +12V ───────────────────────┴──► +12V (J_COILS)
```
- **AO3400** : choisi pour être pleinement saturé à **3,3 V** (l'IRLZ44N ne l'est qu'à ~5 V).
  Pilote toutes les bobines (phases+Hall du boîtier) ~0,8 A → marge énorme.
- **R21 100k** : pulldown ⇒ au repos/panne, bobines OFF ⇒ **MARCHE AVANT** (fail-safe).
- **D20 + TVS2** : protègent le MOSFET du retour inductif (flyback aussi présent côté relais).
- **J_COILS** = 2 fils vers le boîtier relais : `+12V` et `REVERSE_SW` (drain).

---

## 7. Sécurité : E-stop + throttle-kill

```
 E-STOP (NC) ──┬── coupe le +12V vers les bobines  ⇒ relais retombent en AVANT
               ├── coupe la bobine du relais THROTTLE-KILL ⇒ throttle forcé au repos
               └── via [div. 10k/10k] ── GPIO34 (sense) : le firmware sait que l'E-stop est actif

 Relais THROTTLE-KILL (SPDT, bobine via GPIO33 ET E-stop) :
   - énergisé (normal)  : THROTTLE OUT = sortie op-amp
   - relâché (E-stop/panne) : THROTTLE OUT = [diviseur ~0,8V] (repos)  ⇒ fail-safe
```
- Bouton **coup-de-poing NC** ; **contacteur principal** au niveau robot recommandé (coupe la
  puissance).
- **Watchdog CAN** (firmware) : perte cerveau > 200 ms ⇒ throttle repos + (option) reverse OFF.

---

## 8. Bus CAN

```
 GPIO5(TXD)──► SN65HVD230 pin1 TXD
 GPIO4(RXD)◄── pin4 RXD
 +3V3 ── pin3 VCC ; GND ── pin2
 pin8 RS ──[R30 10k]── GND        (mode haute vitesse ; R pour slope-control)
 pin7 CANH ─┬─────────────► J_CAN CANH
 pin6 CANL ─┤                J_CAN CANL, GND
            └─[R31 120Ω (cavalier)] entre CANH/CANL  (terminaison, aux 2 bouts du bus seulement)
 (option ESD : PESD1CAN sur CANH/CANL)
```

---

## 9. Connecteurs (carte-mère, tous **à verrou**)

| Conn. | Broches | Vers |
|---|---|---|
| **J_PWR** | 52V, GND | batterie (bornier/cosses) |
| **J_DASH** | SPEED(bleu), THROTTLE(gris/blanc), GND(noir) | tap connecteur dashboard 6-pin |
| **J_COILS** | +12V, REVERSE_SW | boîtier relais (phases + Hall) |
| **J_CAN** | CANH, CANL, GND | bus CAN (ESP-L ↔ ESP-R ↔ Jetson) |
| **J_ESTOP** | ESTOP_NC ×2, sense | bouton coup-de-poing |
| **J_AUX** | +12V, +5V, GND | ventilo / éclairage (option) |

> ☠️ Les fils **52 V** (J_PWR) et **+52V_PROT** restent **isolés** ; aucune piste 52 V près du MCU.

---

## 10. BOM différentiel (valeurs) — par carte (×2 pour le robot)

| Réf | Valeur / Pièce |
|---|---|
| ESP32 | **ESP32-DevKitC (WROOM-32E)** |
| BUCK1 | module/IC **60→12 V, 1 A** (MP9486A) |
| BUCK2 | module/IC **12→5 V, 1 A** (MP1584) |
| U_buf | **MCP6002** (op-amp RtR) |
| U_can | **SN65HVD230** + R30 10k + R31 120Ω (cavalier) |
| Q_rp | **P-MOSFET ≥80 V** (ou Schottky SS510) |
| Q_rev | **AO3400** (N-MOSFET logic-level) |
| K_kill | **relais SPDT 5 V** (throttle-kill) |
| F1 / F_coil | fusible **3 A** / **2 A** |
| TVS1/TVS2 | **SMCJ60A** / clamp inductif |
| D10 | **BAT54S** (clamp throttle) ; D_speed **BAT54S** (clamp RX) |
| D20 | **1N4007** (flyback) |
| C1 | **100 µF/100 V** ; C2 **10 µF/100 V** ; C3,C10,déc. **100 nF** |
| R10 1k, R11 100Ω, R20 100Ω, R21 100k, R30 10k, R31 120Ω, R_speed 1k, div. 10k×n |
| LED | verte (RUN) + jaune (REV) + R 1k |
| Connecteurs | JST-VH/XH à verrou + borniers ; E-stop coup-de-poing |

---

## 11. Notes de réalisation (rappel `06`)

- **Modules soudés ou sur embases à verrou** (pas de dupont) ; pièces hautes **collées**.
- **Masses étoile** : puissance (12 V/bobines) vs signal (3V3/MCU) jointes en 1 point près de J_PWR.
- **Vernis tropicalisation** après test ; composants **grade auto** si exposé.
- **Test** : alim seule d'abord (vérifier 12/5/3,3 V), puis throttle (mesurer la sortie), puis CAN
  (loopback 2 nœuds), puis reverse (boîtier relais, roue en l'air).
