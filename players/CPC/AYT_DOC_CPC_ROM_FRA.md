![Image Presentation CPC](../../images/AYTPRES1OLD.jpg)

## ATTENTION !
### Ce document concerne l'utilisation de la version ROM de Ayt_builder

Pour un usage du *player* en **ROM**, une version spécifique de AYT_Builder a été créée. 
Son usage et ses paramètres sont sensiblement différents de la version qui fonctionne en RAM.

Si vous envisagez d'utiliser le player en **RAM**, je vous conseille d'utilier la version standard car elle occupe moins de place et est plus performante en **CPU**.

Sur CPC, les **ROMS** peuvent être activées aux adresses *0x0000-0x3FFF* et/ou *0xC000-0xFFFF*.
Le player généré utilisera pour ses variables un buffer passé en paramètre à *Ayt_Builder*

## Appel de Ayt_Builder sur CPC 464/664/6128

		ld ix,AYT_File		; AYT_File est l'adresse où se trouve le fichier AYT
    ld iy,AYT_Buffer ; AYT_Buffer est l'adresse d'un buffer en RAM
		ld de,AYT_Player	; AYT_Player est l'adresse en rom où le player sera exécuté
		ld a,2			; A indique combien de fois la musique sera jouée
		call Ayt_Builder

Le buffer en RAM devra avoir la taille suivante :
		AYT_Buffer	ds 7,0		; 7 octets à 0 lorsque **PlayAccessByJP==0**
		ou
		AYT_Buffer 	ds 5,0		; 5 octets à 0 lorsque **PlayAccessByJP==1**

L'adresse de ce buffer ne devra pas se trouver dans l'espace ou sera connecté la ROM contanant le *player*.

A la sortie de la fonction *Ayt_Builder* :
- le registre **DE** contient l'adresse du premier octet libre après le *player*.
- le registre **HL** contient le nombre de µsecondes (NOPs) consommé par l'appel du *player*.
- le registre **BC** contient l'adresse qu'il faudra **impérativement** appeler pour initialiser le *player*

### Option de compilation
#### PlayerAcessByJP
Si l'option **PlayerAcessByJP** vaut 1, il faut également définir l'adresse de retour du *player*.

		ld ix,AYT_File			; AYT_File est l'adresse où se trouve le fichier AYT
    ld iy,AYT_Buffer   ; AYT_Buffer est l'adresse d'un buffer en RAM
		ld de,AYT_Player		; AYT_Player est l'adresse où le player sera construit
		ld hl,AYT_Player_Ret		; AYT_Player_Ret est l'adresse à laquelle le player revient
		ld a,2				; A indique combien de fois la musique sera jouée
		call Ayt_Builder

On retrouve (par exemple) l'adresse de retour derrière l'appel du *player* 

			jp AYT_Player	; Joue la musique
	AYT_Player_Ret			; Adresse de retour du player

Sauf à vouloir briller en société en prétendant avoir économisé **11 µsecondes**, il est conseillé de laisser cette option à 0, ce qui vous évitera notamment de devoir sauvegarder et restituer le pointeur de pile.

### Initialisation
Si le compresseur identifie des registres dit *inactifs*, ils sont exclus des données **AYT** mais nécessitent néanmoins une initialisation préalable.

Le player en ROM utilise des variables situées en RAM, et la routine d'initialisation est chargée de mettre à jour ces variables.

L'appel de la routine d'initialisation est donc **obligatoire** une seule fois avant d'appeler le *player* périodiquement.

Voici le traitement à mettre en place pour appeler une routine d'initialisation (PlayerAccessByJP=0)

		ld ix,AYT_File		; AYT_File est l'adresse où se trouve le fichier AYT
    ld iy,AYT_Buffer   ; AYT_Buffer est l'adresse d'un buffer en RAM
		ld de,AYT_Player	; AYT_Player est l'adresse où le player sera construit
		ld a,2			; A indique combien de fois la musique sera jouée
		call Ayt_Builder
		ld (InitPlayer),bc	; Mise a jour de la routine d'initialisation
		...
		...
	InitPlayer equ $+1
		call 0			; Après la mise à jour, CALL sera sur la routine d'initialisation

Pour jouer la musique, il faut appeler le *player* à la fréquence requise. 
La majorité des musiques nécessitent que le *player* soit appelé périodiquement 50 fois par seconde.
L'entête du fichier **AYT** indique cette période. 

