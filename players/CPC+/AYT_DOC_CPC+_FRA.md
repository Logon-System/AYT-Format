![Image Presentation CPC+](../../images/AYTPRES1.jpg)
## Appel de Ayt_Builder sur CPC 464+/6128+/GX4000 

Sur les machines **"CPC+"** de Amstrad, il y a un paramètre complémentaire lié aux spécificités techniques de ces machines, qui intègrent des **DMA** permettant de mettre à jour les registres **AY** *presque* automatiquement.

		ld ix,AYT_File		; AYT_File est l'adresse où se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse où le player sera construit
            ld bc,#0101         ; Etat de la page asic en entree (B) et en sortie (C) (0=off/1=on)
		ld a,2			; A indique combien de fois la musique sera jouée
		call Ayt_Builder

Il existe quelques contraintes inhérentes à l'espace occupé par la page d'entrées sorties spéciale du "Plus" qui sera nommé **page Asic** dans ce document.

La **page Asic**, qui permet l'accès aux paramètrages des **DMA**, est située entre **0x4000 et 0x7FFF**. 
Lorsqu'elle est connectée, elle occulte la ram située à cet emplacement. 
- L'adresse de *AYT_Player* ne doit jamais être définie entre **0x4000 et 0x7FFF** car ce dernier a besoin d'écrire dans la **page Asic** pour fonctionner.
- Le fichier **AYT** peut être placé n'importe où en ram. Le *player* gérera lui même la connexion de la **page Asic** selon le contexte.

### Paramètre BC : Etat de la page Asic
Le registre BC en entrée de *Ayt_Builder* permet d'indiquer quel sera l'état de la page Asic au moment de l'appel du *player* et l'état de la page Asic que l'utilisateur souhaite avoir en sortie du *player*.

Il existe **plusieurs configurations** de travail lorsqu'on travaille avec la page Asic.

On peut, par exemple:
- appeler le player alors que la page ASIC est déjà connectée.
- souhaiter que le player déconnecte la page ASIC en sortie.

Ce qu'il faut retenir, c'est que le player aura besoin :
- de déconnecter la page Asic si cette dernière est connectée en entrée ET que le fichier **AYT** déborde dans la zone occupée par cette page.
- de connecter la page Asic pour mettre à jour les registres de contrôle du DMA.
- de connecter ou déconnecter la page Asic selon ce qui est souhaité.

