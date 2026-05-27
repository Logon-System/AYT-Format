# aytforge - convertisseur PT1/PT2/PT3/SQT/YM/PSG vers AYT + builders CPC/CPC+

---

## 1. Rôle de l'outil

aytforge est un outil en ligne de commande qui transforme des musiques AY/YM
en fichiers AYT.

Il accepte en entrée :

- `.pt1`
- `.pt2`
- `.pt3`
- `.sqt`
- `.ym`
- `.ym5`
- `.ym6`
- `.psg`

Les fichiers YM attendus sont des YM5/YM6 "LeOnArD!" non compressés. Les
archives YM emballées, les variantes compressées, ou les fichiers avec données
spéciales non prises en charge doivent être convertis en dump YM5/YM6 simple
avant utilisation.

Les fichiers PSG attendus sont des flux de registres AY au format AY-Emul/VT :
en-tête "PSG" + 0x1A, commandes 0xFF/0xFE/0xFD, et écritures de registres.
R13 est traité comme un événement : les frames sans écriture R13 reçoivent le
sentinel 0xFF pour ne pas relancer l'enveloppe.

Les fichiers SQT attendus sont des modules compilés SQ-Tracker. Ce format est
importé par une logique séparée de PT1/PT2/PT3 : ses positions combinent trois
patterns de canal avec volume, transposition, flags d'effets et vitesse.

La chaîne complète est :

```
source PT1/PT2/PT3/SQT/YM/PSG
  -> frames de registres AY/YM
  -> correction d'horloge si nécessaire
  -> compression AYT
  -> génération optionnelle d'un player CPC/CPC+ préconfiguré
```

aytforge ne sert pas à éditer ni jouer la musique. Il sert à produire un AYT
compact et, si demandé, un ensemble binaire/ASM prêt à intégrer côté CPC.

---

## 2. Origine du convertisseur AYT

La partie encodage/compression AYT vient du travail de Logon System, par Siko,
dans le dépôt GitHub :

  https://github.com/Logon-System/AYT-Format

aytforge intègre ce cœur AYT dans un CLI unique, avec entrée PT1/PT2/PT3/SQT/YM/PSG,
correction d'horloge, batch, récursivité, mode interactif et builders CPC/CPC+.

Le dossier maxam contient l'assembleur Z80 utilisé pour assembler les essais
finaux ASM/bundle et produire des SNA/DSK si besoin.

---

## 3. Compilation

Le projet est en C++17 et n'a pas de dépendance externe obligatoire.

Prérequis :

| Plateforme | Compilateur |
|------------|-------------|
| Windows    | g++ MinGW-w64 récent, ou autre compilateur C++17 équivalent |
| Linux      | g++ ou clang++ avec support C++17 |
| macOS      | clang++ récent via Xcode Command Line Tools ou Homebrew |

> **Important :** `std::filesystem` doit être disponible. Sur de très vieux GCC, il
> peut être nécessaire d'ajouter `-lstdc++fs` en fin de commande.

### 3.1 Compilation Windows

Depuis le dossier qui contient ce README :

**PowerShell :**

```powershell
g++ -std=c++17 -O2 -Wall src\aytforge.cpp src\pt3_bridge.cpp src\psg_loader.cpp src\ayt_encoder.cpp src\builder_cpc.cpp src\builder_cpcplus.cpp third_party\ym2ayt\YMParser.cpp third_party\ym2ayt\ayt.cpp third_party\ym2ayt\optimizers.cpp third_party\ym2ayt\optimizer_ga.cpp third_party\ym2ayt\optimizer_ils.cpp third_party\ym2ayt\optimizer_sa.cpp third_party\ym2ayt\optimizer_tabu.cpp third_party\ym2ayt\platforms.cpp -o aytforge.exe -static
```

**cmd.exe :**

```bat
g++ -std=c++17 -O2 -Wall ^
  src\aytforge.cpp src\pt3_bridge.cpp src\psg_loader.cpp src\ayt_encoder.cpp src\builder_cpc.cpp src\builder_cpcplus.cpp ^
  third_party\ym2ayt\YMParser.cpp ^
  third_party\ym2ayt\ayt.cpp ^
  third_party\ym2ayt\optimizers.cpp ^
  third_party\ym2ayt\optimizer_ga.cpp ^
  third_party\ym2ayt\optimizer_ils.cpp ^
  third_party\ym2ayt\optimizer_sa.cpp ^
  third_party\ym2ayt\optimizer_tabu.cpp ^
  third_party\ym2ayt\platforms.cpp ^
  -o aytforge.exe -static
```

