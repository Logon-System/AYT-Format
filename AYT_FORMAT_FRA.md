
# Description du Format AYT

Un fichier *AYT* est composé :
- D'un entête.
- Des données constitutives des patterns.
- De séquences de pointeurs relatifs à ces patterns.
- D'une séquence de fin. 
- De données d'initialisation du circuit sonore.

Toutes les valeurs 16 bits sont ordonnées en mémoire selon le format *"little endian"* (pour rappel, le poids faible de la valeur 16 bits est codé à Adresse et le poids fort de la valeur 16 bits est codé à Adresse+1)

## Glossaire

* **registres actifs:** Dans un fichier YM, les registres ne sont pas tous utilisés (par exemple si tous les canaux ne sont pas utilisés.). Il est nécessaire d'envoyer des données vers ces registres à chaque frame car ils peuvent changer.  Le nombre de registres actifs varie de 1 a 14,  le registre 13 étant obligatoirement actif dans cette version pour la gestion du player
* **registres inactifs**: Ce sont les registres qui ne changent pas au cours d'un morceau. Il n'est pas nécessaire de les envoyer à chaque frame, et de les stocker dans le fichier AYT. Par contre il peut être nécessaire de leur fixer une valeur précise à l’initialisation.
* **nombre de frames** : C'est une valeur issue du fichier YM qui détermine la durée totale de la musique. A chaque frame des données sont envoyées aux  registres du **AY8910/AY8912/YM2149**.
* **pattern**: bloc d'octets envoyé à un registre du circuit audio. Tous les patterns ont la même taille dans un même fichier AYT. Cette taille peut être de 1 à 256 octets.
* **séquence**: Une séquence est une liste de pointeurs vers des patterns. La suite des séquences permet d'agencer les patterns, et in fine de reconstituer l'intégralité du morceau de musique en YM. Chaque séquence contient autant de pointeurs que de registres 'actifs'.
* **taille des patterns** : Tous les patterns ont la même taille dans un même fichier AYT. Cette taille peut être de 1 a 256 octets, la valeur optimale étant celle qui permet de produire le plus petit fichier AYT possible.
* **nombre de séquences:** Ce nombre est obtenu en divisant le nombre de frame par la taille des patterns.  
* **séquence de fin**: C'est une séquence supplémentaire qui pointe vers des valeurs spéciales (en particulier pour R13) qui permet d'indiquer la fin du morceau, et qui permet au player de faire boucler le morceau au début ou sur la séquence définie par le musicien
* **séquence d'initialisation:** C'est une liste de couples (numéro de registre, valeur) utilisé epour fixer une valeur aux registres avant de démarrer le player. C'est essentiellement pour initialiser les registres inactifs. Cette liste se termine par une valeur dont le bit 7 est à 1. 
* **séquence de démarrage**: C'est un pointeur dans le fichier AYT vers la séquence correspondant au début du morceau de musique. La plupart du temps, cela correspond à la premiere séquence de la liste des séquences, mais dans certains cas, on peut vouloir commencer plus loin.
* **séquence de bouclage:** Quand la séquence de fin a été atteinte, le player va sauter à la séquence de bouclage pour continuer à jouer. La plupart du temps, cela correspond à un pointeur vers la premiere séquence de la liste des séquences,  mais on peut vouloir ne pas reboucler au début (passer l'intro par exemple, ou boucler sur une séquence de silence). 
*

## Définition de l'entête
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

### Ayt_Version, Numéro de version
**1 octet** : Les 8 bits 7 à 0 sont codifiés ainsi: aaaabbbb pour définir une version a.b

### Ayt_ActiveRegs, Registres actifs
**2 octets** : Les **bits 15 à 2** de cette valeur 16 bits représentent respectivement les **registres 0 à 13** du AY/YM.
Si le bit vaut 1, cela signifie que le registre est actif, et 0 qu'il est inactif.
Les registres inactifs ne disposent pas de pointeurs dans une séquence, mais sont toutefois initialisés.

### Ayt_PatternSize, Taille d'un pattern_
**1 octet** : Cette valeur, de **0 à 255**, correspond à la taille d'un pattern (unique pour l'ensemble du fichier)

### Ayt_FirstSeqMarker, Première séquence
**2 octets** : Cette valeur contient l'offset de la première séquence par rapport au début du fichier **AYT**.
Autrement dit, si le fichier AYT est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la première séquence sera située en #9000.
Une séquence représente un ensemble de pointeurs sur les patterns pour les registres actifs.

### Ayt_FirstSeqMarker, Séquence de rebouclage
**2 octets** : Cette valeur contient l'offset de la séquence de reboucle par rapport au début du fichier AYT.
Autrement dit, si le fichier AYT est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la première séquence sera située en #9000.
Une séquence représente un ensemble de pointeurs sur les patterns pour les registres actifs.
La séquence de rebouclage peut être égale à la première séquence si le rebouclage a lieu au début.

