
# Description du Format AYT

Un fichier *AYT* est compos� :
- D'un ent�te.
- Des donn�es constitutives des patterns.
- De s�quences de pointeurs relatifs � ces patterns.
- D'une s�quence de fin. 
- De donn�es d'initialisation du circuit sonore.

Toutes les valeurs 16 bits sont ordonn�es en m�moire selon le format *"little endian"* (pour rappel, le poids faible de la valeur 16 bits est cod� � Adresse et le poids fort de la valeur 16 bits est cod� � Adresse+1)

## Glossaire

* **registres actifs:** Dans un fichier YM, les registres ne sont pas tous utilis�s (par exemple si tous les canaux ne sont pas utilis�s.). Il est n�cessaire d'envoyer des donn�es vers ces registres � chaque frame car ils peuvent changer.  Le nombre de registres actifs varie de 1 a 14,  le registre 13 �tant obligatoirement actif dans cette version pour la gestion du player
* **registres inactifs**: Ce sont les registres qui ne changent pas au cours d'un morceau. Il n'est pas n�cessaire de les envoyer � chaque frame, et de les stocker dans le fichier AYT. Par contre il peut �tre n�cessaire de leur fixer une valeur pr�cise � l�initialisation.
* **nombre de frames** : C'est une valeur issue du fichier YM qui d�termine la dur�e totale de la musique. A chaque frame des donn�es sont envoy�es aux  registres du **AY8910/AY8912/YM2149**.
* **pattern**: bloc d'octets envoy� � un registre du circuit audio. Tous les patterns ont la m�me taille dans un m�me fichier AYT. Cette taille peut �tre de 1 � 256 octets.
* **s�quence**: Une s�quence est une liste de pointeurs vers des patterns. La suite des s�quences permet d'agencer les patterns, et in fine de reconstituer l'int�gralit� du morceau de musique en YM. Chaque s�quence contient autant de pointeurs que de registres 'actifs'.
* **taille des patterns** : Tous les patterns ont la m�me taille dans un m�me fichier AYT. Cette taille peut �tre de 1 a 256 octets, la valeur optimale �tant celle qui permet de produire le plus petit fichier AYT possible.
* **nombre de s�quences:** Ce nombre est obtenu en divisant le nombre de frame par la taille des patterns.  
* **s�quence de fin**: C'est une s�quence suppl�mentaire qui pointe vers des valeurs sp�ciales (en particulier pour R13) qui permet d'indiquer la fin du morceau, et qui permet au player de faire boucler le morceau au d�but ou sur la s�quence d�finie par le musicien
* **s�quence d'initialisation:** C'est une liste de couples (num�ro de registre, valeur) utilis� epour fixer une valeur aux registres avant de d�marrer le player. C'est essentiellement pour initialiser les registres inactifs. Cette liste se termine par une valeur dont le bit 7 est � 1. 
* **s�quence de d�marrage**: C'est un pointeur dans le fichier AYT vers la s�quence correspondant au d�but du morceau de musique. La plupart du temps, cela correspond � la premiere s�quence de la liste des s�quences, mais dans certains cas, on peut vouloir commencer plus loin.
* **s�quence de bouclage:** Quand la s�quence de fin a �t� atteinte, le player va sauter � la s�quence de bouclage pour continuer � jouer. La plupart du temps, cela correspond � un pointeur vers la premiere s�quence de la liste des s�quences,  mais on peut vouloir ne pas reboucler au d�but (passer l'intro par exemple, ou boucler sur une s�quence de silence). 
*

## D�finition de l'ent�te
    Ayt_Start
        struct AYT_Header
        { 
            uint8_t Ayt_Version      	; version aaaabbbb >> a.b
            uint16_t Ayt_ActiveRegs     ; active reg (bit 15:reg 0...bit 2:reg 13) 1=active 0:inactive
            uint8_t Ayt_PatternSize  	; Pattern size 1..255
            uint16_t Ayt_FirstSeqMarker ; Offset from Ayt_Start for Seq Ptr on patterns
            uint16_t Ayt_LoopSeqMarker 	; Offset from Ayt_Start for Loop Seq Ptr 
            uint16_t Ayt_ListInit	; Offset from Ayt_Start for Init of ay reg (*)
            uint16_t Ayt_NbPatternPtr  	; Nb of pattern ptr seq for music (NbSeq x NbReg)
            uint8_t  Ayt_PlatformFreq	; Platform (b0..4) & Frequency (b5..7) (see table) (**)
            uint8_t  Ayt_Reserved	; Reserved for future version (=0)
        }

### Ayt_Version, Num�ro de version
**1 octet** : Les 8 bits 7 � 0 sont codifi�s ainsi: aaaabbbb pour d�finir une version a.b

### Ayt_ActiveRegs, Registres actifs
**2 octets** : Les **bits 15 � 2** de cette valeur 16 bits repr�sentent respectivement les **registres 0 � 13** du AY/YM.
Si le bit vaut 1, cela signifie que le registre est actif, et 0 qu'il est inactif.
Les registres inactifs ne disposent pas de pointeurs dans une s�quence, mais sont toutefois initialis�s.

### Ayt_PatternSize, Taille d'un pattern_
**1 octet** : Cette valeur, de **0 � 255**, correspond � la taille d'un pattern (unique pour l'ensemble du fichier)