Chacune de ces actions peut prendre quelques micro-secondes et le contexte est donc important pour que le gain de CPU soit optimal.
Ainsi, par exemple, on peut avoir les 2 cas opposés suivants :
- La page Asic est toujours connectée et le fichier **AYT** n'est pas situé entre **0x4000 et 0x7FFF**. Pour indiquer cette situation à *Ayt_Builder*, B et C doivent tous les deux contenir la valeur #01. Le fichier n'étant pas situé dans la page Asic, le *player* ne la déconnectera. Il n'aura pas non plus besoin de la reconnecter pour configurer le DMA. Enfin, il n'aura pas besoin de connecter l'Asic (puisqu'il l'est déjà). Dans cette configuration, le player est optimal. 
- La page Asic est connectée en entrée et le fichier **AYT** est situé entre **0x4000 et 0x7FFF**, et la page doit être déconnectée en sortie (à noter qu'il "coûte" moins cher de 1 nop+1 octet de le faire via le *Player* que soi-même à la sortie du *Player*). Pour indiquer cette situation, B doit contenir la valeur #01 et C doit contenir la valeur #00. Le fichier *AYT* se trouvant dans la page Asic, le player devra alors déconnecter l'Asic pour pouvoir accéder aux données. Puis il devra connecter l'Asic pour configurer le DMA. Enfin il devra déconnecter l'Asic avant de rendre la main. Dans cette configuration, le player a consommé 20 nops et 14 octets de Ram en plus que dans la configuration précédente.

Le tableau suivant décrit toutes les configurations possibles.
Il est donc judicieux d'éviter autant que possible de placer le fichier dans la zone Asic, surtout si la page Asic est ouverte en entrée.

| Fichier AYT entre 0x4000 et 0x7FFF | Etat Asic Entrée (B) | Etat Asic Sortie (C) | CPU (Nops) | Octet en plus |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: |
| OUI           | ON (B=#01)               | ON (C=#01)      | +14           | +8            |
| OUI           | ON (B=#01)               | OFF (C=#00)     | +20           | +14            |
| OUI           | OFF (B=#00)               | ON (C=#01)     | +7           | +5            |
| OUI           | OFF (B=#00)              | OFF (C=#00)     | +13           | +9            |    
| NON           | ON (B=#01)               | ON (C=#01)         | +0 :-)       | +0            |
| NON           | ON (B=#01)               | OFF (C=#00)         | +7           | +5           |        
| NON           | OFF (B=#00)              | ON (C=#01)         | +7           | +5            |
| NON           | OFF (B=#00)           | OFF (C=#00)         | +13           | +9            |        


### Options de compilation
#### PlayerAccessByJP
Tout comme pour le *player* des anciens modèles CPC, il est possible de définir l'option **PlayerAccessByJP** qui impose de placer dans le registre HL l'adresse de retour du *player* lors de l'appel de *Ayt_Builder*.
Elle permet de gagner *11 µsecondes* si le programme appelant modifie lui même le pointeur de pile. 
Si ce n'est pas le cas de votre programme, cette option doit rester à 0 pour vous éviter d'avoir à sauvegarder vous même le pointeur de pile (En l'absence de restitution de la pile, empiler une adresse peut détruire les données **AYT**).

#### PlayerDMAUsed_SAR / PlayerDMAUsed_DCSRMask
Il existe deux autres options de compilation pour *Ayt_Builder* spécifiques aux capacités hardware des machines "Plus".
- **PlayerDMAUsed_SAR** permet de définir quel DMA parmi les 3 disponibles sera utilisé par le *player*.
  - 3 constantes sont pré-définies dans le source : **AYT_Asic_SAR0**, **AYT_Asic_SAR1**, **AYT_Asic_SAR2**
  - Par défaut le canal dma utilisé est le 0 : **PlayerDMAUsed_SA equ AYT_Asic_SAR0**
  - Pour en changer, si vous l'utilisez déjà par ailleurs, il suffit de modifier le numéro 0 de l'équivalence.
- **PlayerDMAUsed_DCSRMask** permet de définir le masque utilisé par le *player* pour cohabiter avec les autres canaux **DMA** sans les perturber.
  - 3 constantes sont pré-définies dans le source : **AYT_Asic_DCSRM0**, **AYT_Asic_DCSRM1**,**AYT_Asic_DCSRM2**
  - Ce paramètre doit définir le même canal DMA que celui défini avec **PlayerDMAUsed_SAR**
  - Par défaut le canal dma utilisé est le 0 : **PlayerDMAUsed_DCSRMask	equ AYT_Asic_DCSRM0**


#### Ayt_AsicPage_On / Ayt_AsicPage_Off
La connexion de la **page Asic** implique un registre "gate array" nommé **RMR2**.
Ce registre est multifonction et permet également de sélectionner une des 8 premières rom de la cartouche en rom "basse" à partir de #0000, #4000 ou #8000.
Les pignoufs chez Amstrad n'ont pas pas jugé utile de permettre la lecture de ce registre.
Sa mise à jour implique donc de connaitre son état antérieur, qui résulte d'une configuration propre à chaque programme.
Le *player* ne prévoit actuellement pas de gérer dynamiquement des configurations variantes de roms basses.
Si le *player* est appelé à partir d'une configuration unique, ce sont les équivalences **Ayt_AsicPage_On** et **Ayt_AsicPage_Off** qui permettent de la définir.

Par défaut, ce sont les codes les plus usuels qui sont utilisés, à savoir **#A0** pour la déconnexion et **#B8** pour la connexion.
Ces valeurs par défaut impliquent que le numéro de la rom basse est 0, et que son mapping soit en #0000 lorsque la **page Asic** est déconnectée.

Les utilisateurs ayant besoin que le player restitue une autre rom que 0 et/ou un mapping ailleurs qu'en #0000 peuvent modifier les équivalences pour ces valeurs en fonction de la table de description **RMR2** ci-après :

| Bits 7.6.5 | Bits 4.3 |  Bits 2.1.0 | Valeurs | Page Asic | Rom "basse" |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: |
| 1.0.1  | 0.0   | 0.0.0 à 1.1.1  | **#A0** à #A7  | OFF           | 0 à 7 en #0000 |
| 1.0.1  | 0.1   | 0.0.0 à 1.1.1  | #A8 à #AF  | OFF           | 0 à 7 en #4000 |
| 1.0.1  | 1.0   | 0.0.0 à 1.1.1  | #B0 à #B7  | OFF           | 0 à 7 en #8000 |
| 1.0.1  | 1.1   | 0.0.0 à 1.1.1  | **#B8** à #BF  | ON            | 0 à 7 en #0000 |

#### Player en ROM
Actuellement le **player** est conçu pour une utilisation en **RAM**. 
Il utilise le principe d'auto modification de son propre code objet pour gérer ses variables.
C'est le cas pour la mise à jour du *pointeur de séquences* et du *compteur de pattern*, mais aussi du registre Z80A **SP** lorsque le *player* est appelé avec la méthode "CALL" (voir option **PlayerAccessByJP**).

Si vous projetez d'utiliser le player généré dans une **ROM**, la mise à jour du *builder* n'est pas trop complexe à réaliser.

Une version ROM sera néanmions publiée si il existe une demande pour ça. 
Il faut cependant noter que cette version perdra 6 à 9 nops de CPU (selon l'option **PlayerAccessByJP equ 0**) et qu'il faudra réserver 3 à 5 octets en ram pour y loger les variables.


### Initialisation
Si le compresseur identifie des registres dit *inactifs*, ils sont exclus des données **AYT** mais nécessitent néanmoins une initialisation préalable.

Le player du "Plus" étant bien plus rapide que sur les CPCs de première génération, sa vitesse d'exécution peut devenir inférieure à celle que prendrait la routine d'initialisation (qui n'utilise pas de **DMA**).
Il est donc nécessaire d'appeler une fonction d'initialisation avant d'appeler le *player*.

La fonction *Ayt_Builder* construit une routine d'initialisation qui sera appelée **avant** l'utilisation du *player*.

A la sortie de la fonction *Ayt_Builder*:
- le registre **HL** contient l'adresse de la routine d'initialisation.
- le registre **DE** contient le pointeur sur le premier octet libre après le *player* (ou la routine d'initialisation).

**Remarque :**

Si le fichier ne contient aucun registre inactif, la routine d'initialisation devient inutile.
Dans ce cas, la fonction d'initialisation pointera sur un RET (et la routine d'initialisation occupera alors un octet au lieu de 34 dans le player).

Voici le traitement à mettre en place pour appeler une routine d'initialisation 

		ld ix,AYT_File		; AYT_File est l'adresse où se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse où le player sera construit
		ld a,2			; A indique combien de fois la musique sera jouée
		ld bc,#aabb	; Configuration de la page Asic en entrée et en sortie
		call Ayt_Builder
		ld (InitPlayer),hl	; Mise a jour de la routine d'initialisation
		...
		...
	InitPlayer equ $+1
		call 0			; Après la mise à jour, CALL sera sur la routine d'initialisation
 
### Périodicité d'appel de Ayt_Player

Pour jouer la musique, il faut appeler le *player* à la fréquence requise. 
La majorité des musiques nécessitent que le *player* soit appelé périodiquement 50 fois par seconde.
L'entête du fichier **AYT** indique cette période. 

Il est très **important de s'assurer qu'aucune interruption ne pourra avoir lieu pendant l'appel du player**. 

Si vous n'êtes pas familier avec le système des interruptions en Z80A, vous pouvez utiliser l'instruction **DI** avant l'appel du *player*.

		call AYT_Player	; Joue la musique

La périodicité d'appel du *player* est généralement basée sur la fréquence de l'écran, qui est de 50 Hertz. 
Cette information est disponible dans l'entête du fichier **AYT** (voir la description du format **AYT**)
.
Sur CPC+, la fréquence de 50 Hz est induite par le paramétrage du **CRTC 6845** "asic" qui signale son **signal VSYNC** via le **PPI 8255** "asic".
Il est possible de tester le signal Vsync avec le code suivant :

           ld b,#f5        ; port B du PPI 8255
    Wait_Vsync
            rra             ; Test du bit 0 
            jr nc,Wait_Vsync     ; Attente que le bit 0 passe à 1

Les interruptions sur les CPC offrent aussi la possibilité de se synchroniser de manière très précise.

### Performances

Les performances en temps d'exécution et de place mémoire du Player dépendent de plusieurs facteurs:
- Le nombre de registres actifs détectés par le compresseur (au maximum de 14).
- La *méthode d'appel* du *player* (**CALL** ou **JP**) sur toutes les plateformes.
- La configuration de *connexion/déconnexion* de la **page Asic** évoquée précédemment.

La *méthode d'appel* correspond à la façon dont le *player* est appelé en Z80A.
Cette méthode est une option de compilation du *builder*.
- Lorsque la *méthode d'appel* est de type **CALL**, le programme qui utilise le *player* doit l'appeler avec l'instruction Z80A **"CALL"**
- Lorsque la *méthode d'appel* est de type **JP**, le *player* doit être appelé avec l'instruction Z80A **"JP"**. Cette méthode nécessite toutefois que le programmeur fournisse au *builder* l'adresse de retour du *player*.
  - Le *player* ne sauvegarde alors pas le registre **SP**, ce qui permet de *"gagner"* **11 nops** (sur **CPC**) ou **37 Ts**.
  - C'est une option intéressante seulement si le programme qui appelle le player devait de toute manière modifier le registre **SP**.
  - Dans les autres cas, elle présente les problèmes suivants :
    - elle impose d'appeler le *builder* à chaque fois que l'adresse de retour change :
      - cela peut se produire fréquemment en *développement*, ce qui impose d'avoir le *builder* en ram.
      - cela peut se produire si il est nécessaire d'appeler le *player* de plusieurs endroits différents.
    - elle impose de restaurer le pointeur de pile car le moindre push ou call serait destructeur pour les données **AYT**.
  
Le tableau ci-dessous détaille les performances du *player* entre 10 et 14 registres actifs pour les deux *méthodes d'appel* possibles.

| Méthode Appel | Config Asic      | Nombre Registres | CPU en Nops | Taille Player en octets | Taille Builder en octets |
| :-----------: | :---------------:| :--------------: | :---------: | :-----------: | :------------: |
| JP            | OPTIMAL          | 10               | 178         | 188           | 596            |
| JP            | OPTIMAL          | 11               | 190         | 196           | 596            |        
| JP            | OPTIMAL          | 12               | 202         | 204           | 596            |        
| JP            | OPTIMAL          | 13               | 214         | 212           | 596            |        
| JP            | OPTIMAL          | 14               | 226         | 220           | 596            | 
| CALL          | NON OPTIMAL      | 10               | 196         | 198           | 612            |
| CALL          | NON OPTIMAL      | 11               | 208         | 206           | 612            |
| CALL          | NON OPTIMAL      | 12               | 220         | 214           | 612            |
| CALL          | NON OPTIMAL      | 13               | 232         | 222           | 612            |
| CALL          | NON OPTIMAL      | 14               | 244         | 230           | 612            |


 
