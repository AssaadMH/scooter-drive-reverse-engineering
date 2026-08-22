# 02 — Protocole UART afficheur ↔ contrôleur

> ⚠️ Rappel : ce bus est de la **télémétrie / affichage**. Il **ne commande pas** le moteur
> (l'accélérateur est analogique, cf. `01-hardware-wiring.md`). On l'utilise ici pour **lire la
> vitesse** en boucle fermée.

---

## 1. Couche physique

- **Fil** : bleu du connecteur DASHBOARD 6-pin.
- **Débit** : **9600 bauds**, **8N1** (auto-détecté par le sniffer via la largeur d'impulsion minimale).
- **Niveau** : compatible lecture directe par l'Uno (5 V tolérant).
- **Cadence** : ~10 trames/seconde.

---

## 2. Format de trame (14 octets)

```
 02   0E   01 00   [F]   00   [F]   [N]   [aH aL]   [A]   00   [F]   [XOR]
 B0   B1   B2 B3   B4    B5   B6    B7    B8   B9   B10   B11  B12   B13
```

| Octet | Nom | Valeur(s) | Signification |
|---|---|---|---|
| **B0** | STX | `0x02` | début de trame |
| **B1** | LEN | `0x0E` (=14) | longueur totale de la trame |
| **B2** | type | `0x01` | en-tête / type de message |
| **B3** | flag | `0x00` / `0x20` | bit d'état (bascule) |
| **B4** | flag | `0x80` (`0xA0` si frein) | drapeau ; passe à `A0` quand le frein est actionné |
| **B5** | flag | `0x00` / `0x20` | bit d'état (miroir de B11) |
| **B6** | flag | `0x80` (`0xA0` si frein) | idem B4 |
| **B7** | état | `0x00`..`0x09` | cran/compteur (varie quand le throttle est actif) |
| **B8 B9** | **VALEUR 16 bits** | voir §3 | **période (inverse vitesse)** / position throttle |
| **B10** | flag | `0x00`..`0x03` | bit « actif » |
| **B11** | flag | `0x00` / `0x20` | miroir de B5 |
| **B12** | flag | `0x80` (`0xA0` si frein) | idem B4/B6 |
| **B13** | **CHECKSUM** | XOR(B0..B12) | **somme de contrôle = OU-exclusif de tous les octets précédents** |

### Checksum (vérifié à 100 %)

```
B13 = B0 ^ B1 ^ B2 ^ ... ^ B12
```

Exemple : `02 0E 01 00 80 00 80 00 EA 60 00 00 80` → XOR = `07` ✅ (et la trame complète finit par `07`).

Code de vérification (C) :
```c
uint8_t x = 0;
for (uint8_t i = 0; i < 13; i++) x ^= frame[i];
bool ok = (x == frame[13]);
```

---

## 3. Champ valeur B8–B9 : période = inverse de la vitesse

C'est le point clé pour la boucle fermée.

| État | B8 B9 | Interprétation |
|---|---|---|
| **Roue arrêtée** | `EA 60` (= 60000) | **sentinelle « arrêt »** (période ~infinie) |
| **Roue lente** | grande valeur, **bruitée** | période longue |
| **Roue rapide** | petite valeur, **stable** | période courte |

➡️ La valeur **diminue** quand la vitesse **augmente** ⇒ c'est une **période**, pas une vitesse directe.

### Calibration vitesse (afficheur en mph)

> 🔧 **Recalibré le 2026-06-03.** L'ancienne valeur `SPEED_K ≈ 667` s'est révélée **fausse** sur
> cette trottinette (facteur ~3,8× ; l'afficheur est bien en **mph**). Valeur correcte ci-dessous.

Deux points frais relevés sur l'afficheur (mph), banc roue à vide :
- `B8B9 ≈ 126` ↔ **20 mph**
- `B8B9 ≈ 360` ↔ **~7 mph**

Loi retenue :

```
vitesse_mph = SPEED_K / B8B9        avec   SPEED_K ≈ 2520
```

Vérification : `2520 / 126 = 20,0 mph` ✅ ; `2520 / 360 = 7,0 mph` ✅

> ⚠️ Calibration **de banc** (roue à vide). À ré-étalonner sur route/en charge, et selon le
> diamètre de roue / réglages du contrôleur. Voir `04-closed-loop-tuning.md`.
> 🗄️ Obsolète : `SPEED_K=667` (points `82→8`, `113→6`) — incohérent avec les mesures fraîches.

### Filtrage recommandé

La période est **bruitée** à basse vitesse (roue libre). Utiliser un **filtre médian** (ex. 8
échantillons) avant de calculer la vitesse, et ignorer les valeurs aberrantes (`B8B9 < 30`).

---

## 4. Drapeau frein

Quand le frein est actionné, **B4 / B6 / B12 passent de `0x80` à `0xA0`** (bit `0x20`),
et le champ valeur revient à la sentinelle `EA 60`. Le frein est donc **tout-ou-rien** (numérique),
pas une intensité analogique.

---

## 5. Pseudo-code de parsing (longueur + checksum)

```c
// Reconstruit une trame : 0x02, octet longueur LEN, puis LEN-2 octets, checksum XOR en dernier.
static uint8_t buf[32], idx = 0, len = 0;
while (available()) {
  uint8_t b = read();
  if (idx == 0)       { if (b == 0x02) buf[idx++] = b; }      // cherche le STX
  else if (idx == 1)  { if (b >= 4 && b <= 32) { buf[idx++] = b; len = b; } else idx = 0; }
  else {
    buf[idx++] = b;
    if (idx >= len) {
      uint8_t x = 0; for (uint8_t i = 0; i < len-1; i++) x ^= buf[i];
      if (x == buf[len-1]) {                                  // trame valide
        // B8B9 = (buf[8]<<8) | buf[9]
        // si buf[8]==0xEA -> arrêt ; sinon vitesse = SPEED_K / B8B9
      }
      idx = 0; len = 0;
    }
  }
}
```
