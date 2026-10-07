AYT PC BUILD
============

Outil PC qui remplace le builder Z80 runtime AYT.

Il lit un fichier .ayt source, genere le player Z80 specialise, relocalise
les pointeurs de l'AYT runtime, puis produit les fichiers utilisables sur
Amstrad CPC.

Documentation complete (options, 4 cas de generation, tableaux d'adresses):

  ../GUIDE_FRA.MD   (francais)
  ../GUIDE_ENG.MD   (english)


Contenu
-------

  aytpcbuild.c
    Source C99 portable, sans dependance.

  build/aytpcbuild-win.exe
    Executable Windows deja compile.

  build/aytpcbuild-menu.bat
    Menu interactif Windows (attend aytpcbuild.exe a cote de lui et les
    .ayt dans le sous-dossier ayt\ ; voir le guide).

Les fichiers .ayt d'exemple sont a la racine du depot, dans ayt-files/ :

  david_whittaker_demo.ayt
  med-Merci_Monsieur.ayt
  med-My_Ai_Is_Dumb.ayt
  still_scrolling.ayt

Les resultats ne sont pas fournis : ils sont generes par les commandes
ci-dessous.


Compilation
-----------

Depuis la racine du depot.

Linux ou macOS:

  cc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild \
    players/AYT-Builder-C/CPC/bin/aytpcbuild.c

Windows avec GCC:

  gcc -std=c99 -O2 -Wall -Wextra -pedantic -o aytpcbuild.exe \
    players\AYT-Builder-C\CPC\bin\aytpcbuild.c


Exemple rapide
--------------

Depuis la racine du depot (mkdir out au prealable):

  aytpcbuild --input ayt-files/still_scrolling.ayt \
    --player-addr 0x0040 --ayt-addr 0x1000 --loops 2 --mode call \
    --out-bundle out/still_scrolling.bundle.bin \
    --out-asm-bundle out/still_scrolling.bundle.asm \
    --report out/still_scrolling.report.txt

Sortie attendue:

  AYT PC build ok: player=359 bytes, ayt=11051 bytes at #01A7, cpu=463 nops, init block present

Fichiers produits dans out/ :

  still_scrolling.bundle.bin   11410 octets (player + AYT runtime), charge en #0040
  still_scrolling.bundle.asm   programme de test, RUN #2CD2
  still_scrolling.report.txt   rapport


Syntaxe generale
----------------

  aytpcbuild --input music.ayt --player-addr ADDR --ayt-addr ADDR \
    --loops N --mode jp|call [--return-addr ADDR] \
    [--out-player player.bin] [--out-ayt-runtime music.runtime.ayt] \
    [--out-bundle runtime.bin] [--bundle-base ADDR] \
    [--out-asm-two test-two.asm] [--out-asm-bundle test-bundle.asm] \
    [--program-addr ADDR] [--report report.txt]

--mode call : le player est appele par "call AYT_Player" (il sauvegarde
              et restaure SP lui-meme).
--mode jp   : mode rapide SP/JP, appele par "jp AYT_Player" ; retour par JP
              vers --return-addr (obligatoire).
              Pour les ASM de test generes : return_addr = program_addr + #24
--loops N   : nombre de lectures de la musique (0 a 255).
--out-bundle: bundle compact player + AYT runtime, sans trou ; l'AYT runtime
              suit immediatement le player (adresse indiquee dans le rapport).


Les 4 cas, exemples avec still_scrolling.ayt (player en #0040, loops 2)
------------------------------------------------------------------------

1. call_two    : --mode call --out-player ... --out-ayt-runtime ... --out-asm-two ...
                 player 359 octets, AYT runtime #1000, RUN #3B2B

2. call_bundle : --mode call --out-bundle ... --out-asm-bundle ...
                 bundle 11410 octets, AYT runtime #01A7, RUN #2CD2

3. sp_two      : --mode jp --program-addr 0x3B2B --return-addr 0x3B4F
                 --out-player ... --out-ayt-runtime ... --out-asm-two ...
                 player 355 octets, AYT runtime #1000, RUN #3B2B

4. sp_bundle   : --mode jp --program-addr 0x2CCE --return-addr 0x2CF2
                 --out-bundle ... --out-asm-bundle ...
                 bundle 11406 octets, AYT runtime #01A3, RUN #2CCE

Les tableaux complets pour les 4 fichiers .ayt du depot sont dans le guide.


Assembler les ASM de test
-------------------------

Les .asm generes utilisent une syntaxe de type Maxam (org, equ, incbin, run).
N'importe quel assembleur Z80 moderne convient (rasm, sjasmplus, fantams,
pasmo...). Maxam n'est pas fourni dans ce depot. Selon l'assembleur, la
directive "run $" ou le format de sortie peut demander une petite adaptation.

Les INCBIN ne contiennent que le nom du fichier : lancer l'assembleur depuis
le dossier qui contient le .asm et le .bin.

  cd out
  <assembleur> still_scrolling.bundle.asm still_scrolling.bundle-demo.bin

Avec fantams, par exemple:

  fantams still_scrolling.bundle.asm -o still_scrolling.bundle-demo.bin

Le resultat se charge en #0040 et se lance en #2CD2.


Notes
-----

1. --out-bundle genere toujours un bundle compact (plus de mode avec trous).
2. En mode SP/JP, --return-addr est obligatoire.
3. Les .runtime.ayt ne sont pas des AYT source : ils contiennent les
   pointeurs relocalises pour l'adresse CPC finale.
4. Le player ne depend plus du builder ASM Z80 a l'execution.
