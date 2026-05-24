# AYT PC BUILD - DISTRIBUTION
--------------------------

## Etat de cette distribution

Ce dossier contient l'outil PC qui remplace le *builder Z80 runtime AYT*.

Il lit un fichier .ayt source, genere directement le player Z80 spécialisé, relocalise les pointeurs de l'AYT runtime, puis produit les fichiers utilisables sur Amstrad CPC.

Dans cette distribution, les exemples fournis ont été générés avec:

		player_addr = #0040
		loops       = 2

Les resultats sont dans:

		dist\results\#40\


## Contenu du dossier

		dist\
			aytpcbuild.c
			- Source C99 portable.

			aytpcbuild.exe
			- Exécutable Windows déjà compilé.

			aytpcbuild-menu.bat
			- Menu interactif Windows pour lancer aytpcbuild.exe sans taper toute la ligne de commande.

		ayt\
		Fichiers AYT sources inclus:
			Arhey.ayt
			complications.ayt
			easy.ayt
			stillscrolling.ayt
			tatayoyo.ayt

		results\#40\
			Résultats prebuild fournis, avec player en #0040.


## Compilation de l'outil


Windows avec GCC:

		gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe aytpcbuild.c

Linux ou macOS:

		cc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild aytpcbuild.c

Le code C n'utilise pas de dépendance externe.


## Script interactif Windows

Depuis dist\, lancer:

		aytpcbuild-menu.bat

Le script demande:

		- le fichier AYT à traiter;
		- le cas de generation: call_two, call_bundle, sp_two ou sp_bundle;
		- l'adresse du player;
		- l'adresse du runtime AYT;
		- le nombre de loops;
		- les adresses program_addr et return_addr en mode SP/JP;
		- le dossier de sortie;
		- la génération ou non du mini ASM et du rapport TXT.

Le script lance uniquement aytpcbuild.exe.

Il ne lance pas maxam.exe et ne cree pas de *-demo.bin.

### Exemple avec tatayoyo.ayt

Depuis dist\, lancer:

		aytpcbuild-menu.bat

Exemple de réponses pour générer un bundle CALL de tatayoyo.ayt:

*Choix AYT*
		- Choisir tatayoyo.ayt dans la liste.


| Option | Valeur paramètre | 
| :-----------: | :--------------: |
| Cas de génération  | 2. CALL - bundle compact  |
| Adresse CPC du player | 0x0666  |
| Adresse AYT initiale pour l'outil| 0x1000  |
| Nombre de lectures / loops |  2 |
|  Dossier racine de sortie |  results\manual |
|  Dossier final | results\manual\call_bundle\tatayoyo |
|  Générer le mini ASM de test |    O |
|  Générer le rapport TXT |    O |

Le script lance alors aytpcbuild.exe et produit:

		results\manual\call_bundle\tatayoyo\tatayoyo.bundle.bin
		results\manual\call_bundle\tatayoyo\tatayoyo.bundle.asm
		results\manual\call_bundle\tatayoyo\tatayoyo.report.txt

Le fichier ASM genere reste portable dans son dossier. Son INCBIN ne contient que le nom du fichier binaire, pas le chemin complet:
		incbin "tatayoyo.bundle.bin"

Il ne faut donc pas obtenir:
		incbin "results\manual\call_bundle\tatayoyo\tatayoyo.bundle.bin"

### Syntaxe générale

		aytpcbuild --input music.ayt --player-addr 0x0100 \
		--ayt-addr 0x1000 --loops N --mode jp|call [--return-addr ADDR] \
		[--out-player player.bin] [--out-ayt-runtime music.runtime.ayt] \
		[--out-bundle runtime.bin] [--bundle-base ADDR] \
		[--out-asm-two test-two.asm] [--out-asm-bundle test-bundle.asm] \
		[--program-addr ADDR] \
		[--report report.txt]


