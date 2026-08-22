# AGENTS.md — Instructions pour les IA agentiques

Ce fichier s'adresse à un agent autonome (Claude Code ou équivalent) qui doit **reproduire,
poursuivre ou déboguer** ce projet sur une autre machine. Il condense le contexte et **les pièges
réellement rencontrés** (chacun a coûté du temps — lis-les).

---

## Contexte en une phrase

On pilote en **boucle fermée** une trottinette (Ecoxtrem M41 / ESC CHK2-K1-03) avec un **Arduino Uno R3** :
on **injecte** une tension d'accélérateur analogique (PWM+RC) et on **lit** la vitesse via le bus UART
de l'afficheur. Détails : `README.md` + `docs/`.

---

## Faits matériels essentiels (ne pas re-deviner)

- Arduino Uno R3, **logique 5 V**, FQBN **`arduino:avr:uno`**, port typique **`/dev/ttyACM0`**.
- Connecteur DASHBOARD 6-pin : **rouge=52 V ☠️**, **orangé=52 V commuté ☠️**, **noir=GND**,
  **bleu=UART 9600 8N1**, **gris/blanc=THROTTLE analogique (0,8→3,2 V)**, **violet=frein**.
- Le bus UART est de la **télémétrie**, PAS la commande moteur. La commande = le throttle analogique.
- Vitesse : champ `B8B9` des trames = **période**, `mph = 2520 / B8B9` (recalibré 2026-06-03 ; ex-667 faux), `EA60` = arrêt.
- Câblage boucle fermée : `D11 → R1k → (C10µF/GND) → gris/blanc (côté contrôleur, coupé)` ;
  `D10 → bleu (tap, ne pas couper)` ; `GND → noir` ; `D12` non connecté.

## ☠️ Règles de sécurité non négociables

1. **Jamais** l'Arduino (A0/D11/etc.) sur les fils **52 V** (rouge/orangé).
2. **Roue surélevée** pour tout test moteur.
3. Identifier les fils inconnus **au multimètre**, jamais avec A0 (max 5 V).
4. Monter les consignes **progressivement**. `x` = arrêt logiciel (≠ coupe-circuit).

---

## Pièges outillage (TESTÉS — éviter de les refaire)

### `arduino-cli`
- Peut être **absent** : l'installer en local sans sudo →
  `curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR="$HOME/.local/bin" sh`
  puis `arduino-cli core install arduino:avr`. (`./setup.sh` le fait.)
- `~/.local/bin` n'est pas toujours dans le PATH des shells **non interactifs** → préfixer
  `export PATH="$HOME/.local/bin:$PATH"` ou appeler le binaire en chemin absolu.
- Le fichier `.ino` **doit** être dans un dossier **du même nom** que lui.

### `arduino-cli monitor` en arrière-plan
- **Quitte immédiatement** (exit 0) si lancé en tâche de fond : son `stdin` est `/dev/null`,
  il lit un EOF et sort. ➡️ Pour streamer sans terminal interactif, utiliser un **logger pyserial**
  (`tools/monitor.py`).

### `pkill -f 'arduino-cli monitor'` 💥
- **Se tue lui-même** : la ligne de commande du shell qui lance le `pkill` contient la chaîne
  `arduino-cli monitor`, donc `pkill -f` matche son propre parent (exit 144).
  ➡️ Tuer **par PID** (`pgrep -f '[u]art_logger.py'` puis `kill <PID>`), ou exclure soi-même.

### Reset DTR sur ouverture du port
- Ouvrir `/dev/ttyACM0` avec pyserial **reset l'Uno** (impulsion DTR). Mettre `dtr=False; rts=False`
  **avant** `open()` réduit le risque, mais **ce n'est pas garanti** (le reset peut quand même survenir).
  ➡️ En tenir compte : après ouverture, attendre ~2 s le boot, puis `reset_input_buffer()`.
  Un reset relance le firmware **au repos** (sûr), donc ce n'est pas dangereux, juste à anticiper.

### Un seul process à la fois sur le port
- Le logger et un flash ne peuvent pas utiliser `/dev/ttyACM0` en même temps. **Tuer le logger
  (par PID) avant de flasher.**

---

## Workflow type

```bash
./setup.sh                                  # une fois : arduino-cli + core AVR
arduino-cli board list                      # confirmer le port / FQBN
./flash.sh <nom_firmware>                    # compile + flash (auto-port)
python3 tools/monitor.py                     # logger persistant -> /tmp/uart_monitor.log
python3 tools/send.py "c8"                   # envoyer une commande sans (trop) resetter
python3 tools/discover_speed.py              # tenir 2 paliers + analyser l'octet vitesse
```

`./flash.sh` sans argument liste les firmwares disponibles.

---

## Ordre de reproduction recommandé

1. `shadow_uart_sniffer_uno` → confirmer baud 9600 + trames `02 0E ...`.
2. Décoder (checksum XOR) — cf. `docs/02-protocol.md`.
3. Vérifier au multimètre le brochage 6-pin (cf. `docs/01`).
4. `throttle_injector_uno` → câbler PWM+RC sur le gris/blanc, accélérer.
5. `throttle_loop_uno` → ajouter le tap D10 sur le bleu, trouver/valider l'octet vitesse.
6. `throttle_pi_uno` → boucle fermée `c<mph>`, puis régler (cf. `docs/04`).

---

## Adapter à un AUTRE modèle de trottinette

Les couleurs/octets **changeront**. Refaire la démarche de `docs/03` :
sniffer → décoder → **tester si le bus commande vraiment le moteur** → multimètre →
injecter le throttle analogique → retrouver le champ vitesse → boucler.
Ne pas réutiliser `SPEED_K`, `CAL`, ni le mapping de couleurs aveuglément : **re-mesurer**.
