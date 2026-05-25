# PT3 - Convertisseur PT3 vers YM6
--------------------------

Ce dossier contient un outil en ligne de commande portable, écrit en C,
qui convertit des modules Pro Tracker 3 (.pt3) en fichiers YM6 (.ym).

Le principe n'est pas de jouer le morceau en audio, mais de rejouer la
logique tracker à 50 Hz afin de produire, frame par frame, les registres
AY/YM nécessaires au format YM6.


## Fichiers principaux

		pt3.c
			: code source C99 portable.

		pt3.exe
			: binaire Windows compilé.


## Compilation

L'outil n'a pas de dépendance externe. Il utilise uniquement la bibliothèque C
standard et les API système de base pour parcourir les dossiers.


### Windows avec MinGW / TDM-GCC

Depuis le dossier contenant pt3.c :

		gcc -O2 -std=c99 -Wall -Wextra -static -o pt3.exe pt3.c

L'option -static permet de produire un .exe plus autonome. Si votre toolchain
ne supporte pas bien le statique, vous pouvez retirer -static :

		gcc -O2 -std=c99 -Wall -Wextra -o pt3.exe pt3.c


### Windows avec MSVC Build Tools

Depuis un "Developer Command Prompt" de Visual Studio :

		cl /nologo /O2 /TC pt3.c /Fe:pt3.exe


### Linux

Depuis le dossier contenant pt3.c :

		cc -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

Puis lancement :

		./pt3 fichier.pt3


### macOS

Avec clang, fourni par les outils de développement Apple :

		clang -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

ou simplement :

		cc -O2 -std=c99 -Wall -Wextra -o pt3 pt3.c

Puis lancement :

		./pt3 fichier.pt3


## Utilisation de base

Convertir un fichier PT3 en YM6, avec un nom de sortie automatique :

		pt3 musique.pt3

Sur Linux/macOS :

		./pt3 musique.pt3

Le fichier produit sera :

		musique.ym

Choisir explicitement le fichier de sortie :

		pt3 musique.pt3 sortie.ym

ou :

		pt3 musique.pt3 -o sortie.ym


## Conversion par lot

Convertir tous les .pt3 du dossier courant :

		pt3 "*.pt3"

Sur Linux/macOS :

		./pt3 "*.pt3"

Convertir tous les .pt3 d'un dossier :

		pt3 "mods/*.pt3"

Envoyer les fichiers .ym dans un dossier de sortie existant :

		pt3 "mods/*.pt3" -o ym6

Important : en mode batch, si -o est utilisé avec plusieurs fichiers d'entrée,
la destination doit être un dossier qui existe déjà.

En cas d'erreur sur un fichier pendant un batch, l'outil continue avec les
fichiers suivants. Le code de retour final vaut 1 si au moins une conversion
a échoué.


## Conversion récursive

L'option -r, ou --recursive, permet de parcourir les sous-dossiers.

Convertir tous les .pt3 présents dans un dossier et tous ses sous-dossiers :

		pt3 -r mods

Sur Linux/macOS :

		./pt3 -r mods

Envoyer tous les .ym dans un dossier de sortie existant :

		pt3 -r mods -o ym6

Utiliser un motif de fichier récursif :

		pt3 -r "mods/*.pt3" -o ym6

Avec -r dossier, seuls les fichiers se terminant par .pt3 sont collectés.
Avec -r "dossier/*.pt3", le motif est appliqué dans le dossier indiqué et
dans ses sous-dossiers.

Remarque importante : avec -o dossier, la sortie est aplatie dans ce dossier.

Par exemple :

		mods/a/theme.pt3
		mods/b/theme.pt3

produiront tous les deux :

		ym6/theme.ym

Le second peut donc écraser le premier. Pour éviter ce cas, lancez la
conversion sans -o afin que les .ym soient créés à côté des .pt3, ou traitez
les sous-dossiers séparément.


## La récursivité fonctionne-t-elle sous Linux et macOS ?

Oui, elle est prévue pour fonctionner sous Windows, Linux et macOS.

Le code utilise deux chemins selon la plateforme :

		Windows       : FindFirstFileA / FindNextFileA
		Linux/macOS   : opendir / readdir / stat

Les fonctions opendir, readdir et stat sont des API POSIX standard, disponibles
sur Linux et macOS. Le même code source doit donc compiler et parcourir les
sous-dossiers sur ces systèmes.

La version testée localement ici l'a été sous Windows. La partie Linux/macOS
est écrite avec des API POSIX classiques, mais il faudra idéalement faire une
compilation de validation sur une vraie machine Linux/macOS avant diffusion.


## Options disponibles

		-o FILE
			Fichier de sortie en conversion simple.

		-o DIR
			Dossier de sortie en batch ou en récursif. Le dossier doit exister.

		-r, --recursive
			Parcourt récursivement les sous-dossiers.

		--chip N
			Exporte uniquement la puce N, en commençant à 1.
			Utile pour les fichiers TurboSound.

		--no-pt36-ts
			Désactive l'heuristique de détection TurboSound pour certains PT3.6.

		--clock HZ
			Indique la fréquence master clock écrite dans l'en-tête YM.
			La valeur par défaut est 1773400.

		--max-frames N
			Limite de sécurité pendant le rendu.

		-h, --help
			Affiche l'aide.


## TurboSound

Quand un fichier PT3 contient plusieurs puces AY/YM, l'outil exporte un fichier
YM par puce, comme Vortex Tracker.

Exemple :

		musique.1.ym
		musique.2.ym

Pour exporter uniquement la première puce :

		pt3 musique.pt3 --chip 1

Pour exporter uniquement la deuxième puce :

		pt3 musique.pt3 --chip 2


## Format YM produit

L'outil produit un flux YM6 :

		- frames à 50 Hz ;
		- registres AY/YM exportés frame par frame ;
		- données non entrelacées ;
		- registre 13 écrit avec le marqueur 0xFF quand l'enveloppe ne doit pas
		  être redéclenchée.

Ce comportement est aligné sur l'export YM de Vortex Tracker.


## Exemples rapides

Un seul fichier :

		pt3 "Mast - Insomnia (1999).pt3"

Tous les PT3 du dossier courant :

		pt3 "*.pt3"

Tous les PT3 d'un dossier et de ses sous-dossiers :

		pt3 -r "pt3-2025-2026"

Tous les PT3 récursivement vers un dossier ym6 existant :

		pt3 -r "pt3-2025-2026" -o ym6

Avec une clock différente :

		pt3 musique.pt3 --clock 1750000