> **Attention :** le caractère `^` sert à continuer une ligne dans cmd.exe. Dans
> PowerShell, utilisez la commande sur une seule ligne ci-dessus.

**Exécution :**

```powershell
.\aytforge.exe --help
```

### 3.2 Compilation Linux

Depuis le dossier qui contient ce README :

```bash
c++ -std=c++17 -O2 -Wall \
  src/aytforge.cpp src/pt3_bridge.cpp src/psg_loader.cpp src/ayt_encoder.cpp src/builder_cpc.cpp src/builder_cpcplus.cpp \
  third_party/ym2ayt/YMParser.cpp \
  third_party/ym2ayt/ayt.cpp \
  third_party/ym2ayt/optimizers.cpp \
  third_party/ym2ayt/optimizer_ga.cpp \
  third_party/ym2ayt/optimizer_ils.cpp \
  third_party/ym2ayt/optimizer_sa.cpp \
  third_party/ym2ayt/optimizer_tabu.cpp \
  third_party/ym2ayt/platforms.cpp \
  -o aytforge
```

**Exécution :**

```bash
./aytforge --help
```

Dans les exemples ci-dessous, les commandes Windows utilisent `.\aytforge.exe`.
Sous Linux et macOS, remplacez simplement par `./aytforge`.

### 3.3 Compilation macOS

Depuis le dossier qui contient ce README :

```bash
clang++ -std=c++17 -O2 -Wall \
  src/aytforge.cpp src/pt3_bridge.cpp src/psg_loader.cpp src/ayt_encoder.cpp src/builder_cpc.cpp src/builder_cpcplus.cpp \
  third_party/ym2ayt/YMParser.cpp \
  third_party/ym2ayt/ayt.cpp \
  third_party/ym2ayt/optimizers.cpp \
  third_party/ym2ayt/optimizer_ga.cpp \
  third_party/ym2ayt/optimizer_ils.cpp \
  third_party/ym2ayt/optimizer_sa.cpp \
  third_party/ym2ayt/optimizer_tabu.cpp \
  third_party/ym2ayt/platforms.cpp \
  -o aytforge
```

**Exécution :**

```bash
./aytforge --help
```

---

## 4. Utilisation minimale

Convertir un PT3 vers AYT, cible CPC par défaut :

```powershell
.\aytforge.exe "music.pt3"
```

Convertir un PT2 vers AYT :

```powershell
.\aytforge.exe "music.pt2"
```

Convertir un PT1 vers AYT :

```powershell
.\aytforge.exe "music.pt1"
```

Convertir un SQT vers AYT :

```powershell
.\aytforge.exe "music.sqt"
```

Convertir un YM vers AYT :

```powershell
.\aytforge.exe "music.ym"
```

Convertir un YM6 :

```powershell
.\aytforge.exe "music.ym6"
```

Convertir un PSG :

```powershell
.\aytforge.exe "music.psg"
```

Convertir sans générer le player CPC/CPC+ :

```powershell
.\aytforge.exe "music.pt3" --build-player none
```

Choisir le dossier de sortie :

```powershell
.\aytforge.exe "music.pt3" --out-dir ayt
```

Faire seulement une analyse, sans écrire de fichier :

```powershell
.\aytforge.exe "music.pt3" --dry-run
```

Afficher plus de détails :

```powershell
.\aytforge.exe "music.pt3" -v
```

---

## 5. Plusieurs fichiers, batch et récursivité

Donner plusieurs fichiers explicitement :

```powershell
.\aytforge.exe "a.pt1" "b.pt2" "c.pt3" "d.sqt" "e.ym" "f.ym6" "g.psg" --out-dir ayt
```

Traiter tous les fichiers compatibles dans le dossier courant, sans descendre
dans les sous-dossiers :

```powershell
.\aytforge.exe "." --out-dir ayt
```

