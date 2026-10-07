# AYT PC BUILD - DISTRIBUTION
--------------------------

## Etat de cette distribution

Ce dossier contient l'outil PC qui remplace le *builder Z80 runtime AYT*.

Il lit un fichier .ayt source, génère directement le player Z80 spécialisé, relocalise les pointeurs de l'AYT runtime, puis produit les fichiers utilisables sur Amstrad CPC.

Les exemples de ce guide utilisent les fichiers .ayt présents à la **racine du dépôt**, dans `ayt-files/`, avec:

		player_addr = #0040
		loops       = 2

Les résultats ne sont pas fournis pré-générés: ils sont créés par les commandes ci-dessous (dossier `out/`).


## Contenu du dossier

		players/AYT-Builder-C/CPC/
			bin/aytpcbuild.c
			- Source C99 portable.

			bin/build/aytpcbuild-win.exe
			- Exécutable Windows déjà compilé.

			bin/build/aytpcbuild-menu.bat
			- Menu interactif Windows pour lancer l'exécutable sans taper toute la ligne de commande.

			web/aytpcbuild-web.html
			- Application web (navigateur).

		ayt-files/   (à la racine du dépôt)
		Fichiers AYT sources utilisés dans les exemples:
			david_whittaker_demo.ayt
			med-Merci_Monsieur.ayt
			med-My_Ai_Is_Dumb.ayt
			still_scrolling.ayt


## Compilation de l'outil

Depuis la racine du dépôt.

Linux ou macOS:

		cc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild players/AYT-Builder-C/CPC/bin/aytpcbuild.c

Windows avec GCC:

		gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe players\AYT-Builder-C\CPC\bin\aytpcbuild.c

Le code C n'utilise pas de dépendance externe.

Windows sans compilateur: utiliser directement `players\AYT-Builder-C\CPC\bin\build\aytpcbuild-win.exe` à la place de `aytpcbuild`.

Dans les exemples suivants, `aytpcbuild` désigne l'exécutable compilé, lancé depuis la racine du dépôt.


## Premier exemple (5 minutes)

Générer, pour la musique `ayt-files/still_scrolling.ayt`, un bundle (player + données) chargé en #0040, avec un petit programme de test:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
			--player-addr 0x0040 --ayt-addr 0x1000 --loops 2 --mode call \
			--out-bundle out/still_scrolling.bundle.bin \
			--out-asm-bundle out/still_scrolling.bundle.asm \
			--report out/still_scrolling.report.txt

(créer le dossier `out/` au préalable: `mkdir out`)

L'outil affiche:

		AYT PC build ok: player=359 bytes, ayt=11051 bytes at #01A7, cpu=463 nops, init block present