### Ayt_FirstSeqMarker, Premi�re s�quence
**2 octets** : Cette valeur contient l'offset de la premi�re s�quence par rapport au d�but du fichier **AYT**.
Autrement dit, si le fichier AYT est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la premi�re s�quence sera situ�e en #9000.
Une s�quence repr�sente un ensemble de pointeurs sur les patterns pour les registres actifs.

### Ayt_FirstSeqMarker, S�quence de rebouclage
**2 octets** : Cette valeur contient l'offset de la s�quence de reboucle par rapport au d�but du fichier AYT.
Autrement dit, si le fichier AYT est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la premi�re s�quence sera situ�e en #9000.
Une s�quence repr�sente un ensemble de pointeurs sur les patterns pour les registres actifs.
La s�quence de rebouclage peut �tre �gale � la premi�re s�quence si le rebouclage a lieu au d�but.

### Ayt_ListInit, Donn�es d'initialisation
**2 octets** : Cette valeur contient l'offset de la structure d'initialisation des registres par rapport au d�but du fichier **AYT**.
Autrement dit, si le fichier **AYT** est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la structure d'initialisation sera situ�e en #9000.
Actuellement, les compresseurs g�n�rent des donn�es d'initialisation lorsque le nombre de registres actifs est inf�rieur � 14.
Chaque registre inactif est alors initialis� car sa valeur a �t� d�termin�e constante.
Le format est cependant ouvert � d'autres initialisation si n�cessaires.
La structure de la liste d'initialisation est la suivante :

    struct Ay_Init [N register]
    {
        uint8_t Ayt_Reg		; No reg (0 to 13) 
        uint8_t Ayt_Val
    } 
    defb #FF                 ; End of Init List

**Ayt_Reg** est le num�ro de registre du circuit, et **Ayt_Val** sa valeur d'initialisation.

Si un fichier **AYT** ne contient pas de registre � initialiser, il contiendra seulement l'octet 0xFF.

### Ayt_NbPatternPtr, Nombre de pointeurs Patterns
**2 octets** : Cette valeur contient le nombre de pointeurs total sur les patterns pr�sents dans le fichier **AYT**, en incluant ceux de la s�quence de fin.
Une s�quence est compos�e de **N pointeurs** sur des patterns, **N** �tant le nombre de **registres actifs**.
Cette valeur contient la valeur **N x Nb total de s�quences du fichier**.
Le nombre de frame correspond � : **(Nb Total de S�quences-1) x Taille des Patterns.**

### Ayt_PlatformFreq, Plateforme et fr�quence
**1 octet** : les bits **0 � 4** codent un num�ro de plateforme sur 5 bits, et les bits **5 � 7** codent un type de fr�quence sur 3 bits.
Le format AYT �tant **multi-plateformes**, il est important de conserver cette information afin de pouvoir envisager des transferts **inter-plateformes**.

|Bits 4..0 | Plateforme | Frequence du circuit (Hz) |
| :-------:| :--------: | :------------------:|
| 00000 | AMSTRAD CPC | 1 000 000 |
| 00001 | ORIC | 1 000 000 |
| 00010 | ZXUNO | 1 750 000 |
| 00011 | PENTAGON | 1 750 000 |
| 00100 | TIMEXTS2068 | 1 764 000 |
| 00101 | ZX128 | 1 773 450 |
| 00110 | MSX | 1 789 772 |
| 00111 | ATARI ST | 2 000 000 |
| 01000 | VG5000 | 1 000 000 |
| 11111 | UNKNOWN |  |