Traiter uniquement les PT1/PT2/PT3/SQT du dossier courant :

```powershell
.\aytforge.exe "*.pt1" "*.pt2" "*.pt3" "*.sqt" --out-dir ayt
```

Traiter plusieurs masques :

```powershell
.\aytforge.exe "*.pt1" "*.pt2" "*.pt3" "*.sqt" "*.ym" "*.ym5" "*.ym6" "*.psg" --out-dir ayt
```

Traiter un dossier complet récursivement :

```powershell
.\aytforge.exe -r "mods" --out-dir ayt
```

Traiter le dossier courant et tous ses sous-dossiers :

```powershell
.\aytforge.exe -r "." --out-dir ayt
```

La récursivité n'a pas de profondeur maximale codée. Elle descend dans tous les
sous-dossiers accessibles par le système de fichiers. Les limites pratiques
sont les droits d'accès, les chemins trop longs, et le comportement du système
face aux liens symboliques.

En batch, une erreur sur un fichier est signalée mais le traitement continue
sur les fichiers suivants. À la fin, l'outil affiche le nombre d'échecs. Le
code retour vaut 0 si tout a réussi, 1 si au moins une conversion a échoué.

---

## 6. Horloges, machines et correction de fréquence

Les musiques AY/YM peuvent venir de machines dont l'horloge sonore diffère.
aytforge peut corriger les périodes pour adapter la musique à une cible.

Options principales :

```
--source zx128|pentagon|zxuno|custom
--source-clock HZ
--target cpc|cpcplus|msx|zx128|pentagon|atari|vg5000|custom
--target-clock HZ
--rate 50|25|60|30|100|200|300
--no-scale
```

Par défaut :

| Paramètre | Valeur |
|-----------|--------|
| source PT1/PT2/PT3/SQT | zx128, 1773400 Hz |
| cible | cpc, 1000000 Hz |
| rate | 50 Hz |

La valeur `--source auto` est acceptée par le CLI. Pour un PT1/PT2/PT3/SQT, elle revient
au profil zx128 par défaut. Pour un YM, le meilleur automatisme consiste à ne pas
forcer `--source` : l'outil relit alors l'horloge depuis l'en-tête YM si elle est
présente.

Pour un fichier YM5/YM6, si aucune source n'est forcée, l'outil utilise
l'horloge et la fréquence de replay stockées dans l'en-tête YM quand elles
existent.

Pour un fichier PSG, le flux contient déjà des écritures de registres mais pas
de point de boucle fiable ni d'horloge source portable. Par défaut, aytforge
utilise donc le profil source zx128 et 50 Hz. Si le PSG vient déjà d'un CPC,
utilisez `--source cpc --target cpc`, ou `--no-scale` si aucune correction de
fréquence n'est souhaitée.

Exemples :

```powershell
.\aytforge.exe "music.pt3" --source zx128 --target cpc
.\aytforge.exe "music.pt3" --source pentagon --target cpc
.\aytforge.exe "music.ym" --target cpc
.\aytforge.exe "music.ym" --source custom --source-clock 2000000 --target cpc
.\aytforge.exe "music.psg" --source zx128 --target cpc
.\aytforge.exe "music.psg" --source cpc --target cpc
```

Désactiver toute correction de fréquence :

```powershell
.\aytforge.exe "music.pt3" --no-scale
```

---

## 7. Qualité et compression AYT

Options principales :

```
--quality fast|balanced|best
--pattern-size fast|auto|full|N|MIN:MAX/STEP
--optim greedy|sa|ga|tabu|ils|none
--sa-gen-min N
--sa-gen-max N
--sa-gen-ext N
--no-mask
--normalize-patterns
--r13-filter
--export-all-regs
```

### Presets

| Option | Description |
|--------|-------------|
| `--quality fast` | Rapide, utilise greedy et pattern-size fast. |
| `--quality balanced` | Compromis vitesse/taille, utilise auto et recuit simulé plus court. |
| `--quality best` | Valeur par défaut, teste le plus large intervalle de pattern-size. |

### Pattern-size

