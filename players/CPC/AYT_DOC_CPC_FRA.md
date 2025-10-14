

## Appel de Ayt_Builder sur CPC 464/664/6128

		ld ix,AYT_File		; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player	; AYT_Player est l'adresse o� le player sera construit
		ld a,2			; A indique combien de fois la musique sera jou�e
		call Ayt_Builder

A la sortie de la fonction *Ayt_Builder* :
- le registre **DE** contient l'adresse du premier octet libre apr�s le *player*.
- le registre **HL** contient le nombre de �secondes (NOPs) consomm� par l'appel du *player*.

Pour jouer la musique, il faut appeler le *player* � la fr�quence requise. 
La majorit� des musiques n�cessitent que le *player* soit appel� p�riodiquement 50 fois par seconde.
L'ent�te du fichier **AYT** indique cette p�riode. 

Il est tr�s **important de s'assurer qu'aucune interruption ne pourra avoir lieu pendant l'appel du player**. Si vous n'�tes pas familier avec le syst�me des interruptions en **Z80A**, vous pouvez utiliser l'instruction **DI** avant l'appel du *player*.

		call AYT_Player	; Joue la musique

### Option de compilation
#### PlayerAcessByJP
Si l'option **PlayerAcessByJP** vaut 1, il faut �galement d�finir l'adresse de retour du *player*.

		ld ix,AYT_File			; AYT_File est l'adresse o� se trouve le fichier AYT
		ld de,AYT_Player		; AYT_Player est l'adresse o� le player sera construit
		ld hl,AYT_Player_Ret		; AYT_Player_Ret est l'adresse � laquelle le player revient
		ld a,2				; A indique combien de fois la musique sera jou�e
		call Ayt_Builder

On retrouve l'adresse de retour derri�re l'appel du *player** 

			jp AYT_Player	; Joue la musique
	AYT_Player_Ret			; Adresse de retour du player

Sauf � vouloir briller en soci�t� en pr�tendant avoir �conomis� **11 �secondes**, il est conseill� de laisser cette option � 0, ce qui vous �vitera notamment de devoir sauvegarder et restituer le pointeur de pile.

 
Vous pouvez vous rapporter au tableau des performances en bas du document pour connaitre la Cpu et la taille du *player* g�n�r�.

***Remarques :***
- Le *player* met le circuit sonore en sourdine une fois le nombre de reboucle effectu�.
- Si des registres n�cessitent une initialisation, cette derni�re est faite de mani�re transparente avec le 1er appel.
- La reboucle a lieu � l'endroit ou elle a �t� pr�vue par le fichier **YM**.
- Que ce soit lors de la reboucle ou lors de la mise en sourdine du *player*, ce dernier respectera � chaque appel une dur�e de Cpu constante.

### P�riodicit� d'appel de Ayt_Player
La p�riodicit� d'appel du *player* est g�n�ralement bas�e sur la fr�quence de l'�cran, qui est de 50 Hertz. 
Cette information est disponible dans l'ent�te du fichier AYT (voir la description du format **AYT**).
Sur CPC 464/664/6128, la fr�quence de 50 Hz est induite par le param�trage du **CRTC 6845**, qui signale son signal de Vsync via le **PPI 8255**.
Il est possible de tester le signal de Vsync avec le code suivant :

           ld b,#f5        ; port B du PPI 8255
    Wait_Vsync
            rra             ; Test du bit 0 
            jr nc,Wait_Vsync     ; Attente que le bit 0 passe � 1

Les interruptions sur les CPC offrent aussi la possibilit� de se synchroniser de mani�re tr�s pr�cise.

### Pr�-Construction
Il est tout � fait possible de ***"pr� construire"*** le *player*.

Vous pouvez utiliser *Ayt_Builder* pour cr�er pr�alablement le *player* et initialiser le fichier **AYT**.
Il suffit de sauvegarder le player cr�� et le fichier **AYT** mis � jour apr�s l'appel de la fonction.

Vous pouvez ensuite int�grer le *player* et le fichier **AYT** en prenant soin de les replacer aux adresses d�finies lors de l'appel � *Ayt_Builder*.

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

| M�thode Appel | Nombre Registres | CPU en Nops | Taille Player en octets | Taille Builder en octets |
| :-----------: | :--------------: | :---------: | :-----------: | :------------: |
| JP            | 10               | 358         | 247           | 466            |
| JP            | 11               | 389         | 264           | 466            |        
| JP            | 12               | 320         | 281           | 466            |        
| JP            | 13               | 450         | 297           | 466            |        
| JP            | 14               | 479         | 312           | 466            |        
| CALL          | 10               | 369         | 252           | 480            |
| CALL          | 11               | 400         | 268           | 480            |
| CALL          | 12               | 431         | 286           | 480            |
| CALL          | 13               | 461         | 302           | 480            |
| CALL          | 14               | 490         | 317           | 480            |
