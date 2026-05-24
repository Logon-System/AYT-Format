# AYT PC BUILD - DISTRIBUTION
===========================

## État de cette distribution
--------------------------

Ce dossier contient l'outil PC qui remplace le builder Z80 runtime AYT. Il lit un fichier `.ayt` source, génère directement le player Z80 spécialisé, relocalise les pointeurs de l'AYT runtime, puis produit les fichiers utilisables sur Amstrad CPC.

Dans cette distribution, les exemples fournis ont été générés avec :
* `player_addr = #0040`
* `loops = 2`

Les résultats se trouvent dans : `dist\results\#40\`.

---

## Contenu du dossier
------------------

### Dossier `dist\`
* **`aytpcbuild.c`** : Source C99 portable.
* **`aytpcbuild.exe`** : Exécutable Windows déjà compilé.
* **`aytpcbuild-menu.bat`** : Menu interactif Windows pour lancer `aytpcbuild.exe` sans taper toute la ligne de commande.
* **`README.txt`** : Cette documentation.

### Dossier `dist\ayt\`
Fichiers AYT sources inclus :
* `Arhey.ayt`
* `complications.ayt`
* `easy.ayt`
* `stillscrolling.ayt`
* `tatayoyo.ayt`

### Dossier `dist\results\#40\`
Résultats prébuilt fournis, avec player en `#0040`.

---

## Compilation de l'outil
----------------------

**Windows avec GCC :**
```bash
gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe aytpcbuild.c