et produit dans `out/`:

		still_scrolling.bundle.bin     (11410 octets = player 359 + AYT runtime 11051, chargé en #0040)
		still_scrolling.bundle.asm     (programme de test, RUN #2CD2)
		still_scrolling.report.txt     (rapport lisible)

Il reste à assembler le programme de test (voir [Assembler les ASM de test](#assembler-les-asm-de-test)):

		cd out
		<assembleur> still_scrolling.bundle.asm still_scrolling.bundle-demo.bin

Le binaire obtenu se charge en #0040 et se lance en #2CD2.


## Script interactif Windows

Le script `bin\build\aytpcbuild-menu.bat` s'utilise avec l'exécutable à côté de lui, sous le nom `aytpcbuild.exe`, et lit les `.ayt` du sous-dossier `ayt\` du dossier courant. Préparer donc un dossier de travail:

		copier  players\AYT-Builder-C\CPC\bin\build\aytpcbuild-menu.bat   -> travail\
		copier  players\AYT-Builder-C\CPC\bin\build\aytpcbuild-win.exe    -> travail\aytpcbuild.exe
		copier  ayt-files\*.ayt                                           -> travail\ayt\

Puis, depuis `travail\`, lancer:

		aytpcbuild-menu.bat

Le script demande:

		- le fichier AYT à traiter;
		- le cas de génération: call_two, call_bundle, sp_two ou sp_bundle;
		- l'adresse du player;
		- l'adresse du runtime AYT;
		- le nombre de loops;
		- les adresses program_addr et return_addr en mode SP/JP;
		- le dossier de sortie;
		- la génération ou non du mini ASM et du rapport TXT.

Le script lance uniquement l'exécutable aytpcbuild. Il n'assemble rien et ne crée pas de *-demo.bin.

### Exemple avec still_scrolling.ayt

Exemple de réponses pour générer un bundle CALL de still_scrolling.ayt:

*Choix AYT*
		- Choisir still_scrolling.ayt dans la liste.


| Option | Valeur paramètre | 
| :-----------: | :--------------: |
| Cas de génération  | 2. CALL - bundle compact  |
| Adresse CPC du player | 0x0040  |
| Adresse AYT initiale pour l'outil| 0x1000  |
| Nombre de lectures / loops |  2 |
|  Dossier racine de sortie |  results\manual |
|  Dossier final | results\manual\call_bundle\still_scrolling |
|  Générer le mini ASM de test |    O |
|  Générer le rapport TXT |    O |

Le script lance alors aytpcbuild et produit:

		results\manual\call_bundle\still_scrolling\still_scrolling.bundle.bin
		results\manual\call_bundle\still_scrolling\still_scrolling.bundle.asm
		results\manual\call_bundle\still_scrolling\still_scrolling.report.txt

Le fichier ASM généré reste portable dans son dossier. Son INCBIN ne contient que le nom du fichier binaire, pas le chemin complet:
		incbin "still_scrolling.bundle.bin"

Il ne faut donc pas obtenir:
		incbin "results\manual\call_bundle\still_scrolling\still_scrolling.bundle.bin"


## Application Web Html/Js

Ouvrir dans un navigateur:

		players/AYT-Builder-C/CPC/web/aytpcbuild-web.html

puis charger un ou plusieurs fichiers `.ayt` (par exemple ceux de `ayt-files/`).


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

		Si --out-bundle est demandé, l'outil produit toujours un bundle compact.
		Dans ce cas, l'AYT runtime est placé juste après le player et son adresse est recalculée automatiquement. La valeur --ayt-addr sert alors d'adresse initiale avant compactage.

		--loops N
			Nombre de lectures de la musique, de 0 à 255.
			Les exemples de ce guide utilisent --loops 2.

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
			Pour les ASM de test générés par l'outil:
				return_addr = program_addr + #24

		--program-addr ADDR
				Adresse du mini programme ASM de test généré par --out-asm-two ou --out-asm-bundle.
  			En mode call, cette option est facultative.
			Si elle est absente, l'outil place le launcher à #0500 quand c'est possible, sinon juste après les données générées.
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
			Dans cette version, cette adresse doit correspondre à --player-addr.

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


### Les 4 cas de génération

#### 1. call_two

Mode CALL, deux fichiers:
		<nom>.player.bin
		<nom>.runtime.ayt
		<nom>.two.asm
		<nom>.report.txt

#### 2. call_bundle
Mode CALL, bundle compact:

		<nom>.bundle.bin
		<nom>.bundle.asm
		<nom>.report.txt

#### 3. sp_two
Mode SP/JP, deux fichiers:

		<nom>.player.bin
		<nom>.runtime.ayt
		<nom>.two.asm
		<nom>.report.txt

#### 4. sp_bundle
Mode SP/JP, bundle compact:

		<nom>.bundle.bin
		<nom>.bundle.asm
		<nom>.report.txt

Après assemblage des .asm de test, on obtient en plus `<nom>.two-demo.bin` ou `<nom>.bundle-demo.bin`.


### Exemples de commandes

Les exemples ci-dessous sont à lancer depuis la racine du dépôt, avec les fichiers de `ayt-files/`. Les sorties vont dans `out/<cas>/<nom>/` (créer le dossier au préalable, par exemple `mkdir -p out/call_two/still_scrolling`).

#### CALL deux fichiers, exemple still_scrolling:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode call \
		--out-player out/call_two/still_scrolling/still_scrolling.player.bin \
		--out-ayt-runtime out/call_two/still_scrolling/still_scrolling.runtime.ayt \
		--out-asm-two out/call_two/still_scrolling/still_scrolling.two.asm \
		--report out/call_two/still_scrolling/still_scrolling.report.txt

Résultat: player de 359 octets en #0040, AYT runtime en #1000, programme de test (RUN) en #3B2B.

#### CALL bundle compact, exemple still_scrolling:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode call \
		--out-bundle out/call_bundle/still_scrolling/still_scrolling.bundle.bin \
		--out-asm-bundle out/call_bundle/still_scrolling/still_scrolling.bundle.asm \
		--report out/call_bundle/still_scrolling/still_scrolling.report.txt

Résultat: bundle de 11410 octets en #0040, AYT runtime en #01A7, RUN en #2CD2.

#### SP/JP deux fichiers, exemple still_scrolling:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode jp \
		--program-addr 0x3B2B \
		--return-addr 0x3B4F \
		--out-player out/sp_two/still_scrolling/still_scrolling.player.bin \
		--out-ayt-runtime out/sp_two/still_scrolling/still_scrolling.runtime.ayt \
		--out-asm-two out/sp_two/still_scrolling/still_scrolling.two.asm \
		--report out/sp_two/still_scrolling/still_scrolling.report.txt

Résultat: player de 355 octets en #0040, AYT runtime en #1000, RUN en #3B2B.

#### SP/JP bundle compact, exemple still_scrolling:

		aytpcbuild --input ayt-files/still_scrolling.ayt \
		--player-addr 0x0040 \
		--ayt-addr 0x1000 \
		--loops 2 \
		--mode jp \
		--program-addr 0x2CCE \
		--return-addr 0x2CF2 \
		--out-bundle out/sp_bundle/still_scrolling/still_scrolling.bundle.bin \
		--out-asm-bundle out/sp_bundle/still_scrolling/still_scrolling.bundle.asm \
		--report out/sp_bundle/still_scrolling/still_scrolling.report.txt

Résultat: bundle de 11406 octets en #0040, AYT runtime en #01A3, RUN en #2CCE.

#### Traiter tous les fichiers du dépôt d'un coup (CALL bundle)

		mkdir -p out/call_bundle
		for f in ayt-files/*.ayt; do
		  n=$(basename "$f" .ayt)
		  mkdir -p "out/call_bundle/$n"
		  aytpcbuild --input "$f" --player-addr 0x0040 --ayt-addr 0x1000 --loops 2 --mode call \
		    --out-bundle "out/call_bundle/$n/$n.bundle.bin" \
		    --out-asm-bundle "out/call_bundle/$n/$n.bundle.asm" \
		    --report "out/call_bundle/$n/$n.report.txt"
		done

#### Choisir program-addr et return-addr en mode SP/JP

En mode jp, `--return-addr` est obligatoire et vaut `program-addr + #24` pour les ASM de test générés. Pour obtenir les adresses sans calcul, générer d'abord le cas CALL correspondant et lire l'adresse `RUN` dans son rapport:

- deux fichiers: `--program-addr` = RUN du cas call_two;
- bundle: `--program-addr` = RUN du cas call_bundle - 4 (le player SP/JP est 4 octets plus court).

Les valeurs sont données dans les tableaux ci-dessous.


### Assembler les ASM de test

Les fichiers .asm générés utilisent une syntaxe de type Maxam (`org`, `equ`, `incbin`, `run`). **N'importe quel assembleur Z80 moderne** convient (par exemple rasm, sjasmplus, fantams, pasmo, etc.); Maxam n'est pas fourni dans ce dépôt. Selon l'assembleur, la directive `run $` ou l'écriture de la sortie (binaire ou fichier SNA) peut demander une petite adaptation: se référer à sa documentation.

Le source ASM doit avoir accès aux fichiers appelés par INCBIN. Les ASM générés par aytpcbuild utilisent uniquement le nom du fichier, par exemple:

		incbin "still_scrolling.bundle.bin"

Il faut donc lancer l'assembleur **depuis le dossier** qui contient le .asm et le .bin référencé (ou les placer ensemble).

Exemple, cas call_two:

		cd out/call_two/still_scrolling
		<assembleur> still_scrolling.two.asm still_scrolling.two-demo.bin

Exemple, cas call_bundle:

		cd out/call_bundle/still_scrolling
		<assembleur> still_scrolling.bundle.asm still_scrolling.bundle-demo.bin

(`<assembleur>` et sa ligne de commande dépendent de l'outil choisi.) Avec fantams, par exemple:

		fantams still_scrolling.bundle.asm -o still_scrolling.bundle-demo.bin

Le résultat se charge à l'adresse du player (#0040) et se lance à l'adresse `RUN` indiquée dans le rapport (#2CD2 pour ce bundle).

Si l'on préfère un assembleur en ligne, tout assembleur Z80 web acceptant plusieurs fichiers convient; il faut fournir ensemble le .asm et le .bin.


## Récapitulatif des fichiers AYT du dépôt

Tous les exemples ci-dessous utilisent (valeurs générées par aytpcbuild à partir de `ayt-files/`):

	player_addr = #0040
	loops       = 2

### CALL deux fichiers:

| Nom AYT | AYT Runtime | Taille Player | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
| david_whittaker_demo.ayt | #1000 | 326 bytes | #3BB0 |
| med-Merci_Monsieur.ayt | #1000 | 326 bytes | #4574 |
| med-My_Ai_Is_Dumb.ayt | #1000 | 359 bytes | #3D37 |
| still_scrolling.ayt | #1000 | 359 bytes | #3B2B |


### CALL bundle compact:

| Nom AYT | AYT Runtime | Taille bundle | demo RUN | 
| :-----------: | :--------------: | :-------------: | :---------: |
| david_whittaker_demo.ayt | #0186 | 11510 bytes | #2D36 |
| med-Merci_Monsieur.ayt | #0186 | 14010 bytes | #36FA |
| med-My_Ai_Is_Dumb.ayt | #01A7 | 11934 bytes | #2EDE |
| still_scrolling.ayt | #01A7 | 11410 bytes | #2CD2 |

### SP/JP deux fichiers:
| Nom AYT | AYT Runtime | Taille Player | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
| david_whittaker_demo.ayt | #1000 | 322 bytes | #3BB0 | #3BD4 |
| med-Merci_Monsieur.ayt | #1000 | 322 bytes | #4574 | #4598 |
| med-My_Ai_Is_Dumb.ayt | #1000 | 355 bytes | #3D37 | #3D5B |
| still_scrolling.ayt | #1000 | 355 bytes | #3B2B | #3B4F |

### SP/JP bundle compact:

| Nom AYT | AYT Runtime | Taille bundle | demo RUN | return | 
| :-----------: | :--------------: | :-------------: | :---------: | :-------: |
| david_whittaker_demo.ayt | #0182 | 11506 bytes | #2D32 | #2D56 |
| med-Merci_Monsieur.ayt | #0182 | 14006 bytes | #36F6 | #371A |
| med-My_Ai_Is_Dumb.ayt | #01A3 | 11930 bytes | #2EDA | #2EFE |
| still_scrolling.ayt | #01A3 | 11406 bytes | #2CCE | #2CF2 |

(En mode SP/JP, `demo RUN` est la valeur à passer en `--program-addr`, et `return` en `--return-addr`.)


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