| Valeur | Description |
|--------|-------------|
| `fast` | Teste 4..64 par pas de 4. |
| `auto` | Teste 8..96 par pas de 4. |
| `full` | Teste 1..255 par pas de 1. |
| `N` | Force une seule taille de pattern, entre 1 et 255. |
| `MIN:MAX/STEP` | Teste un intervalle personnalisé. |

Exemples :

```powershell
.\aytforge.exe "music.pt3" --quality fast
.\aytforge.exe "music.pt3" --quality best
.\aytforge.exe "music.pt3" --pattern-size full --optim sa
.\aytforge.exe "music.pt3" --pattern-size 16 --optim greedy
.\aytforge.exe "music.pt3" --pattern-size 1:255/1 --optim ils
```

### Notes sur les méthodes d'optimisation

| Méthode | Description |
|---------|-------------|
| `none` | Pas d'optimisation d'ordre avancé. |
| `greedy` | Méthode rapide et déterministe. |
| `sa` | Simulated annealing / recuit simulé. Bon compromis pour chercher une meilleure taille. |
| `ga` | Genetic algorithm / algorithme génétique. Mode expert. |
| `tabu` | Tabu search. Mode expert. |
| `ils` | Iterative local search. Peut être lent, mais peut trouver plus petit. |

---

## 8. PT1, PT2, PT3, SQT et TurboSound

Options :

```
--chip auto|all|1|2
--no-pt36-ts
--max-frames N
```

Par défaut, aytforge traite automatiquement les modules trouvés dans le PT3.
Pour un PT1, un PT2, un SQT ou un PT3 standard, cela donne un seul AYT. Pour un PT3
TurboSound, l'outil peut produire une sortie par chip.

`--max-frames` sert aussi de limite de sécurité pour le rendu PT1/PT2/SQT et le
décodage PSG. Les entrées PT1, PT2, SQT, YM et PSG sont mono-chip dans aytforge :
`--chip` doit rester sur `auto/all/1` pour ces formats.

SQT est importé via la couche SQ-Tracker de VortexTracker : le format assemble
des patterns de canal par position et n'a pas le même modèle interne que PT1,
PT2 ou PT3. Une fois rendu en registres AY, la compression AYT et les builders
CPC/CPC+ restent strictement communs.

Exemples :

```powershell
.\aytforge.exe "music.pt3" --chip auto
.\aytforge.exe "music.pt3" --chip 1
.\aytforge.exe "music.pt3" --chip 2
```

Désactiver l'heuristique TurboSound PT3.6 :

```powershell
.\aytforge.exe "music.pt3" --no-pt36-ts
```

Limiter la durée analysée :

```powershell
.\aytforge.exe "music.pt3" --max-frames 12000
```

---

## 9. Point de boucle musical

aytforge conserve le point de boucle musical quand il existe dans la source.

- Pour un PT1/PT2/PT3/SQT, le point de boucle est déduit de la position de loop du module.
- Pour un YM5/YM6, le point de boucle vient du champ `loopFrame` de l'en-tête YM.
- Pour un PSG, aucun point de boucle fiable n'est déduit : le `loopFrame` reste à 0.

Le format AYT boucle sur une entrée de séquence. En pratique, cela veut dire
que le point de boucle doit tomber sur une frontière de pattern AYT pour être
exact à la frame près.

Comportement attendu :

- Si `loopFrame` est aligné avec la pattern-size retenue, la boucle est exacte.
- Sinon, la boucle est placée au mieux sur une frontière compatible AYT.
- La musique n'est pas forcée à reboucler au début sauf si la source boucle elle-même au début.
- Un avertissement est affiché si le point de boucle n'est pas aligné.

> **Important :** cette notion de `loopFrame` musical est différente de l'option
> `--loops` du builder player. `--loops` règle le compteur de répétitions injecté
> dans le player ; il ne change pas le point de boucle extrait du PT1/PT2/PT3/SQT ou du YM.

---

## 10. Builders CPC / CPC+

Quand la cible est CPC ou CPC+, aytforge peut générer en plus du AYT un
player préconfiguré et un petit lanceur ASM.

La conversion PT1/PT2/PT3/SQT/YM/PSG vers AYT est strictement commune. La divergence commence
uniquement après la compression AYT :

- `cpc`     → builder CPC old RAM, player PPI/AY direct
- `cpcplus` → builder CPC+ RAM, player DMA/ASIC

