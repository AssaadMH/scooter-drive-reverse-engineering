# 04 — Calibration & réglage de la boucle fermée

Firmware concerné : `firmware/throttle_pi_uno/throttle_pi_uno.ino`.

---

## 1. Constantes à connaître

| `#define` | Valeur par défaut | Rôle |
|---|---|---|
| `IDLE_V` | `0.80` | tension throttle au repos (V) |
| `THRESH_V` | `1.35` | feedforward : tension qui amorce le mouvement |
| `MAX_V` | `2.20` | **plafond de sécurité** du throttle (V) |
| `ABS_MAX_V` | `3.40` | garde-fou dur, jamais dépassé |
| `CAL` | `1.00` | calibration tension = `V_mesurée_au_fil / V_commandée` |
| `SPEED_K` | `2520.0` | conversion : `mph = SPEED_K / B8B9` (recalibré 2026-06-03 ; ex-667 faux) |
| `KP` | `0.060` | gain proportionnel (V par mph d'erreur) |
| `KI` | `0.030` | gain intégral (V par mph·s) |
| `CTRL_MS` | `150` | période de régulation (ms) |
| `SLEW` | `0.12` | variation throttle max par cycle (anti-à-coup) |
| `NPER` | `8` | taille du filtre médian de période |

---

## 2. Calibration de la tension throttle (`CAL`)

Le filtre RC peut atténuer légèrement la tension réellement vue par le contrôleur.

1. Flasher `throttle_injector_uno`, commander une tension connue (ex. `2.0`).
2. **Mesurer au multimètre** la vraie tension sur le fil gris/blanc.
3. Si elle diffère : `CAL = tension_mesurée / 2.0`.
4. Re-flasher. Désormais les volts commandés ≈ volts réels.

Pour une précision parfaite (sans atténuation ni ondulation) : remplacer le PWM+RC par un **DAC MCP4725**.

---

## 3. Calibration de la vitesse (`SPEED_K`)

`mph = SPEED_K / B8B9`. Pour ré-étalonner :

1. Flasher `throttle_loop_uno` (affiche `thr | trame hex`) **ou** `throttle_pi_uno` (affiche `meas`).
2. Tenir une vitesse stable, **lire le km/h (ou mph) de l'afficheur** et noter `B8B9`.
3. `SPEED_K = vitesse_affichée × B8B9`.
4. Idéalement faire 2 points et moyenner.

Exemple banc (recalibré 2026-06-03, afficheur en mph) : `20 mph × 126 = 2520`, `7 mph × 360 = 2520`
→ `SPEED_K ≈ 2520`. *(Ancien : `8×82`, `6×113` → 667, obsolète/faux pour cette trottinette.)*

> En **km/h** : remplacer les lectures mph par des km/h ; `SPEED_K` s'exprimera alors en km/h·unité.

---

## 4. Réglage du PI

Comportement observé à vide (consigne `c7`) : la roue décolle, le throttle se stabilise ~1,5 V,
`meas` oscille **5,4–9,3 mph** autour de 7.

Causes de l'oscillation **à vide** :
- la vitesse **sature** ~6–8 mph (bande de réglage étroite, très sensible) ;
- la **période est bruitée** à basse vitesse.

### Pour lisser

| Symptôme | Action |
|---|---|
| Oscillation rapide | **baisser `KP`** (0,06 → 0,03) |
| Bruit de mesure | **augmenter `NPER`** (8 → 16) |
| Réponse trop molle | augmenter `KP`, ou réduire `CTRL_MS` |
| Dépassement au démarrage | baisser `THRESH_V`, réduire `SLEW` |
| Erreur statique persistante | augmenter légèrement `KI` (attention au windup) |

> Sur route (en charge), la plage de vitesse s'ouvre et la régulation devient **nettement plus stable** :
> les gains de banc sont volontairement prudents.

### Anti-windup

L'intégrale est bornée par `INTEG_CLAMP` (±8). En cas de saturation prolongée, réduire cette borne.

---

## 5. Commandes du firmware `throttle_pi_uno`

| Commande (moniteur série 115200) | Effet |
|---|---|
| `c8` | **boucle fermée** : tenir 8 mph (`c0` = repos, boucle off) |
| un nombre, ex `1.5` | **mode manuel** : throttle = 1,5 V (coupe la boucle) |
| `n` | repos (0,8 V), boucle off |
| `x` | **ARRÊT d'urgence** (repos immédiat, boucle off) |
| `?` | état : mode, consigne, vitesse mesurée, throttle |

Affichage périodique (500 ms) : `LOOP set=7.0 meas=6.2mph thr=1.55V`.

---

## 6. Sécurités intégrées

- Démarrage **au repos** (0,8 V).
- Plafond `MAX_V` + garde-fou dur `ABS_MAX_V`.
- **Slew-rate** : pas d'à-coup de throttle.
- **Perte de télémétrie > 1,5 s** en boucle → retour automatique au repos.
- `x` = arrêt immédiat.

> ⚠️ Ces sécurités **logicielles** ne remplacent pas un **coupe-circuit physique** ni la
> **roue surélevée**. Toujours garder la coupure d'alimentation à portée.