### Ayt_ListInit, Données d'initialisation
**2 octets** : Cette valeur contient l'offset de la structure d'initialisation des registres par rapport au début du fichier **AYT**.
Autrement dit, si le fichier **AYT** est en #8000, par exemple, et que cette valeur vaut #1000, alors l'adresse de la structure d'initialisation sera située en #9000.
Actuellement, les compresseurs génèrent des données d'initialisation lorsque le nombre de registres actifs est inférieur à 14.
Chaque registre inactif est alors initialisé car sa valeur a été déterminée constante.
Le format est cependant ouvert à d'autres initialisation si nécessaires.
La structure de la liste d'initialisation est la suivante :

    struct Ay_Init [N register]
    {
        uint8_t Ayt_Reg		; No reg (0 to 13) 
        uint8_t Ayt_Val
    } 
    defb #FF                 ; End of Init List

**Ayt_Reg** est le numéro de registre du circuit, et **Ayt_Val** sa valeur d'initialisation.

Si un fichier **AYT** ne contient pas de registre à initialiser, il contiendra seulement l'octet 0xFF.

### Ayt_NbPatternPtr, Nombre de pointeurs Patterns
**2 octets** : Cette valeur contient le nombre de pointeurs total sur les patterns présents dans le fichier **AYT**, en incluant ceux de la séquence de fin.
Une séquence est composée de **N pointeurs** sur des patterns, **N** étant le nombre de **registres actifs**.
Cette valeur contient la valeur **N x Nb total de séquences du fichier**.
Le nombre de frame correspond à : **(Nb Total de Séquences-1) x Taille des Patterns.**

### Ayt_PlatformFreq, Plateforme et fréquence
**1 octet** : les bits **0 à 4** codent un numéro de plateforme sur 5 bits, et les bits **5 à 7** codent un type de fréquence sur 3 bits.
Le format AYT étant **multi-plateformes**, il est important de conserver cette information afin de pouvoir envisager des transferts **inter-plateformes**.

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

### Ayt_Reserved, Réservé
**1 octet** : Nan, j'dirais pas à quoi ça sert!

## Définition des patterns
Situé après l'entête, on trouve des *patterns*.

Un *pattern* est un lot de N données destinées à être envoyées en N passages au circuit sonore.
N correspond au nombre de données définie par *Ayt_PatternSize*

L'adresse de début des *patterns* est égale au début du fichier auquel on ajoute la taille du header.

Les patterns ne sont pas :
- spécifiques à un registre AY. Autrement dit un pattern est mutualisé et peut servir à des registres différents.
- contigus en mémoire car deux lots de N données peuvent constituer plus de 2 patterns. Autrement dit les données des patterns sont mutualisées.

## Définition des séquences
Situées apres les patterns, on trouve les séquences de pointeurs sur *patterns*.
L'adresse de début des séquences est calculée grâce à *Ayt_FirstSeqMarker*. 
Une séquence est composée de N (N=nombre registres actifs) pointeurs sur les *patterns*.

## Définition de la séquence de fin
Située après la dernière séquence, cette séquence permet d'indiquer la fin de la musique.
Cette séquence particulière a 2 fonctions :
- elle indique la fin de la musique via la donnée prévue pour le *registre 13*.
- elle permet de couper le son, ce qui est utile si le fichier YM a été mal "coupé", par exemple.

Le *registre 13* est un registre du circuit sonore particulier dans la mesure ou sa valeur ne doit pas être mise à jour si sa valeur n'a pas changé.
Toute mise à jour de ce registre "réinitialise" la gestion de l'enveloppe hardware du circuit sonore (cela peut être "voulu", mais en général, ce n'est pas le cas).

Cette caractéristique implique de traiter ce registre spécifiquement en lecture.

Les fichiers **YM** prévoient que la valeur **#FF** indique que le registre ne doit pas être remis à jour.
Les fichiers **AYT** utilisent uniquement les **bit 6 et 7** du *registre 13* pour déterminer cette situation.
Tant que les bits **6 et 7 de R13 valent 1** (comme c'est le cas avec la valeur **#FF**) la valeur n'est pas renvoyée au circuit.
Cependant, si le **bit 7 vaut 1 et le bit 6 vaut 0**, alors le player sait qu'il est sur la dernière séquence.

Les pointeurs de patterns de cette dernière séquence sont **particuliers** car :
- Les registres **R0 à R6, R8 à 12** pointent sur un **0**.
- Le registre **R7** pointent sur **#3F** (mute des canaux sonores).
- Le registre **R12** contient **0x10111111** pour signaler la fin de la musique.

**Remarque :** 
Selon la version du compresseur, ces 3 octets sont "recherchés" dans les patterns, ou dans le pire des cas, créés après la séquence de fin.
(*Tronic t'es une feignasse* 😆)
 
## Définition de la structure d'initalisation 
Si aucune initialisation de registre ne doit avoir lieu car la musique contient 14 registres actifs, par exemple, alors on trouvera un seul octet avec la valeur de **#FF** à la suite de la séquence de fin.
Si une initialisation doit être faite, on trouve des couples d'octets qui représentent les registres à initialiser

Voir *Ayt_ListInit* pour le détail de la structure.

Si par exemple, les registre 3, 10 et 11 doivent être initialisés avec les valeurs 0,1,2, on aura:

    defb 3,0,10,1,11,2,0x0FFh