Options :

```
--build-player auto|cpc|cpcplus|none
--case call-bundle|call-two|sp-bundle|sp-two
--player-addr 0x0040
--ayt-addr 0x1000
--loops N
--program-addr 0xNNNN
--return-addr 0xNNNN
--asic-in on|off
--asic-out on|off
--dma-channel 0|1|2
--asic-page-on 0xB8
--asic-page-off 0xA0
--no-asm
--no-report
```

Par défaut :

| Option | Valeur |
|--------|--------|
| `--target` | `cpc` |
| `--build-player` | `auto` |
| `--case` | `call-bundle` |
| `--player-addr` | `0x0040` |
| `--loops` | `255` |

Avec `--target cpcplus`, `--build-player auto` sélectionne le builder CPC+ RAM.
Avec `--target cpc`, il sélectionne le builder CPC old RAM.

### 10.1 Sorties générées

**Sortie commune :**

- `music.ayt` — fichier AYT compressé.

**En mode bundle :**

- `music.bundle.bin` — player + AYT runtime dans un seul binaire compact.
- `music.bundle.asm` — exemple ASM qui incbin le bundle et appelle le player.
- `music.build.txt` — rapport d'adresses, tailles, CPU et fichiers produits.

**En mode two-file :**

- `music.player.bin` — blob player seul.
- `music.runtime.ayt` — AYT relogé pour l'adresse runtime demandée.
- `music.two.asm` — exemple ASM qui incbin le player et le runtime AYT.
- `music.build.txt` — rapport d'adresses, tailles, CPU et fichiers produits.

Désactiver l'ASM généré :

```powershell
.\aytforge.exe "music.pt3" --no-asm
```

Désactiver le rapport :

```powershell
.\aytforge.exe "music.pt3" --no-report
```

### 10.2 Les quatre cas builder

#### call-bundle

Cas recommandé pour commencer. Un seul binaire contient le player et l'AYT
runtime. `--player-addr` est l'adresse de chargement du bundle. L'adresse AYT
runtime est recalculée automatiquement juste après le player.

```powershell
.\aytforge.exe "music.pt3" --case call-bundle --player-addr 0x0040
```

#### call-two

Deux binaires séparés : player et AYT runtime. `--player-addr` charge le player,
`--ayt-addr` charge le runtime AYT.

```powershell
.\aytforge.exe "music.pt3" --case call-two --player-addr 0x0040 --ayt-addr 0x1000
```

#### sp-bundle

Comme call-bundle, mais le lanceur utilise un mode JP/SP. C'est un mode plus
avancé, utile si l'intégration finale veut éviter un CALL classique.

```powershell
.\aytforge.exe "music.pt3" --case sp-bundle --player-addr 0x0040
```

#### sp-two

Deux fichiers séparés, avec mode JP/SP.

```powershell
.\aytforge.exe "music.pt3" --case sp-two --player-addr 0x0040 --ayt-addr 0x1000
```

### 10.2.1 Options CPC+

Le builder CPC+ RAM ajoute la gestion de la page ASIC et du canal DMA :

```
--target cpcplus
--build-player cpcplus
--asic-in on|off
--asic-out on|off
--dma-channel 0|1|2
--asic-page-on 0xB8
--asic-page-off 0xA0
```

Par défaut, le builder CPC+ utilise le canal DMA 0 et suppose la page ASIC
connectée à l'entrée et à la sortie du player :

```
--asic-in on --asic-out on --dma-channel 0
```

La page ASIC occupe `#4000-#7FFF` quand elle est connectée. Le blob player CPC+
généré par aytforge, c'est-à-dire code player, routine d'initialisation,
padding et liste DMA, ne doit jamais recouvrir cette fenêtre.

Le lanceur ASM généré par aytforge ne doit pas non plus s'exécuter dans
`#4000-#7FFF`, car la page ASIC peut être connectée pendant l'appel du player. En
placement automatique, aytforge déplace donc `MyProgram` à `#8000` si le bundle est
trop gros et pousserait le lanceur dans cette fenêtre. Si `--program-addr` force
explicitement une adresse dans `#4000-#7FFF` sur CPC+, la génération est refusée
avec un message explicite.