Il est très **important de s'assurer qu'aucune interruption ne pourra avoir lieu pendant l'appel du player**. Si vous n'êtes pas familier avec le système des interruptions en **Z80A**, vous pouvez utiliser l'instruction **DI** avant l'appel du *player*.

		call AYT_Player	; Joue la musique

 
Vous pouvez vous rapporter au tableau des performances en bas du document pour connaitre la Cpu et la taille du *player* généré.

***Remarques :***
- Le *player* met le circuit sonore en sourdine une fois le nombre de reboucle effectué.
- Si des registres nécessitent une initialisation, cette dernière est faite de manière transparente avec le 1er appel.
- La reboucle a lieu à l'endroit ou elle a été prévue par le fichier **YM**.
- Que ce soit lors de la reboucle ou lors de la mise en sourdine du *player*, ce dernier respectera à chaque appel une durée de Cpu constante.

### Périodicité d'appel de Ayt_Player
La périodicité d'appel du *player* est généralement basée sur la fréquence de l'écran, qui est de 50 Hertz. 
Cette information est disponible dans l'entête du fichier AYT (voir la description du format **AYT**).
Sur CPC 464/664/6128, la fréquence de 50 Hz est induite par le paramétrage du **CRTC 6845**, qui signale son signal de Vsync via le **PPI 8255**.
Il est possible de tester le signal de Vsync avec le code suivant :

           ld b,#f5        ; port B du PPI 8255
    Wait_Vsync
            rra             ; Test du bit 0 
            jr nc,Wait_Vsync     ; Attente que le bit 0 passe à 1

Les interruptions sur les CPC offrent aussi la possibilité de se synchroniser de manière très précise.

### Pré-Construction
Il est tout à fait possible de ***"pré construire"*** le *player*.

Vous pouvez utiliser *Ayt_Builder* pour créer préalablement le *player* et initialiser le fichier **AYT**.
Il suffit de sauvegarder le player créé et le fichier **AYT** mis à jour après l'appel de la fonction.

Vous pouvez ensuite intégrer le *player* et le fichier **AYT** en prenant soin de les placer aux adresses définies lors de l'appel à *Ayt_Builder*.

### Performances

Les performances en temps d'exécution et de place mémoire du Player dépendent de plusieurs facteurs:
- Le nombre de registres actifs détectés par le compresseur (au maximum de 14).
- La *méthode d'appel* du *player* (**CALL** ou **JP**) sur toutes les plateformes.

La *méthode d'appel* correspond à la façon dont le *player* est appelé en Z80A.
Cette méthode est une option de compilation du *builder*.
- Lorsque la *méthode d'appel* est de type **CALL**, le programme qui utilise le *player* doit l'appeler avec l'instruction Z80A **"CALL"**
- Lorsque la *méthode d'appel* est de type **JP**, le *player* doit être appelé avec l'instruction Z80A **"JP"**. Cette méthode nécessite toutefois que le programmeur fournisse au *builder* l'adresse de retour du *player*.
  - Le *player* ne sauvegarde alors pas le registre **SP**, ce qui permet de *"gagner"* **11 nops** (sur **CPC**).
  - C'est une option intéressante seulement si le programme qui appelle le player devait de toute manière modifier le registre **SP**.
  - Dans les autres cas, elle présente les problèmes suivants :
    - elle impose d'appeler le *builder* à chaque fois que l'adresse de retour change :
      - cela peut se produire fréquemment en *développement*, ce qui impose d'avoir le *builder* en ram.
      - cela peut se produire si il est nécessaire d'appeler le *player* de plusieurs endroits différents.
    - elle impose de restaurer le pointeur de pile car le moindre push ou call serait destructeur pour les données **AYT**.
  
Le tableau ci-dessous détaille les performances du *player* entre 10 et 14 registres actifs pour les deux *méthodes d'appel* possibles.
La taille du player comptabilise la routine d'initialisation des registres.
La CPU peut varier de 1 à 3 Nops selon les registres actifs.

| Méthode Appel | Nombre Registres | CPU en Nops | Taille Player en octets | Taille Builder en octets |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: |
| JP            | 10               | 366         | 299           | 386            |
| JP            | 11               | 397         | 315           | 386            |        
| JP            | 12               | 428         | 331           | 386            |        
| JP            | 13               | 458         | 347           | 386            |        
| JP            | 14               | 487         | 363           | 386            |        
| CALL          | 10               | 380         | 305           | 397            |
| CALL          | 11               | 411         | 321           | 397            |
| CALL          | 12               | 442         | 337           | 397            |
| CALL          | 13               | 472         | 353           | 397            |
| CALL          | 14               | 501         | 369           | 397            |




