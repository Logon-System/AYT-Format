# AYT PC BUILD - DISTRIBUTION
===========================

Etat de cette distribution
--------------------------

Ce dossier contient l'outil PC qui remplace le builder Z80 runtime AYT.
Il lit un fichier .ayt source, genere directement le player Z80 specialise,
relocalise les pointeurs de l'AYT runtime, puis produit les fichiers
utilisables sur Amstrad CPC.

Dans cette distribution, les exemples fournis ont ete generes avec:

  player_addr = #0040
  loops       = 2

Les resultats sont dans:

  dist\results\#40\


Contenu du dossier
------------------

dist\
  aytpcbuild.c
    Source C99 portable.

  aytpcbuild.exe
    Executable Windows deja compile.

  aytpcbuild-menu.bat
    Menu interactif Windows pour lancer aytpcbuild.exe sans taper toute la
    ligne de commande.

  README.txt
    Cette documentation.

  ayt\
    Fichiers AYT sources inclus:

      Arhey.ayt
      complications.ayt
      easy.ayt
      stillscrolling.ayt
      tatayoyo.ayt

  results\#40\
    Resultats prebuild fournis, avec player en #0040.


Compilation de l'outil
----------------------

Windows avec GCC:

  gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe aytpcbuild.c

Linux ou macOS:

  cc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild aytpcbuild.c

Le code C n'utilise pas de dependance externe.


Script interactif Windows
-------------------------

Depuis dist\, lancer:

  aytpcbuild-menu.bat

Le script demande:

  - le fichier AYT a traiter;
  - le cas de generation: call_two, call_bundle, sp_two ou sp_bundle;
  - l'adresse du player;
  - l'adresse du runtime AYT;
  - le nombre de loops;
  - les adresses program_addr et return_addr en mode SP/JP;
  - le dossier de sortie;
  - la generation ou non du mini ASM et du rapport TXT.

Le script lance uniquement aytpcbuild.exe.
Il ne lance pas maxam.exe et ne cree pas de *-demo.bin.


Exemple avec tatayoyo.ayt
-------------------------

Depuis dist\, lancer:

  aytpcbuild-menu.bat

Exemple de reponses pour generer un bundle CALL de tatayoyo.ayt:

  Choix AYT
    Choisir tatayoyo.ayt dans la liste.
  Cas de generation
    2. CALL - bundle compact

  Adresse CPC du player
    0x0666

  Adresse AYT initiale pour l'outil
    0x1000

  Nombre de lectures / loops
    2

  Dossier racine de sortie
    results\manual

  Dossier final
    results\manual\call_bundle\tatayoyo

  Generer le mini ASM de test
    O

  Generer le rapport TXT
    O

Le script lance alors aytpcbuild.exe and produit:

  results\manual\call_bundle\tatayoyo\tatayoyo.bundle.bin
  results\manual\call_bundle\tatayoyo\tatayoyo.bundle.asm
  results\manual\call_bundle\tatayoyo\tatayoyo.report.txt

Le fichier ASM genere reste portable dans son dossier.
Son INCBIN ne contient que le nom du fichier binaire, pas le chemin complet:

  incbin "tatayoyo.bundle.bin"

Il ne faut donc pas obtenir:

  incbin "results\manual\call_bundle\tatayoyo\tatayoyo.bundle.bin"


Syntaxe generale
----------------

  aytpcbuild --input music.ayt --player-addr 0x0100 \
    --ayt-addr 0x1000 --loops N --mode jp|call [--return-addr ADDR] \
    [--out-player player.bin] [--out-ayt-runtime music.runtime.ayt] \
    [--out-bundle runtime.bin] [--bundle-base ADDR] \
    [--out-asm-two test-two.asm] [--out-asm-bundle test-bundle.asm] \
    [--program-addr ADDR] \
    [--report report.txt]


Options
-------

--input fichier.ayt
  Fichier AYT source.
--player-addr ADDR
  Adresse CPC ou le player genere sera charge.
--ayt-addr ADDR
  Adresse CPC de l'AYT runtime dans le cas deux fichiers.
Si --out-bundle est demande, l'outil produit toujours un bundle compact.
Dans ce cas, l'AYT runtime est place juste apres le player et son adresse
est recalculee automatiquement.
La valeur --ayt-addr sert alors d'adresse initiale avant compactage.
--loops N
  Nombre de lectures de la musique, de 0 a 255.

  Les exemples fournis utilisent --loops 2.