Le runtime AYT peut être placé n'importe où en RAM. S'il touche ou traverse
`#4000-#7FFF`, le player généré le code de connexion/déconnexion ASIC nécessaire
selon `--asic-in` et `--asic-out`. Le rapport `.build.txt` indique explicitement :

```
AYT in ASIC range : yes/no
B0 ASIC off block : yes/no
```

Le blob player CPC+ contient le code player, la routine d'initialisation AY
éventuelle, les éventuels octets de padding, et la liste DMA alignée sur une
adresse paire sans franchir une page de 256 octets.

Les valeurs RMR2 par défaut sont `#B8` pour connecter la page ASIC et `#A0` pour
la déconnecter. Elles peuvent être modifiées avec `--asic-page-on` et
`--asic-page-off` si votre programme utilise une autre ROM basse ou un autre
mapping.

Exemple CPC+ bundle :

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-bundle --player-addr 0x0100
```

Exemple CPC+ two-file avec AYT à #8000 :

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-two --player-addr 0x0100 --ayt-addr 0x8000
```

Exemple CPC+ two-file avec AYT dans la fenêtre ASIC :

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case call-two --player-addr 0x2200 --ayt-addr 0x5000 --program-addr 0x9000
```

Exemple CPC+ SP/JP avec adresse de retour automatique :

```powershell
.\aytforge.exe "music.pt3" --target cpcplus --case sp-two --player-addr 0x3200 --ayt-addr 0x8000 --program-addr 0x9000
```

### 10.3 Exemple du ASM généré

En mode bundle CPC+, le fichier `.bundle.asm` ressemble à ceci :

```asm
;; AYT prebuilt bundle test / CALL mode
;; Generated by aytforge.

AYT_Bundle equ #0040
AYT_Player equ AYT_Bundle
AYT_Init   equ #01AE
MyProgram  equ #0500

        org AYT_Bundle
        incbin "music.bundle.bin"

        org MyProgram
        run $

StartExample
        di
        ld sp,MyStack
        ...
        call AYT_Init
        ei
        ...
        call AYT_Player
        ...
        jr MainLoop
```

En mode two-file CPC+, le fichier `.two.asm` place deux binaires :

```asm
AYT_Player equ #0040
AYT_Init   equ #01AE
AYT_File   equ #1000
MyProgram  equ #0500

        org AYT_Player
        incbin "music.player.bin"

        org AYT_File
        incbin "music.runtime.ayt"

        org MyProgram
        run $
