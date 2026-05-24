# AYT PC BUILD - DISTRIBUTION
===========================

## État de cette distribution
--------------------------

[cite_start]Ce dossier contient l'outil PC qui remplace le builder Z80 runtime AYT[cite: 1]. [cite_start]Il lit un fichier `.ayt` source, génère directement le player Z80 spécialisé, relocalise les pointeurs de l'AYT runtime, puis produit les fichiers utilisables sur Amstrad CPC[cite: 2].

[cite_start]Dans cette distribution, les exemples fournis ont été générés avec[cite: 3]:
* [cite_start]`player_addr = #0040` [cite: 3]
* [cite_start]`loops = 2` [cite: 3]

[cite_start]Les résultats se trouvent dans : `dist\results\#40\`[cite: 3].

---

## Contenu du dossier
------------------

### Dossier `dist\`
* [cite_start]**`aytpcbuild.c`** : Source C99 portable[cite: 4].
* [cite_start]**`aytpcbuild.exe`** : Exécutable Windows déjà compilé[cite: 4].
* [cite_start]**`aytpcbuild-menu.bat`** : Menu interactif Windows pour lancer `aytpcbuild.exe` sans taper toute la ligne de commande[cite: 4].
* [cite_start]**`README.txt`** : Cette documentation[cite: 5].

### Dossier `dist\ayt\`
[cite_start]Fichiers AYT sources inclus[cite: 5]:
* [cite_start]`Arhey.ayt` [cite: 5]
* [cite_start]`complications.ayt` [cite: 5]
* [cite_start]`easy.ayt` [cite: 5]
* [cite_start]`stillscrolling.ayt` [cite: 5]
* [cite_start]`tatayoyo.ayt` [cite: 5]

### Dossier `dist\results\#40\`
[cite_start]Résultats prébuilt fournis, avec player en `#0040`[cite: 5].

---

## Compilation de l'outil
----------------------

**Windows avec GCC :**
```bash
gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe aytpcbuild.c
