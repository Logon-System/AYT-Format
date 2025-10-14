## Appel de Ayt_Builder sur CPC 464+/6128+/GX4000 

Sur les machines **"CPC+"** de Amstrad, il y a un param�tre compl�mentaire li� aux sp�cificit�s techniques de ces machines, qui int�grent des **DMA** permettant de mettre � jour les registres **AY** *presque* automatiquement.

		ld ix,AYT_File		; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse o� le player sera construit
            ld bc,#0101         ; Etat de la page asic en entree (B) et en sortie (C) (0=off/1=on)
		ld a,2			; A indique combien de fois la musique sera jou�e
		call Ayt_Builder

Il existe quelques contraintes inh�rentes � l'espace occup� par la page d'entr�es sorties sp�ciale du "Plus" qui sera nomm� **page Asic** dans ce document.

La **page Asic**, qui permet l'acc�s aux param�trages des **DMA**, est situ�e entre **0x4000 et 0x7FFF**. 
Lorsqu'elle est connect�e, elle occulte la ram situ�e � cet emplacement. 
- L'adresse de *AYT_Player* ne doit jamais �tre d�finie entre **0x4000 et 0x7FFF** car ce dernier a besoin d'�crire dans la **page Asic** pour fonctionner.
- Le fichier **AYT** peut �tre plac� n'importe o� en ram. Le *player* g�rera lui m�me la connexion de la **page Asic** selon le contexte.

### Param�tre BC : Etat de la page Asic
Le registre BC en entr�e de *Ayt_Builder* permet d'indiquer quel sera l'�tat de la page Asic au moment de l'appel du *player* et l'�tat de la page Asic que l'utilisateur souhaite avoir en sortie du *player*.

Il existe **plusieurs configurations** de travail lorsqu'on travaille avec la page Asic.

On peut, par exemple:
- appeler le player alors que la page ASIC est d�j� connect�e.
- souhaiter que le player d�connecte la page ASIC en sortie.

Ce qu'il faut retenir, c'est que le player aura besoin :
- de d�connecter la page Asic si cette derni�re est connect�e en entr�e ET que le fichier **AYT** d�borde dans la zone occup�e par cette page.
- de connecter la page Asic pour mettre � jour les registres de contr�le du DMA.
- de connecter ou d�connecter la page Asic selon ce qui est souhait�.

Chacune de ces actions peut prendre quelques micro-secondes et le contexte est donc important pour que le gain de CPU soit optimal.
Ainsi, par exemple, on peut avoir les 2 cas oppos�s suivants :
- La page Asic est toujours connect�e et le fichier **AYT** n'est pas situ� entre **0x4000 et 0x7FFF**. Pour indiquer cette situation � *Ayt_Builder*, B et C doivent tous les deux contenir la valeur #01. Le fichier n'�tant pas situ� dans la page Asic, le *player* ne la d�connectera. Il n'aura pas non plus besoin de la connecter pour configurer le DMA. Enfin, il n'aura pas besoin de connecter l'Asic (puisqu'il l'est d�j�). Dans cette configuration, le player �vite de consommer 20 Nops et 14 octets de Ram. 
- La page Asic est connect�e en entr�e et le fichier **AYT** est situ� entre **0x4000 et 0x7FFF**, et la page doit �tre d�connect�e en sortie (� noter qu'il "coute" moins cher de 1 nop/1 octet de le faire via le *Player* qu'� la sortie du *Player*). Pour indiquer cette situation, B doit contenir la valeur #01 et C doit contenir la valeur #00. Le fichier *AYT* se trouvant dans la page Asic, le player devra alors d�connecter l'Asic pour pouvoir acc�der aux donn�es. Puis il devra connecter l'Asic pour configurer le DMA. Enfin il devra d�connecter l'Asic avant de rendre la main. Dans cette configuration, le player a consomm� 20 nops et 14 octets de Ram en plus que dans la configuration pr�c�dente.

Le tableau suivant d�crit toutes les configurations possibles.
Il est donc judicieux d'�viter autant que possible de placer le fichier dans la zone Asic si la page Asic est ouverte en entr�e.

