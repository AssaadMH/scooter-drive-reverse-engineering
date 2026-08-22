# 01 — Matériel & câblage

Tout le câblage tourne autour du connecteur **DASHBOARD 6-pin** du contrôleur, qui regroupe
alimentation, masse, communication ET les signaux accélérateur/frein.

Voir l'image `harness_map.jpg` pour la carte complète des connecteurs du contrôleur.

---

## 1. Connecteurs du contrôleur (ESC CHK2-K1-03)

| Connecteur | Rôle |
|---|---|
| MOTOR | phases U/V/W + hall (prise combinée) |
| UART / PROGRAMMING (4-pin doré) | **libre / non connecté** — port de *flash firmware* du contrôleur |
| BATTERY | alimentation principale (prise XT) |
| **DASHBOARD (6-pin)** | **+ throttle + frein + comm** ← tout se passe ici |
| TURN / TAIL / HEAD / SIDE LIGHTS | éclairages |
| MOTOR #2 | 2ᵉ roue (version dual-motor) |

> ⚠️ Le boîtier **gris/alu**, c'est le **corps du contrôleur**, PAS un connecteur.
> Il n'y a **pas** de « prise TX » séparée : la comm est dans le 6-pin DASHBOARD.

---

## 2. Brochage du connecteur DASHBOARD 6-pin (confirmé au multimètre)

| Fil | Mesure | Rôle | Arduino ? |
|---|---|---|---|
| **Rouge** | **52 V** | Batterie + | ☠️ **JAMAIS** |
| **Orangé** | **52 V** quand allumé | +52 V commuté = ligne *power / enable* du contrôleur | ☠️ **JAMAIS** |
| **Noir** | 0 V | **GND** (négatif batterie) | ✅ masse commune |
| **Bleu** | signal data | **Comm UART** afficheur↔contrôleur, 9600 8N1. **E-06** sur l'afficheur si coupé | écoute (tap) |
| **Gris / blanc** | **0,8 V repos → ~3,2 V** (~8 mph à vide) | **🎯 THROTTLE (signal analogique)** | ✅ on injecte ici |
| **Violet** | 3–5 V variable | frein / signal | laisser branché |

> ⚠️ Les couleurs peuvent varier selon le lot. **Toujours re-vérifier au multimètre** :
> le throttle est le fil qui passe de **~0,8 V à ~3–4 V** quand on tourne la poignée.

### Comment identifier les fils (méthode multimètre, sans Arduino)

Référence = fil **noir** (GND). Mesurer chaque fil :
- **~52 V fixe** → batterie (rouge) ou enable (orangé) → ☠️ ne pas y toucher.
- **0 V** → GND (noir).
- **0,8 V au repos, monte à ~3–4 V quand on tourne la poignée** → **THROTTLE** (gris/blanc).
- **valeur qui change au freinage** → frein (violet).
- **valeur instable / qui « grésille »** → comm UART (bleu).

---

## 3. Pourquoi le bus UART ne pilote PAS le moteur

Test décisif : on coupe le fil **bleu** (comm) →
- l'afficheur affiche **E-06** (erreur de communication) et vitesse 0,
- **mais la roue tourne quand même** quand on accélère.

➡️ Donc l'accélérateur atteint le contrôleur par un **fil analogique séparé** (le gris/blanc),
indépendant de l'UART. Le bus UART (bleu) ne sert qu'à **l'affichage / la télémétrie**.

---

## 4. Schéma de câblage final (boucle fermée)

```
   ┌──────────────────────── ARDUINO UNO R3 (5V) ────────────────────────┐
   │                                                                      │
   │   D11 ──[ R 1 kΩ ]──┬─────────────►  fil GRIS/BLANC (côté CONTRÔLEUR) │  THROTTLE (injection)
   │                     │                                                │
   │                  [ C 10 µF ]                                         │
   │                     │                                                │
   │   GND ──────────────┴─────────────►  fil NOIR                        │  GND commun
   │                                                                      │
   │   D10 ────────────────────────────►  fil BLEU (tap PARALLÈLE)        │  ÉCOUTE vitesse (ne pas couper)
   │                                                                      │
   │   D12 ───  (non connecté)                                            │  TX factice imposé par SoftwareSerial
   └──────────────────────────────────────────────────────────────────────┘

   Fils LAISSÉS branchés afficheur↔contrôleur : rouge(52V), orangé(52V), noir, bleu, violet
   → le contrôleur reste alimenté/activé, l'afficheur fonctionne (pas d'E-06).

   ☠️  NE JAMAIS relier l'Arduino aux fils ROUGE / ORANGÉ (52 V).
```

### Détails importants

- **Throttle** : on **coupe** le fil gris/blanc et on injecte **côté contrôleur**. Si on ne
  coupe pas, l'accélérateur réel (≈0,8 V) « tire » la ligne et entre en conflit avec l'Arduino.
- **Écoute vitesse** : le fil bleu est **tappé en parallèle** (dérivation), **PAS coupé** →
  l'afficheur continue de fonctionner, pas d'E-06.
- **Filtre RC** : transforme le PWM (490 Hz sur D11) en tension continue ≈ `rapport_cyclique × 5 V`.
  Si la tension réelle au fil diffère de la commande (atténuation/charge), ajuster `CAL` dans le firmware
  (`CAL = tension_mesurée / tension_commandée`). Pour une précision supérieure, remplacer le RC par un
  **DAC MCP4725** (I²C) ou un potentiomètre numérique.
- **Niveau logique** : l'Uno est en 5 V. Si l'entrée throttle du contrôleur attend du 3,3 V,
  prévoir un diviseur/level-shifter. Ici l'échelle 0,8–3,2 V est compatible directement.

---

## 5. Brochage Arduino récapitulatif

| Broche Arduino | Direction | Connexion | Firmware concerné |
|---|---|---|---|
| D11 | sortie (PWM) | → R 1 kΩ → (C 10 µF/GND) → fil gris/blanc | throttle_injector, throttle_loop, throttle_pi |
| D10 | entrée (SoftwareSerial RX) | ← fil bleu (tap) | sniffer, throttle_loop, throttle_pi |
| D12 | (SoftwareSerial TX factice) | non connecté | sniffer, throttle_loop, throttle_pi |
| GND | masse | ← fil noir | tous |
| D10 (RX) / D11 (TX dummy) | sniffer seul | D10←data, D11 non utilisé | shadow_uart_sniffer / injector |

> Note : le **sniffer** d'origine utilise D10=RX et D11=TX factice. Dès qu'on **injecte** le throttle,
> D11 devient la sortie PWM et le TX factice de SoftwareSerial migre sur **D12**.