--mode call
  Genere un player appele avec:

    call AYT_Player

  Dans ce mode, le player sauvegarde et restaure SP lui-meme.
--mode jp
  Genere le mode rapide SP/JP.

  Le programme doit appeler le player avec:

    jp AYT_Player

  Le player utilise SP comme pointeur rapide, puis revient par un JP vers
  l'adresse fournie par --return-addr.
--return-addr ADDR
  Obligatoire en mode jp.

  Pour les ASM de test generes par l'outil:

    return_addr = program_addr + #24

--program-addr ADDR
  Adresse du mini programme ASM de test genere par --out-asm-two ou
  --out-asm-bundle.
En mode call, cette option est facultative.
Si elle est absente, l'outil place le launcher a #0500 quand c'est possible,
sinon juste apres les donnees generees.
En mode jp, cette option est optionnelle mais doit rester coherente avec
  --return-addr.
--out-player fichier.bin
  Ecrit le player Z80 genere.

--out-ayt-runtime fichier.ayt
  Ecrit l'AYT runtime, avec les pointeurs de sequences relocalises.
--out-bundle fichier.bin
  Ecrit un bundle compact:

    player + ayt-runtime

  Il n'y a aucun trou de remplissage dans ce fichier.
--bundle-base ADDR
  Base du bundle.

  Dans cette version, cette adresse doit correspondre a --player-addr.
--out-asm-two fichier.asm
  Genere un mini source ASM de test pour le cas deux fichiers:

    player.bin + runtime.ayt

  Cette option demande aussi --out-player et --out-ayt-runtime.
--out-asm-bundle fichier.asm
  Genere un mini source ASM de test pour le cas bundle:

    bundle.bin

  Cette option demande aussi --out-bundle.
--report fichier.txt
  Ecrit un rapport texte lisible: tailles, adresses, registres actifs,
  bloc d'init, cout CPU, fichiers generes et RUN des ASM de test.
--help
  Affiche l'aide courte.


Les 4 cas generes
-----------------

Pour chaque AYT, quatre cas sont fournis dans dist\results\#40:

1. call_two
   Mode CALL, deux fichiers:

     <nom>.player.bin
     <nom>.runtime.ayt
     <nom>.two.asm
     <nom>.two-demo.bin
     <nom>.report.txt

2. call_bundle
   Mode CALL, bundle compact:

     <nom>.bundle.bin
     <nom>.bundle.asm
     <nom>.bundle-demo.bin
     <nom>.report.txt

3. sp_two
   Mode SP/JP, deux fichiers:

     <nom>.player.bin
     <nom>.runtime.ayt
     <nom>.two.asm
     <nom>.two-demo.bin
     <nom>.report.txt

4. sp_bundle
   Mode SP/JP, bundle compact:

     <nom>.bundle.bin
     <nom>.bundle.asm
     <nom>.bundle-demo.bin
     <nom>.report.txt


Arborescence des resultats #0040
--------------------------------

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


Exemples de commandes
---------------------

Les exemples ci-dessous sont a lancer depuis dist\.
CALL deux fichiers, exemple Arhey:

  .\aytpcbuild.exe --input ayt\Arhey.ayt ^
    --player-addr 0x0040 ^
    --ayt-addr 0x1000 ^
    --loops 2 ^
    --mode call ^
    --out-player "results\#40\call_two\Arhey\Arhey.player.bin" ^
    --out-ayt-runtime "results\#40\call_two\Arhey\Arhey.runtime.ayt" ^
    --out-asm-two "results\#40\call_two\Arhey\Arhey.two.asm" ^
    --report "results\#40\call_two\Arhey\Arhey.report.txt"

CALL bundle compact, exemple Arhey:

  .\aytpcbuild.exe --input ayt\Arhey.ayt ^
    --player-addr 0x0040 ^
    --ayt-addr 0x1000 ^
    --loops 2 ^
    --mode call ^
    --out-bundle "results\#40\call_bundle\Arhey\Arhey.bundle.bin" ^
    --out-asm-bundle "results\#40\call_bundle\Arhey\Arhey.bundle.asm" ^
    --report "results\#40\call_bundle\Arhey\Arhey.report.txt"