#### Options

		--input fichier.ayt
			Fichier AYT source.

		--player-addr ADDR
			Adresse CPC où le player généré sera chargé.

		--ayt-addr ADDR
			Adresse CPC de l'AYT runtime dans le cas deux fichiers.

		Si --out-bundle est demande, l'outil produit toujours un bundle compact.
		Dans ce cas, l'AYT runtime est placé juste après le player et son adresse est recalculée automatiquement. La valeur --ayt-addr sert alors d'adresse initiale avant compactage.

		--loops N
			Nombre de lectures de la musique, de 0 a 255.
			Les exemples fournis utilisent --loops 2.

		--mode call
			Génère un player appelé avec:
				call AYT_Player
			Dans ce mode, le player sauvegarde et restaure SP lui-même.

		--mode jp
			Génère le mode "rapide" SP/JP.
			Le programme doit appeler le player avec:
				jp AYT_Player
				Le player utilise SP comme pointeur rapide, puis revient par un JP vers l'adresse fournie par --return-addr.

		--return-addr ADDR
			Obligatoire en mode jp.
			Pour les ASM de test generes par l'outil:
				return_addr = program_addr + #24

		--program-addr ADDR
				Adresse du mini programme ASM de test généré par --out-asm-two ou --out-asm-bundle.
  			En mode call, cette option est facultative.
			Si elle est absente, l'outil place le launcher a #0500 quand c'est possible, sinon juste après les donnees générées.
			En mode jp, cette option est optionnelle mais doit rester cohérente avec --return-addr.

		--out-player fichier.bin
			Ecrit le player Z80 généré.

		--out-ayt-runtime fichier.ayt
			Ecrit l'AYT runtime, avec les pointeurs de séquences relocalisés.

		--out-bundle fichier.bin
			Ecrit un bundle compact:
				player + ayt-runtime
			Il n'y a aucun trou de remplissage dans ce fichier.

		--bundle-base ADDR
			Base du bundle.
			Dans cette version, cette adresse doit correspondre a --player-addr.

		--out-asm-two fichier.asm
			Génère un mini source ASM de test pour le cas deux fichiers:
				player.bin + runtime.ayt
				Cette option demande aussi --out-player et --out-ayt-runtime.

		--out-asm-bundle fichier.asm
			Génère un mini source ASM de test pour le cas bundle:
				bundle.bin
				Cette option demande aussi --out-bundle.

		--report fichier.txt
			Ecrit un rapport texte lisible: tailles, adresses, registres actifs,  bloc d'init, coût CPU, fichiers générés et RUN des ASM de test.

		--help
			Affiche l'aide courte.


### Les 4 cas générés

Pour chaque AYT, quatre cas sont fournis dans dist\results\#40:

#### 1. call_two

Mode CALL, deux fichiers:
		<nom>.player.bin
		<nom>.runtime.ayt
		<nom>.two.asm
		<nom>.two-demo.bin
		<nom>.report.txt

#### 2. call_bundle
Mode CALL, bundle compact:

		<nom>.bundle.bin
		<nom>.bundle.asm
		<nom>.bundle-demo.bin
		<nom>.report.txt

#### 3. sp_two
Mode SP/JP, deux fichiers:

		<nom>.player.bin
		<nom>.runtime.ayt
		<nom>.two.asm
		<nom>.two-demo.bin
		<nom>.report.txt

#### 4. sp_bundle
Mode SP/JP, bundle compact:

		<nom>.bundle.bin
		<nom>.bundle.asm
		<nom>.bundle-demo.bin
		<nom>.report.txt


### Arborescence des resultats #0040

		dist\results\#40\call_two\Arhey\
		dist\results\#40\call_two\complications\
		dist\results\#40\call_two\easy\
		dist\results\#40\call_two\stillscrolling\

		dist\results\#40\call_bundle\Arhey\
		dist\results\#40\call_bundle\complications\
		dist\results\#40\call_bundle\easy\
		dist\results\#40\call_bundle\stillscrolling\

		dist\results\#40\sp_two\Arhey\
		dist\results\#40\sp_two\complications\
		dist\results\#40\sp_two\easy\
		dist\results\#40\sp_two\stillscrolling\

		dist\results\#40\sp_bundle\Arhey\
		dist\results\#40\sp_bundle\complications\
		dist\results\#40\sp_bundle\easy\
		dist\results\#40\sp_bundle\stillscrolling\


### Exemples de commandes

Les exemples ci-dessous sont a lancer depuis dist\.

#### CALL deux fichiers, exemple Arhey:

		.\aytpcbuild.exe --input ayt\Arhey.ayt ^
		--player-addr 0x0040 ^
		--ayt-addr 0x1000 ^
		--loops 2 ^
		--mode call ^
		--out-player "results\#40\call_two\Arhey\Arhey.player.bin" ^
		--out-ayt-runtime "results\#40\call_two\Arhey\Arhey.runtime.ayt" ^
		--out-asm-two "results\#40\call_two\Arhey\Arhey.two.asm" ^
		--report "results\#40\call_two\Arhey\Arhey.report.txt"

#### CALL bundle compact, exemple Arhey:

		.\aytpcbuild.exe --input ayt\Arhey.ayt ^
		--player-addr 0x0040 ^
		--ayt-addr 0x1000 ^
		--loops 2 ^
		--mode call ^
		--out-bundle "results\#40\call_bundle\Arhey\Arhey.bundle.bin" ^
		--out-asm-bundle "results\#40\call_bundle\Arhey\Arhey.bundle.asm" ^
		--report "results\#40\call_bundle\Arhey\Arhey.report.txt"

