# Mise en oeuvre

## R�cup�rer un fichier YM
La premi�re chose � faire est de r�cup�rer un fichier au format **YM5** ou **YM6**. 

Il est possible d'en r�cup�rer des quantit�s extraordinaires sur diff�rents sites d�di�s.

Mais il est aussi possible de partir de ses propres compositions, pour les exporter dans ce format. 

Par exemple **Arkos Tracker**[^1] propose un export **YM**.
 
Certains �mulateurs, comme **Winape** (Cpc) permettent �galement de capturer un flux **YM**.

Des players comme **AY_Emul** [^2] g�rent la lecture de tr�s nombreux formats ZX Spectrum et permettent ce type d'export.

Id�alement, il faut que le fichier **YM** contienne uniquement les donn�es du morceau de musique, donc sans silence suppl�mentaire au d�but ou � la fin, et qu'il soit le plus court possible, et en ne jouant qu'une seule fois le morceau (on n'enregistre pas le rebouclage). 
Aussi il est pr�f�rable d'utiliser un tracker pour produire un **YM** car cela �vite de devoir nettoyer � posteriori.

Dans le cadre du **AYT Project**, des outils en ligne ont �t� d�velopp�. 

L'un d'eux est un s�quenceur qui permet de manipuler un fichier **YM** (s�lection de plage, suppression, fusion, export,...)

https://amstrad.neocities.org/ym-player-sequencer
![Image Presentation Web YM Sequencer ](./images/YMSequencerWeb.jpg)

[^1]: https://www.julien-nevo.com/arkostracker/

[^2]: http://bulba.untergrund.net/


## Conversion d'un fichier YM5/6 en AYT

Une fois la musique choisie, il faut passer par l'un des convertisseurs disponibles avec le projet.
 
Il existe diff�rentes versions selon les go�ts, selon vos pr�f�rences d'usage.

- une version en C++ qui peut �tre recompil�e sur toutes les plateformes capables de compiler du C++.
- une version en javascript contenue dans un seul fichier html qui tourne localement dans le navigateur. 
- Des binaires pr�-compil�s sont aussi mis � disposition. 
- Une version en ligne.
- Une application tout-en-un qui joue des YM ou AYT, g�re les transformations, g�n�re (pour CPC), un source de d�mo d'utilisation avec le builder selon votre assembleur pr�f�r�, et ouvre �galement l'�mulateur de votre choix (ou m�me g�n�re le fichier Snapshot (.SNA) directement utilisable. 

 
### Quelques exemples : 
#### Version en ligne ou html en local
Vous pouvez acc�der au convertisseur en ligne **YM5/6** to **AYT** ici:
https://amstrad.neocities.org/ym2ayt

Cet outil peut �tre t�l�charg� pour �tre utilis� en local.

![Image Presentation Web Ym2Ayt ](./images/YM2AYTWeb.jpg)

#### Application windows tout en un
Il s'agit d'un outil windows �crit en delphi, qui est assez adapt� pour aux environnement de cross-dev.

Une image valant mille mots

![Image Presentation WinAYT.exe ](./images/winAYT.png)


#### Ecouter un fichier en AYT

Afin de contr�ler le r�sultat de la conversion r�alis�e, plusieurs applications permettent d'�couter un fichier **AYT**.

![Image Player AYT Windows](./images/PlayerAytWindows.jpg)

Il existe �galement un portail qui le permet :

https://amstrad.neocities.org/ayt-web-player
![Image Presentation Web Ayt Player ](./images/AYTPlayerWeb.jpg)



## Utiliser un fichier AYT sur sa plateforme.

### AytPlayerBuilder
Sur les diff�rentes plateformes g�r�es, une fonction *builder* existe.

Elle permet de cr�er ex nihilo un *player* en m�moire � partir de quelques param�tres en entr�e de la fonction.

Une documentation existe au niveau du source pour chaque plateforme, afin de d�tailler plus pr�cis�ment ces param�tres.

Ces documentations int�grent �galement les performances en temps (NOPS ou TStates) et la taille occup�e par le *player* g�n�r� par le *builder*.

Les players sont tous en temps constant. A noter toutefois que pour certaines plateformes, le temps fixe est garanti lorsque le player est utilis� hors de la p�riode d'affichage car la CPU est alors partag�e pour permettre au circuit vid�o d'acc�er � la ram.

Pour chaque plateforme, il existe deux fichiers source en **Z80A**.

A noter que ces sources ont �t� cr�� avec l'assembleur int�gr� de l'�mulateur *CPC Winape*, qui respecte la syntaxe **MAXAM**.
Aucune *"fake instruction"* (ou autre directive h�r�tique de cette nature) n'est utilis� dans ces sources.
Ils sont facilement transposables sur d'autres assembleurs.

Ces sources existent actuellement pour 5 plateformes : **"CPC"**, **"CPC+"**, **"ZX"**, **"MSX"**, **"VG5000"**

Le premier source contient la fonction *Ayt_Builder*, qui sert � construire le *player*.

Le source est nomm� **AytPlayerBuilder-[Plateforme].asm** 

Ainsi pour la plateforme **MSX**, le fichier s'appelle **AytPlayerBuilder-MSX.asm**

Le second source est un programme d'exemple utilisant la fonction *Ayt_Builder*. 
Il est nomm� comme suit : **AytPlayerBuilder-[Plateforme]-demo.asm**

A cette fin il int�gre le source de la fonction *Ayt_Builder* via la directive **read**
 
Par exemple : **read "AytPlayerBuilder-CPC.asm"**

Le **fichier AYT** de votre choix est int�grable dans votre code via la directive **incbin** qui permet d'int�grer un fichier binaire nomm� � l'emplacement de la directive.

Par exemple : **incbin "mybestsong.ayt"**

### Option de compilation
Tous les sources qui contiennent la fonction *Ayt_Builder* disposent d'une **option de compilation** pour d�finir la **m�thode d'appel (CALL/JP)** souhait�e.
Cette option est d�finie par d�faut ainsi

**PlayerAcessByJP	equ 0**

- Lorsque cette option vaut **0**, cela signifie que *Ayt_Builder* cr�era un player qui devra �tre appel� avec l'instruction Z80A **CALL**.
- Lorsque cette option vaut **1**, cela signifie que *Ayt_Builder* cr�era un player qui devra �tre appel� avec l'instruction Z80A **JP** (dans ce cas, un param�tre compl�mentaire est n�cessaire lors de l'appel de la fonction)

Il est conseill� de laisser cette option par d�faut � 0 afin d'�viter de devoir sauvegarder et restituer le pointeur de pile si vous ne l'utilisez pas pour faire des galipettes dans la m�moire.

A noter que dans le source du *builder* de la plateforme CPC+, il existe d'autres options de compilation sp�cifiques aux m�thodes d'acc�s au circuit sonore.