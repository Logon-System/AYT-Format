## Appel de Ayt_Builder sur ZX SPECTRUM 128/+2/+3

		ld ix,AYT_File		; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse o� le player sera construit
		ld bc,AYT_Init			; AYT_Init est l'adresse ou est cr��e la fonction d'initialisation si <>de 0
            ld a,1	; Nb of loop for the music
		call AYT_Builder	; Build the player @DE for file pointed by @IX for "A" loop

Pour jouer la musique, il faut appeler le *player* � la fr�quence requise. 
La majorit� des musiques n�cessitent que le *player* soit appel� p�riodiquement 50 fois par seconde.
L'ent�te du fichier *AYT* indique cette p�riode. 

Il est tr�s **important de s'assurer qu'aucune interruption ne pourra avoir lieu pendant l'appel du player**. Si vous n'�tes pas familier avec le syst�me des interruptions en **Z80A**, vous pouvez utiliser l'instruction **DI** avant l'appel du *player*.

		call AYT_Player	; Joue la musique

### Option de compilation
#### PlayerAcessByJP

Si l'option PlayerAcessByJP vaut 1, il faut �galement d�finir l'adresse de retour du *player*.

		ld ix,AYT_File			; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player		; AYT_Player est l'adresse o� le player sera construit
		ld bc,AYT_Init			; AYT_Init est l'adresse ou est cr��e la fonction d'initialisation si <>de 0
		ld hl,AYT_Player_Ret		; AYT_Player_Ret est l'adresse � laquelle le player revient
		ld a,2				; A indique combien de fois la musique sera jou�e
		call Ayt_Builder

On retrouve l'adresse de retour derri�re l'appel du *player* 

			jp AYT_Player	; Joue la musique
	AYT_Player_Ret			; Adresse de retour du player

## Initialisation
Si le compresseur identifie des registres *inactifs*, ils sont exclus des donn�es **AYT** mais n�cessitent n�anmoins une initialisation pr�alable.

Il est n�cessaire d'appeler une fonction d'initialisation avant d'appeler le player.