SP/JP deux fichiers, exemple Arhey:

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

SP/JP bundle compact, exemple Arhey:

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


Assembler les ASM de test
-------------------------

Les fichiers .asm generes sont compatibles avec maxam.
Deux solutions sont possibles:

  - utiliser maxam.exe en local, fourni dans le dossier parent du projet;
  - utiliser Maxam Web:

      https://amstrad.neocities.org/maxamweb/

Dans les deux cas, le source ASM doit avoir acces aux fichiers appeles par
INCBIN.
Les ASM generes par aytpcbuild utilisent uniquement le nom du fichier,
par exemple:

  incbin "tatayoyo.bundle.bin"

Il suffit donc de garder le .asm et le .bin reference dans le meme dossier, ou
de les fournir ensemble a Maxam Web si l'interface le demande.

Exemple depuis:

  dist\results\#40\call_two\Arhey\

Assembler le test:

  ..\..\..\..\..\maxam.exe Arhey.two.asm Arhey.two-demo.bin

Exemple depuis:

  dist\results\#40\call_bundle\Arhey\

Assembler le test:

  ..\..\..\..\..\maxam.exe Arhey.bundle.asm Arhey.bundle-demo.bin


Recapitulatif des AYT inclus
----------------------------

Tous les exemples ci-dessous utilisent:

  player_addr = #0040
  loops       = 2

CALL deux fichiers:

  Arhey.ayt
    AYT runtime : #1000
    player      : 359 bytes
    demo RUN    : #1F47

  complications.ayt
    AYT runtime : #1000
    player      : 359 bytes
    demo RUN    : #381C

  easy.ayt
    AYT runtime : #1000
    player      : 343 bytes
    demo RUN    : #1C79

  stillscrolling.ayt
    AYT runtime : #1000
    player      : 359 bytes
    demo RUN    : #3F9B

CALL bundle compact:

  Arhey.ayt
    AYT runtime : #01A7
    bundle      : 4270 bytes
    demo RUN    : #10EE

  complications.ayt
    AYT runtime : #01A7
    bundle      : 10627 bytes
    demo RUN    : #29C3

  easy.ayt
    AYT runtime : #0197
    bundle      : 3536 bytes
    demo RUN    : #0E10

  stillscrolling.ayt
    AYT runtime : #01A7
    bundle      : 12546 bytes
    demo RUN    : #3142

SP/JP deux fichiers:

  Arhey.ayt
    AYT runtime : #1000
    player      : 355 bytes
    demo RUN    : #1F47
    return      : #1F6B

  complications.ayt
    AYT runtime : #1000
    player      : 355 bytes
    demo RUN    : #381C
    return      : #3840

  easy.ayt
    AYT runtime : #1000
    player      : 339 bytes
    demo RUN    : #1C79
    return      : #1C9D

  stillscrolling.ayt
    AYT runtime : #1000
    player      : 355 bytes
    demo RUN    : #3F9B
    return      : #3FBF

SP/JP bundle compact:

  Arhey.ayt
    AYT runtime : #01A3
    bundle      : 4266 bytes
    demo RUN    : #10EA
    return      : #110E

  complications.ayt
    AYT runtime : #01A3
    bundle      : 10623 bytes
    demo RUN    : #29BF
    return      : #29E3

  easy.ayt
    AYT runtime : #0193
    bundle      : 3532 bytes
    demo RUN    : #0E0C
    return      : #0E30

  stillscrolling.ayt
    AYT runtime : #01A3
    bundle      : 12542 bytes
    demo RUN    : #313E
    return      : #3162


Notes importantes
-----------------

1. --out-bundle genere toujours un bundle compact.
   L'ancien mode avec trous n'existe plus.

2. En bundle, l'AYT runtime est place immediatement apres le player.
   Le rapport indique l'adresse finale.

3. En mode SP/JP, --return-addr est obligatoire.
   Pour les ASM de test generes par l'outil:

     return_addr = program_addr + #24

4. Les fichiers .runtime.ayt ne sont pas des AYT source originaux.
   Ils contiennent les pointeurs relocalises pour l'adresse CPC finale.

5. Le player prebuild ne depend plus du builder ASM Z80 a l'execution.
   Le builder est remplace par l'outil PC aytpcbuild.