| Fichier AYT entre 0x4000 et 0x7FFF | Etat Asic Entr�e (B) | Etat Asic Sortie (C) | CPU (Nops) | Octet en plus |
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
Tout comme pour le *player* des anciens mod�les CPC, il est possible de d�finir l'option **PlayerAccessByJP** qui impose de placer dans le registre HL l'adresse de retour du *player* lors de l'appel de *Ayt_Builder*.
Elle permet de gagner *11 �secondes* si le programme appelant modifie lui m�me le pointeur de pile. 
Si ce n'est pas le cas de votre programme, cette option doit rester � 0 pour vous �viter d'avoir � sauvegarder vous m�me le pointeur de pile (En l'absence de restitution de la pile, empiler une adresse peut d�truire les donn�es **AYT**).

#### PlayerDMAUsed_SAR / PlayerDMAUsed_DCSRMask
Il existe deux autres options de compilation pour *Ayt_Builder* sp�cifiques aux capacit�s hardware des machines "Plus".
- **PlayerDMAUsed_SAR** permet de d�finir quel DMA parmi les 3 disponibles sera utilis� par le *player*.
  - 3 constantes sont pr�-d�finies dans le source : **AYT_Asic_SAR0**, **AYT_Asic_SAR1**, **AYT_Asic_SAR2**
  - Par d�faut le canal dma utilis� est le 0 : **PlayerDMAUsed_SA equ AYT_Asic_SAR0**
  - Pour en changer, si vous l'utilisez d�j� par ailleurs, il suffit de modifier le num�ro 0 de l'�quivalence.
- **PlayerDMAUsed_DCSRMask** permet de d�finir le masque utilis� par le *player* pour cohabiter avec les autres canaux **DMA** sans les perturber.
  - 3 constantes sont pr�-d�finies dans le source : **AYT_Asic_DCSRM0**, **AYT_Asic_DCSRM1**,**AYT_Asic_DCSRM2**
  - Ce param�tre doit d�finir le m�me canal DMA que celui d�fini avec **PlayerDMAUsed_SAR**
  - Par d�faut le canal dma utilis� est le 0 : **PlayerDMAUsed_DCSRMask	equ AYT_Asic_DCSRM0**

Pour jouer la musique, il faut appeler le *player* � la fr�quence requise. 
La majorit� des musiques n�cessitent que le *player* soit appel� p�riodiquement 50 fois par seconde.
L'ent�te du fichier **AYT** indique cette p�riode. 

Il est tr�s **important de s'assurer qu'aucune interruption ne pourra avoir lieu pendant l'appel du player**. 

Si vous n'�tes pas familier avec le syst�me des interruptions en Z80A, vous pouvez utiliser l'instruction **DI** avant l'appel du *player*.

		call AYT_Player	; Joue la musique

### Initialisation
Si le compresseur identifie des registres dit *inactifs*, ils sont exclus des donn�es **AYT** mais n�cessitent n�anmoins une initialisation pr�alable.

Le player du "Plus" �tant bien plus rapide que sur les CPCs de premi�re g�n�ration, sa vitesse d'ex�cution peut devenir inf�rieure � celle que prendrait la routine d'initialisation (qui n'utilise pas de **DMA**).
Il est donc n�cessaire d'appeler une fonction d'initialisation avant d'appeler le *player*.

La fonction *Ayt_Builder* construit une routine d'initialisation qui sera appel�e **avant** l'utilisation du *player*.

A la sortie de la fonction *Ayt_Builder*:
- le registre **HL** contient l'adresse de la routine d'initialisation.
- le registre **DE** contient le pointeur sur le premier octet libre apr�s le *player* (ou la routine d'initialisation).

**Remarque :**

Si le fichier ne contient aucun registre inactif, la routine d'initialisation devient inutile.
Dans ce cas, la fonction d'initialisation pointera sur un RET (et la routine d'initialisation occupera alors un octet au lieu de 34 dans le player).

Voici le traitement � mettre en place pour appeler une routine d'initialisation 

		ld ix,AYT_File		; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse o� le player sera construit
		ld a,2			; A indique combien de fois la musique sera jou�e
		ld bc,#aabb	; Configuration de la page Asic en entr�e et en sortie
		call Ayt_Builder
		ld (InitPlayer),hl	; Mise a jour de la routine d'initialisation
		...
		...
	InitPlayer equ $+1
		call 0			; Apr�s la mise � jour, CALL sera sur la routine d'initialisation
 
### P�riodicit� d'appel de Ayt_Player
La p�riodicit� d'appel du *player* est g�n�ralement bas�e sur la fr�quence de l'�cran, qui est de 50 Hertz. 
Cette information est disponible dans l'ent�te du fichier **AYT** (voir la description du format **AYT**)
.
Sur CPC+, la fr�quence de 50 Hz est induite par le param�trage du **CRTC 6845** "asic" qui signale son **signal VSYNC** via le **PPI 8255** "asic".
Il est possible de tester le signal Vsync avec le code suivant :

           ld b,#f5        ; port B du PPI 8255
    Wait_Vsync
            rra             ; Test du bit 0 
            jr nc,Wait_Vsync     ; Attente que le bit 0 passe � 1

Les interruptions sur les CPC offrent aussi la possibilit� de se synchroniser de mani�re tr�s pr�cise.

### Performances

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

| M�thode Appel | Config Asic      | Nombre Registres | CPU en Nops | Taille Player en octets | Taille Builder en octets |
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

 