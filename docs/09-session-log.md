# 09 — Journal de session & état d'avancement (pour reprise)

> Pour tout membre/agent qui reprend le projet : **où on en est** et **ce qui reste**.
> Dernière session : packaging **v2**.

---

## ✅ Acquis (fonctionne, validé)

1. **Sniff + décodage UART** (fil bleu, 9600 8N1, trame 14 o, checksum XOR). → `docs/02`.
2. **Throttle analogique** (fil gris/blanc, 0,8 V repos → ~3,2 V) **injecté** par PWM+RC → la roue
   tourne. → `firmware/throttle_injector_uno`.
3. **Vitesse** décodée depuis la télémétrie : `mph = 2520 / B8B9` (B8B9 = période ; `EA60` = arrêt).
   ⚠️ Recalibré le 2026-06-03 : l'ancien `SPEED_K=667` était faux (afficheur en mph ; 126→20, 360→7).
4. **Boucle fermée PI** de vitesse (`c<mph>`) validée. → `firmware/throttle_pi_uno`.
5. **Marche arrière** : pas de reverse firmware (confirmé) → **plan relais** (swap phases+Hall). → `docs/05`.
6. **Conception** : carte ESP32 modulaire + anti-vibration + robot skid-steer 2 trottinettes + CAN. → `docs/06, 07, 08`.

## 🔎 En cours : caractériser le FREIN et le fil VIOLET

- **Frein détecté sur l'UART** : flag `B4/B6/B12 = 0xA0` quand on serre (confirmé). Fiable.
- **Fil violet** : **PAS un frein analogique** — c'est un **signal numérique/PWM** qui bascule
  **0 V ↔ ~4,8 V** en continu (≈ chaque <150 ms) **quand la comm tourne**. Au repos comm coupée il
  se fige (~3,36 V). **Nature exacte encore à déterminer** (PWM ? data ? lié à l'activité bus).
- Firmware d'investigation : `firmware/bus_logger_uno` (lecture seule : UART + 2 ADC) et
  `firmware/drive_logger_uno` (pilote le throttle **et** log, pour tester le frein **en roulant**).

## ⛔ Point de blocage à la dernière session (À RÉSOUDRE EN PREMIER)

Le **drive throttle de `drive_logger_uno` n'était pas connecté** : en commandant 1,6 V,
`spd=0`, `A1=0,00 V`, roue immobile. Le PWM D11 n'atteignait pas le gris/blanc.

**Reprise** : vérifier au multimètre (throttle tenu à 1,6 V), pointe noire sur GND :
1. broche **D11** → ~1,6 V (PWM moyenné) ?
2. **nœud après R 1 kΩ** (où arrivent C 10 µF, A1, gris/blanc) → ~1,6 V ?
3. **gris/blanc côté CONTRÔLEUR** → ~1,6 V ?

Rappels : le gris/blanc doit être **coupé** et on injecte **côté contrôleur** ; **GND commun** ;
A1 sur le même nœud. ⚠️ La trottinette **s'éteint seule** après quelques minutes → si plus aucune
trame UART (`trame=(aucune)`), **la rallumer** d'abord.

## ▶️ Prochaines étapes (ordre suggéré)

1. **Réparer le drive throttle** (multimètre ci-dessus) → refaire tourner la roue.
2. **Test frein en dynamique** : rouler (~1,5 V) puis freiner → observer chute de `spd`, flag UART,
   et comportement du violet en mouvement.
3. **Décoder le violet** (probable PWM/data) : si besoin, l'échantillonner plus vite ou en logique.
4. Caractériser le frein pour pouvoir **l'injecter** plus tard (autonomie totale).
5. Côté hardware : achat relais (≥80 A après mesure du courant) + câblage reverse (`docs/05`).
6. Côté carte : router le PCB ESP32 (`docs/08`) + protocole CAN (`docs/07`).

## 🧰 Rappels outils (pièges connus)

- `arduino-cli` dans `~/.local/bin` ; flasher via `./flash.sh <nom>`.
- `arduino-cli monitor` en arrière-plan **quitte** (stdin EOF) → utiliser `tools/monitor.py`.
- **Jamais `pkill -f 'arduino-cli monitor'`** (se tue lui-même) → tuer par PID.
- Ouvrir le port en pyserial peut **resetter** l'Uno (mettre `dtr=False`) → le firmware repart **au repos** (sûr).
- ☠️ **Jamais** l'Arduino sur rouge/orangé (**52 V**). 🛞 **Roue surélevée** pour tout test moteur.