#### SP/JP deux fichiers, exemple Arhey:

		.\aytpcbuild.exe --input ayt\Arhey.ayt ^
		--player-addr 0x0040 ^
		--ayt-addr 0x1000 ^
		--loops 2 ^
		--mode jp ^
		--program-addr 0x1F47 ^
		--return-addr 0x1F6B ^
		--out-player "results\#40\sp_two\Arhey\Arhey.player.bin" ^
		--out-ayt-runtime "results\#40\sp_two\Arhey\Arhey.runtime.ayt" ^
		--out-asm-two "results\#40\sp_two\Arhey\Arhey.two.asm" ^
		--report "results\#40\sp_two\Arhey\Arhey.report.txt"

#### SP/JP bundle compact, exemple Arhey:

		.\aytpcbuild.exe --input ayt\Arhey.ayt ^
		--player-addr 0x0040 ^
		--ayt-addr 0x1000 ^
		--loops 2 ^
		--mode jp ^
		--program-addr 0x10EA ^
		--return-addr 0x110E ^
		--out-bundle "results\#40\sp_bundle\Arhey\Arhey.bundle.bin" ^
		--out-asm-bundle "results\#40\sp_bundle\Arhey\Arhey.bundle.asm" ^
		--report "results\#40\sp_bundle\Arhey\Arhey.report.txt"

### Assembler les ASM de test

Les fichiers .asm générés sont compatibles avec maxam.

Deux solutions sont possibles:

- utiliser maxam.exe en local, fourni dans le dossier parent du projet;
- utiliser Maxam Web:
		https://amstrad.neocities.org/maxamweb/

Dans les deux cas, le source ASM doit avoir accès aux fichiers appelés par INCBIN. 

Les ASM générés par aytpcbuild utilisent uniquement le nom du fichier, par exemple:
		incbin "tatayoyo.bundle.bin"

Il suffit donc de garder le .asm et le .bin référence dans le meme dossier, ou de les fournir ensemble a Maxam Web si l'interface le demande.

Exemple depuis:
		dist\results\#40\call_two\Arhey\

Assembler le test:
		..\..\..\..\..\maxam.exe Arhey.two.asm Arhey.two-demo.bin

Exemple depuis:
		dist\results\#40\call_bundle\Arhey\

Assembler le test:
		..\..\..\..\..\maxam.exe Arhey.bundle.asm Arhey.bundle-demo.bin


## Recapitulatif des AYT inclus

Tous les exemples ci-dessous utilisent:

	player_addr = #0040
	loops       = 2

### CALL deux fichiers:

| Nom AYT | AYT Runtime | Taille Player | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
| Arhey.ayt | #1000 | 359 bytes | #1F47 |
| complications.ayt | #1000 | 359 bytes | #381C |
|  easy.ayt | #1000 | 343 bytes | #1C79 |
|  stillscrolling.ayt | #1000 |359 bytes |#3F9B |


### CALL bundle compact:

| Nom AYT | AYT Runtime | Taille Player | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
|  Arhey.ayt | #01A7 |4270 bytes | #10EE |
|  complications.ayt | #01A7 | 10627 bytes | #29C3 |
|  easy.ayt | #0197 | 3536 bytes |#0E10 |
|  stillscrolling.ayt| #01A7 |12546 bytes | #3142|

### SP/JP deux fichiers:
| Nom AYT | AYT Runtime | Taille Player | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
|  Arhey.ayt | #1000 | 355 bytes |#1F47 |#1F6B |
|  complications.ayt |#1000 |355 bytes | #381C | #3840 |
|  easy.ayt | #1000 | 339 bytes | #1C79 | #1C9D |
| stillscrolling.ayt | #1000 |355 bytes | #3F9B| #3FBF|

### SP/JP bundle compact:

| Nom AYT | AYT Runtime | Taille Player | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
|  Arhey.ayt |#01A3 |4266 bytes| #10EA|#110E|
| complications.ayt |#01A3| 10623 bytes|#29BF| #29E3|
|  easy.ayt | #0193| 3532 bytes| #0E0C |#0E30 |
| stillscrolling.ayt | #01A3 | 12542 bytes | #313E |#3162|


## Notes importantes

1. --out-bundle génère toujours un bundle compact.
L'ancien mode avec trous n'existe plus.

2. En bundle, l'AYT runtime est placé immédiatement après le player.
Le rapport indique l'adresse finale.

3. En mode SP/JP, --return-addr est obligatoire.
Pour les ASM de test générés par l'outil:
		return_addr = program_addr + #24

4. Les fichiers .runtime.ayt ne sont pas des AYT source originaux.
Ils contiennent les pointeurs relocalisés pour l'adresse CPC finale.

5. Le player prebuild ne dépend plus du builder ASM Z80 à l'exécution.
Le builder est remplacé par l'outil PC aytpcbuild.