```

Le lanceur ASM est un exemple d'intégration. Il sert à valider rapidement le
chargement, les adresses, l'initialisation AY et l'appel du player. Sur CPC+,
il inclut aussi le déverrouillage ASIC, le paramétrage de la page ASIC, une
attente VSync et le couple ei/halt/halt utilisé dans les exemples de test.
Sur CPC old RAM, l'exemple généré est plus simple et n'a pas de symbole
`AYT_Init` séparé : l'initialisation éventuelle est intégrée au player généré.
Dans un vrai programme CPC, vous pouvez reprendre uniquement les adresses, les
binaires produits et l'appel au player correspondant à votre architecture.

---

## 11. Mode interactif

Le mode interactif pose les questions principales et propose des valeurs par
défaut. Son affichage est en anglais pour rester cohérent avec le CLI, l'aide
en ligne et les messages générés par l'outil.

Lancer :

```powershell
.\aytforge.exe "music.pt3" --interactive
```

Avec affichage de la commande complète équivalente :

```powershell
.\aytforge.exe "music.pt3" --interactive --print-command
```

Les questions oui/non se répondent avec `y` ou `n`. Les anciennes réponses `o/oui`
sont encore acceptées, mais elles ne sont plus affichées par défaut.

Le mode interactif demande notamment :

- source d'horloge
- cible AYT
- mode de qualité
- construction ou non du player CPC/CPC+
- cas builder
- adresses importantes
- nombre de boucles
- paramètres CPC+ ASIC/DMA/RMR2 si la cible est cpcplus
- impression ou non de la commande complète

Les choix d'optimisation proposés :

| Choix | Description |
|-------|-------------|
| `1. fast` | greedy + pattern-size rapide. |
| `2. best` | recuit simulé + pattern-size full 1..255. |
| `3. expert` | choix manuel de pattern-size, méthode d'optimisation, masking, R13, etc. |

Extrait typique :

```
aytforge interactive assistant
Source clock profile (auto/zx128/pentagon/zxuno/custom) [zx128]:
AYT target (cpc/cpcplus/msx/zx128/pentagon/atari/vg5000) [cpc]: cpcplus
Frequency scaling [y]:
Optimization choice [2]:
Build CPC/CPC+ prebuilt player [y]:
Selected builder: CPC+ RAM
Builder case (call-bundle/call-two/sp-bundle/sp-two) [call-bundle]:
Player address [0x0100]:
Loops [255]:
ASM launcher program address (auto/0xNNNN) [auto]:
CPC+ ASIC page at player entry (on/off) [on]:
CPC+ ASIC page after player exit (on/off) [on]:
CPC+ DMA channel (0/1/2) [0]:
CPC+ RMR2 ASIC page on value [0x00B8]:
CPC+ RMR2 ASIC page off value [0x00A0]:
```

En mode bundle interactif, seule l'adresse player est demandée : c'est l'adresse
de chargement du bundle. L'adresse AYT runtime est calculée automatiquement
juste après le player.

En mode two-file interactif, l'adresse player et l'adresse AYT runtime sont
toutes les deux demandées.

En mode CPC+ interactif, aytforge rappelle que le blob player généré ne doit
pas recouvrir `#4000-#7FFF`, et que le lanceur ASM généré ne doit pas non plus
s'exécuter dans cette fenêtre. L'adresse du lanceur peut rester en auto :
aytforge la déplace hors `#4000-#7FFF` si le bundle est trop gros. Si une
adresse `--program-addr` forcée tombe dans cette fenêtre, la génération est
refusée. Si le runtime AYT commence dans cette fenêtre, ou s'il commence avant
`#4000` et peut la traverser selon sa taille, l'outil affiche un message
explicite et le rapport final tranche avec "AYT in ASIC range".

En mode SP/JP interactif, l'adresse de retour peut rester en auto. Si vous
indiquez une adresse de retour manuelle, elle doit correspondre au point de
retour réel de votre lanceur.

---

## 12. Commandes utiles

Aide :

```powershell
.\aytforge.exe --help
```

Conversion simple PT3 vers CPC :

```powershell
.\aytforge.exe "music.pt3"
```

Conversion simple PT2 vers CPC :

```powershell
.\aytforge.exe "music.pt2"
```

Conversion simple PT1 vers CPC :

```powershell
.\aytforge.exe "music.pt1"
```

Conversion simple SQT vers CPC :

```powershell
.\aytforge.exe "music.sqt"
```

Conversion simple YM vers CPC :

```powershell
.\aytforge.exe "music.ym"
```

Conversion simple PSG vers CPC :

```powershell
.\aytforge.exe "music.psg"
```

AYT seul, sans builder :

```powershell
.\aytforge.exe "music.pt3" --build-player none
```

Batch du dossier courant :

```powershell
.\aytforge.exe "." --out-dir ayt
```

Batch récursif :

```powershell
.\aytforge.exe -r "." --out-dir ayt
```

Compression rapide :

```powershell
.\aytforge.exe "music.pt3" --quality fast
```

Compression maximale raisonnable :

```powershell
.\aytforge.exe "music.pt3" --quality best
```

Compression lente/expert :

```powershell
.\aytforge.exe "music.pt3" --pattern-size full --optim ils
```

Forcer une source Pentagon vers CPC :

```powershell
.\aytforge.exe "music.pt3" --source pentagon --target cpc
```

Forcer une horloge custom :

```powershell
.\aytforge.exe "music.ym" --source custom --source-clock 2000000 --target cpc
```

Générer un bundle CPC à #4000 :

```powershell
.\aytforge.exe "music.pt3" --case call-bundle --player-addr 0x4000
```

Générer deux fichiers CPC :

```powershell
.\aytforge.exe "music.pt3" --case call-two --player-addr 0x0040 --ayt-addr 0x4000
```

Imprimer une commande complète reproductible :

```powershell
.\aytforge.exe "music.pt3" --interactive --print-command
```

---