La fonction *Ayt_Builder* construit une routine d'initialisation qui sera appel�e **avant** l'utilisation du *player*.
Deux alternatives se pr�sentent en entr�e de la fonction:
- Si le registre **BC vaut 0**, alors la fonction *AYT_Builder* va r�server **16 octets** apr�s le *player* pour cr�er cette routine.
- Si le registre **BC est diff�rent de 0**, il doit alors contenir l'adresse d'une zone r�serv�e de **16 octets** (qui peut se situer n'importe ou en ram). Cette zone peut �tre r�cup�r�e par le programme apr�s initialisation.

A la sortie de la fonction *Ayt_Builder*:
- le registre **HL** contient l'adresse de la routine d'initialisation (il vaut donc BC (en entr�e) si ce dernier �tait non nul).
- le registre **DE** contient le pointeur sur le premier octet libre apr�s le *player* (ou la routine d'initialisation).

**Remarque :**

Si le fichier ne contient aucun registre inactif, la routine d'initialisation devient inutile.
Dans ce cas, la fonction d'initialisation pointera sur un RET (et la routine d'initialisation occupera alors un octet au lieu de 34).

Voici le traitement � mettre en place pour appeler une routine d'initialisation 

		ld ix,AYT_File		; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse o� le player sera construit
		ld bc,AYT_Init			; AYT_Init est l'adresse ou est cr��e la fonction d'initialisation si <>de 0
            ld a,2	; Nb of loop for the music
		call AYT_Builder	; Build the player @DE for file pointed by @IX for "A" loop

		ld (InitPlayer),hl	; Mise a jour de la routine d'initialisation
		...
		...
	InitPlayer equ $+1
		call 0			; Apr�s la mise � jour, CALL sera sur la routine d'initialisation

## P�riodicit� d'appel de Ayt_Player
La p�riodicit� d'appel du *player* est g�n�ralement bas�e sur la fr�quence de l'�cran, qui est de 50 Hertz. 
Cette information est disponible dans l'ent�te du fichier **AYT** (voir la description du format **AYT**).

**Avertissement :** Le player a �t� test� sur l'�mulateur **ZX SPIN 0.666** qui dispose d'un assembleur int�gr� mais qui s'est av�r� assez bugg� sur ma machine.

A priori, il n'existe pas de signal Vsync testable sur ZX SPECTRUM 128.

Il existe cependant une interruption produite une fois par frame.
Sur le Z80A en mode IM 1, une interruption g�n�re un **RST 0x38h**, qui repr�sente un **CALL 0x0038h**.
Cependant la ROM, qui ne peut pas �tre d�connect�e de l'espace adressable, d�bute en 0x0000.
L'appel des interruptions a donc lieu dans la ROM, qui semble ne pas avoir pr�vu de passerelle en RAM.

Fort heureusement, le Z80A permet de d�placer ses interruptions via le mode IM2 (mode vectoris�).
Ce mode a rarement �t� exploit� dans les architectures des anciennes plateformes.
Pour l'utiliser il faut tricher un peu pour contourner l'absence de p�riph�riques vectoris�s, et construire une table de vecteurs compatible avec toutes les situations.

Sans rentrer dans des consid�rations trop techniques qui d�passeraient le cadre de ce document, voici la m�thode employ�e pour cadencer l'appel de *AYT_Player**

		;
		; Swith to Im2
            ; 
		ld bc,256		; 257 bytes to write
		ld a,&c9		; between C900 and CA00 included
		ld h,a			; with C9 byte = RET
		ld l,c
		ld (hl),a
		ld d,h
		ld e,b
		ldir
		ld i,a			; Msb of vector tab
		im 2           ; switch to interrupt mode 2

Ainsi c'est l'instruction Z80A **HALT**, plac�e derri�re le EI qui attend que l'interruption Vsync se produise.
La table d'interruption cr�� entre **0xC900h et 0xCA00h contient** des **0xC9h**, qui correspondent � l'instruction **RET** en Z80A.
Une fois que l'interruption s'est produite, elle ne peut plus se reproduire avant le prochain **EI**.

		di
		call AYT_Player
		ei			; Int authorized
		halt		; wait for irq on vsync via Im2

## Pr�-Construction
Il est tout � fait possible de ***"pr� construire"*** le *player*.

Vous pouvez utiliser *Ayt_Builder* pour cr�er pr�alablement le *player* et initialiser le fichier **AYT**.
Il suffit de sauvegarder le player cr�� et le fichier **AYT** mis � jour apr�s l'appel de la fonction.

Vous pouvez ensuite int�grer le *player* et le fichier **AYT** en prenant soin de les replacer aux adresses d�finies lors de l'appel � *Ayt_Builder*.

## Performances

Les performances en temps d'ex�cution et de place m�moire du Player d�pendent de plusieurs facteurs:
- Le nombre de registres actifs d�tect�s par le compresseur (au maximum de 14).
- La *m�thode d'appel* du *player* (**CALL** ou **JP**) sur toutes les plateformes.
- La configuration de *connexion/d�connexion* de la **page Asic** �voqu�e pr�c�demment.

La *m�thode d'appel* correspond � la fa�on dont le *player* est appel� en Z80A.
Cette m�thode est une option de compilation du *builder*.
- Lorsque la *m�thode d'appel* est de type **CALL**, le programme qui utilise le *player* doit l'appeler avec l'instruction Z80A **"CALL"**
- Lorsque la *m�thode d'appel* est de type **JP**, le *player* doit �tre appel� avec l'instruction Z80A **"JP"**. Cette m�thode n�cessite toutefois que le programmeur fournisse au *builder* l'adresse de retour du *player*.
  - Le *player* ne sauvegarde alors pas le registre **SP**, ce qui permet de *"gagner"* **11 nops** (sur **CPC**) ou **37 Ts**.
  - C'est une option int�ressante seulement si le programme qui appelle le player devait de toute mani�re modifier le registre **SP**.
  - Dans les autres cas, elle pr�sente les probl�mes suivants :
    - elle impose d'appeler le *builder* � chaque fois que l'adresse de retour change :
      - cela peut se produire fr�quemment en *d�veloppement*, ce qui impose d'avoir le *builder* en ram.
      - cela peut se produire si il est n�cessaire d'appeler le *player* de plusieurs endroits diff�rents.
    - elle impose de restaurer le pointeur de pile car le moindre push ou call serait destructeur pour les donn�es **AYT**.
  
Le tableau ci-dessous d�taille les performances du *player* entre 10 et 14 registres actifs pour les deux *m�thodes d'appel* possibles.


| M�thode Appel | Nombre Registres | CPU en Tstates | Taille Player | Taille Builder |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: |
| JP            | 10               | 685         | 148           | 338            |
| JP            | 11               | 738         | 155           | 338            |    
| JP            | 12               | 791         | 162           | 338            |        
| JP            | 13               | 844         | 169           | 338            |        
| JP            | 14               | 897         | 176           | 338            |  
| CALL          | 10               | 722         | 153           | 353            |
| CALL          | 11               | 775         | 160           | 353            |
| CALL          | 12               | 828         | 167           | 353            |
| CALL          | 13               | 881         | 174           | 353            |
| CALL          | 14               | 934         | 181           | 353            |