|Bits 7..5 | Vitesse Appel Player (Hz) |
| :-------:| :--------: | 
| 000 | 50 |
| 001 | 25 |
| 010 | 60 |
| 011 | 30 |
| 100 | 100 |
| 101 | 200 |
| 111 | UNKNOWN |

### Ayt_Reserved, R�serv�
**1 octet** : Nan, j'dirais pas � quoi �a sert!

## D�finition des patterns
Situ� apr�s l'ent�te, on trouve des *patterns*.

Un *pattern* est un lot de N donn�es destin�es � �tre envoy�es en N passages au circuit sonore.
N correspond au nombre de donn�es d�finie par *Ayt_PatternSize*

L'adresse de d�but des *patterns* est �gale au d�but du fichier auquel on ajoute la taille du header.

Les patterns ne sont pas :
- sp�cifiques � un registre AY. Autrement dit un pattern est mutualis� et peut servir � des registres diff�rents.
- contigus en m�moire car deux lots de N donn�es peuvent constituer plus de 2 patterns. Autrement dit les donn�es des patterns sont mutualis�es.

## D�finition des s�quences
Situ�es apres les patterns, on trouve les s�quences de pointeurs sur *patterns*.
L'adresse de d�but des s�quences est calcul�e gr�ce � *Ayt_FirstSeqMarker*. 
Une s�quence est compos�e de N (N=nombre registres actifs) pointeurs sur les *patterns*.

## D�finition de la s�quence de fin
Situ�e apr�s la derni�re s�quence, cette s�quence permet d'indiquer la fin de la musique.
Cette s�quence particuli�re a 2 fonctions :
- elle indique la fin de la musique via la donn�e pr�vue pour le *registre 13*.
- elle permet de couper le son, ce qui est utile si le fichier YM a �t� mal "coup�", par exemple.

Le *registre 13* est un registre du circuit sonore particulier dans la mesure ou sa valeur ne doit pas �tre mise � jour si sa valeur n'a pas chang�.
Toute mise � jour de ce registre "r�initialise" la gestion de l'enveloppe hardware du circuit sonore (cela peut �tre "voulu", mais en g�n�ral, ce n'est pas le cas).

Cette caract�ristique implique de traiter ce registre sp�cifiquement en lecture.

Les fichiers **YM** pr�voient que la valeur **#FF** indique que le registre ne doit pas �tre remis � jour.
Les fichiers **AYT** utilisent uniquement les **bit 6 et 7** du *registre 13* pour d�terminer cette situation.
Tant que les bits **6 et 7 de R13 valent 1** (comme c'est le cas avec la valeur **#FF**) la valeur n'est pas renvoy�e au circuit.
Cependant, si le **bit 7 vaut 1 et le bit 6 vaut 0**, alors le player sait qu'il est sur la derni�re s�quence.

Les pointeurs de patterns de cette derni�re s�quence sont **particuliers** car :
- Les registres **R0 � R6, R8 � 12** pointent sur un **0**.
- Le registre **R7** pointent sur **#3F** (mute des canaux sonores).
- Le registre **R12** contient **0x10111111** pour signaler la fin de la musique.

**Remarque :** 
Selon la version du compresseur, ces 3 octets sont "recherch�s" dans les patterns, ou dans le pire des cas, cr��s apr�s la s�quence de fin.
(*Tronic t'es une feignasse* :-))
 
## D�finition de la structure d'initalisation 
Si aucune initialisation de registre ne doit avoir lieu car la musique contient 14 registres actifs, par exemple, alors on trouvera un seul octet avec la valeur de **#FF** � la suite de la s�quence de fin.
Si une initialisation doit �tre faite, on trouve des couples d'octets qui repr�sentent les registres � initialiser

Voir *Ayt_ListInit* pour le d�tail de la structure.

Si par exemple, les registre 3, 10 et 11 doivent �tre initialis�s avec les valeurs 0,1,2, on aura:

    defb 3,0,10,1,11,2,0x0FFh