## 13. Référence rapide des options

### Entrées/sorties

| Option | Description |
|--------|-------------|
| `-o, --output FILE` | Force le nom du `.ayt` quand il n'y a qu'une seule sortie. |
| `-P, --out-dir DIR` | Dossier de sortie. Alias long : `--out-dir`. |
| `-r, --recursive` | Active la recherche dans les sous-dossiers. |
| `--dry-run, --analyze` | Analyse et compresse en mémoire, mais n'écrit rien. |
| `-v, --verbose` | Sortie plus détaillée. |
| `-q, --quiet` | Sortie minimale. |

### Machines/fréquences

| Option | Description |
|--------|-------------|
| `--source NAME` | Machine source connue : `auto`, `zx128`, `pentagon`, `zxuno`, `custom`. |
| `--source-clock HZ` | Horloge source manuelle, utile avec `--source custom`. |
| `--target NAME` | Machine cible connue : `cpc`, `cpcplus`, `msx`, `zx128`, `pentagon`, `atari`, `vg5000`, `custom`. |
| `--target-clock HZ` | Horloge cible manuelle, utile avec `--target custom`. |
| `--rate N` | Fréquence de replay. |
| `--no-scale` | Ne corrige pas les périodes. |

### Compression

| Option | Description |
|--------|-------------|
| `--quality fast\|balanced\|best` | Preset global. |
| `--pattern-size ...` | Plage de tailles de patterns AYT à tester. |
| `--optim ...` | Méthode d'optimisation. |
| `--no-mask` | Désactive le pattern masking. |
| `--normalize-patterns` | Normalise les bits ignorés avant compression. |
| `--r13-filter` | Filtre certaines répétitions de R13. |
| `--export-all-regs` | Force l'export de tous les registres. |

### PT1 / PT2 / PT3 / SQT

| Option | Description |
|--------|-------------|
| `--chip auto\|all\|1\|2` | Sélectionne les chips/modules PT3. PT1, PT2 et SQT sont mono-chip. |
| `--no-pt36-ts` | Désactive l'heuristique TurboSound PT3.6. |
| `--max-frames N` | Limite le nombre de frames rendues depuis le PT1/PT2/PT3/SQT ou décodées depuis le PSG. |

### CPC / CPC+

| Option | Description |
|--------|-------------|
| `--build-player auto\|cpc\|cpcplus\|none` | Active ou désactive le builder player. En auto, `cpc` sélectionne le builder CPC old RAM et `cpcplus` sélectionne le builder CPC+ RAM. |
| `--case ...` | Choisit le type de sortie player. |
| `--player-addr 0xNNNN` | Adresse du player, ou du bundle en mode bundle. |
| `--ayt-addr 0xNNNN` | Adresse du runtime AYT en mode two-file. |
| `--loops N` | Compteur de répétitions injecté dans le player. Défaut : 255. Ne modifie pas le point de boucle musical PT1/PT2/PT3/SQT/YM/PSG. |
| `--asic-in on\|off` | CPC+ seulement. État attendu de la page ASIC à l'entrée du player. |
| `--asic-out on\|off` | CPC+ seulement. État souhaité de la page ASIC à la sortie du player. |
| `--dma-channel 0\|1\|2` | CPC+ seulement. Canal DMA utilisé par le player. Défaut : 0. |
| `--asic-page-on 0xNN` | CPC+ seulement. Valeur RMR2 utilisée pour connecter la page ASIC. |
| `--asic-page-off 0xNN` | CPC+ seulement. Valeur RMR2 utilisée pour déconnecter la page ASIC. |
| `--program-addr 0xNNNN` | Adresse du programme ASM exemple. Sur CPC+, elle ne doit pas être dans `#4000-#7FFF`. |
| `--return-addr 0xNNNN` | Adresse de retour pour les modes SP/JP. |
| `--no-asm` | Ne génère pas le lanceur ASM. |
| `--no-report` | Ne génère pas le rapport `.build.txt`. |

### Debug

| Option | Description |
|--------|-------------|
| `--emit-ym` | Écrit aussi un dump YM6 intermédiaire. |
| `--print-command` | Affiche la commande complète équivalente, pratique après le mode interactif ou pour rendre un batch reproductible. |
