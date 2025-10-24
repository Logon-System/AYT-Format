# LE FORMAT AYT
***Tronic*** (du groupe *GPA*), ***Longshot*** & ***Siko*** (du groupe *Logon System*) sont fiers de présenter un nouveau format de fichier audio appelé ***AYT***.
C'est un format compact associé utilisable simplement par tout programme nécessitant une haute performance **CPU** et un fonctionnement en **durée constante au cycle près**. 

Ce format est prévu pour toutes les plateformes utilisant un processeur sonore ***AY-3-8910/AY-3-8912*** (General Instrument) ou compatible (***YM-2149*** de Yamaha). 
Plusieurs outils de création et des players ont été réalisés et testés pour les plateformes suivantes : ***CPC-***, ***CPC+***, ***MSX***, ***ZX 128***, ***VG5000 (VG5210)*** 

![Image Presentation CPC+](./images/AYTPRES1.jpg)
# Principe 

A partir d'un fichier audio **YM5/YM6** [^1], un fichier au format ***AYT*** est généré grâce à un convertisseur. 
Ce fichier ***AYT*** peut ensuite être lu par un player optimisé sur plusieurs plateformes. 
Le compresseur exploite la nature séquentielle des musiques pour optimiser l'organisation en mémoire.
Cette organisation impacte in fine le player, qui est construit en fonction de la musique. 
Le fichier **YM** est découpé en patterns, qui sont agencés via des séquences de patterns.  
Des optimisations complémentaires comme le retrait des registres inutilisés ou l'ordonnancement des données permettent de réduire la taille des fichiers et/ou le temps **CPU**.

[^1]: Le format YM a été créé par Leonard (Arnaud Carré) http://leonard.oxg.free.fr/ymformat.html

# Caractéristiques 

- Plusieurs outils de compression sont disponibles pour créer des fichiers au format **AYT**
  - en ligne de commande quelque soit la plateforme.
  - directement sur le web.
- les fichiers **AYT** se compressent très bien avec ZX0 car les données ne sont pas déja compressées avec un algorithme de compression.
- un système de *builder* permet de créer un *player* à une adresse spécifiée et offre plusieurs avantages. 
  - il n'est plus nécessaire une fois le *player* créé et peut donc être totalement absent du programme dans lequel le *player* est utilisé.
  - il est de taille modeste.
  - il génère la routine d'initialisation des registres inacrtifs (cette routine étant transparente pour les utilisateurs de CPC "old")
  - il n'est pas nécessaire d'appeler le *builder* si la musique reboucle ou pour initialiser les registres. 
- le *player* est construit par le *builder* en fonction de la musique qu'il devra jouer
  - il est très performant en **CPU** (*voir tableaux de performances*).
  - il occupe peu de place en mémoire. 
  - il ne nécessite aucun buffer de décompression et permet ainsi de réduire l'empreinte mémoire du *player* en Ram.
  - il gère un nombre paramétrable de bouclage des musiques tel qu'il est défini dans le fichier **YM** (le rebouclage n'a pas forcément lieu au début)
  - il gère l'arrêt du son à l'issue du/des rebouclages (Les fichiers **YM** ne le font pas tous)
  - il fonctionne en temps constant dans toutes les circonstances (rebouclage, mute du player une fois la musique finie)
  - il permet de jouer des musiques de toutes tailles, et seulement limitées par l'espace adressable du processeur ou les capacités mémoire de la machine.


## Objectifs
### Création d'un format compact et performant

Un des **objectifs initiaux** de ce projet était **la création d'un nouveau format de fichier son conciliant un player très efficace en terme CPU, tout en restant raisonnable sur la taille des données en Ram**.

Il s'agissait de sortir du paradigme classique impliquant des compresseurs généralistes impliquant des buffers gourmands et ayant atteint leurs limites.
Une autre approche consistait à réfléchir préalablement à la nature des données pour pouvoir mieux les factoriser. Autrement dit, ne pas essayer de compacter les données comme un bourrin.

Le principe utilisé ne *"compresse"* donc pas les données brutalement mais s'appuie sur la logique particulière de compositions des chiptunes.
Cette particularité permet ensuite de compresser un fichier **AYT** avec un compacteur de type **ZX0** ou **Shrinker** de manière plus performante que si la compression avait été réalisée directement sur le fichier brut. Des statistiques sur le conversion **AYT** de **10 000 fichiers** l'ont démontré.
Cela permet ainsi de stocker un fichier **AYT** sur disque ou en ram avant usage de manière plus compacte que de nombreuses solutions existantes.

 
Comme souvent lorsqu'on tend vers les limites d'architecture, la vitesse s'oppose à la place occupée en mémoire (*There is no free lunch*).
Le compromis **Cpu versus Taille** du format **AYT** nous semble correcte.

Des réflexions pour réduire la taille des fichiers **AYT** sont à l'étude, car ce format le permet facilement.
En effet, les *patterns* n'étant pas compressés, il est facilement envisageable de les compresser de manière plus *"légères"* qu'avec des poids lourds de la compression.
Il devrait ainsi conserver des atouts pour rester très performant au niveau **CPU**. :-)

#### Simplification de la compression
Un autre **objectif important** était de **simplifier le processus de création et d'utilisation de ce format**. 
En effet, trop souvent dans ce domaine, les choses se révèlent plus compliquées qu'annoncées, autant au niveau des **compresseurs que des players**.

Au niveau de la compression, lorsque d'autres formats sont convertis à partir du format **YM**, il est souvent nécessaire de saisir des options parfois peu compréhensibles.
Une conversion à partir du format **YM** suppose également de tenir compte des informations relatives à la **fréquence des sons de la plateforme source et ceux de la plateforme d'arrivée**. 
La plupart du temps ces projets sont construits en mode *"mono plateforme"*.

Si ce type d'option reste possible avec les outils créés pour **AYT**, le processus a été grandement simplifié grâce à une **codification des plateformes** (voir format du **AYT**) et le souci d'en demander le moins possible à l'utilisateur lorsque le programme peut faire le job tout seul.
Il suffit par exemple d'indiquer au compresseur quelle est *la plateforme cible* et il se charge seul de la conversion en tenant compte de la fréquence de base du circuit de la plateforme source et celle d'arrivée, tout en calculant les meilleurs compromis en terme de taille. 
En effet, des conversions de fréquence peuvent rendre certains registres inactifs et réduire ainsi notablement la taille du fichier **AYT**.


Le format **AYT contient dans son header l'information de la plateforme et la fréquence d'appel du player**, ce qui permet de convertir des fichiers **AYT** prévus pour une *plateforme A*vers une *plateforme B*.

Le compresseur existe sous différents formats et il est également accessible via une **interface web** très simple à utiliser, puisqu'il suffit de poser un YM en Drag'N'Drop pour le convertir.
L'algorithme cherchera la meilleure compression possible sans qu'il soit nécessaire de refaire des essais.

Enfin, et parce que le format **YM** est parfois taillé à la hâche (fichiers mal "terminés" qui embarquent le début d'un rebouclage), une autre interface web a été conçue autour de ce projet et **permet de séquencer (écouter, couper, manipuler des fichiers YM)**.

De quoi réaliser de beaux **medley AY**...

#### Simplification du player
Au niveau de l'utilisation d'un fichier **AYT**, le processus a été simplifié à l'extrême. 

La notion de *"builder"* a été adoptée. Il s'agit d'une fonction qui crée ex nihilo le code du *player* en fonction de l'adresse souhaitée pour ce dernier et l'adresse du fichier musical.
Cela permet de disposer d'un *player* dont le code est parfaitement adapté à la musique qu'il doit jouer.

Cette notion permet de pré-initialiser le *player* et la musique sans que le *builder* soit nécessaire dans le code final. Afin d'être complet, le *builder* génère donc également une méthode pour initialiser les registres inactifs selon la plateforme utilisée.

Sur un **CPC 464/664/6128** par exemple, le programme indique au *builder* l'adresse où le *player* doit être créé, l'adresse où la musique se trouve et le nombre de fois qu'elle sera jouée.

Le *builder* renvoie au programme **l'adresse du premier octet libre après le player**, ainsi que **le nombre de µsecondes qu'occupera le player avec le CALL**.
(cette information peut être utile pour les programmes gérant des processus en temps constant).

Lorsque le *player* est appelé la **première fois**, il initialise les registres *inactifs*. 
Ce processus est réalisé  avec **la même durée CPU que pour les appels suivants** qui vont jouer la musique.



# Remerciements
Nous tenons à remercier les personnes suivantes pour leur enthousiasme et leur support, ainsi que leurs conseils et leurs idées, tout au long du processus de maturation de ce nouveau format.

- **Candy (Sébastien Broudin)** qui cogitait depuis déjà un moment sur la *patternisation* des **YM** avec quelques essais probants et qui a particulièrement suivi nos progrès sur le sujet.
- **Fred Crazy (Frédéric Floch)**, **Ker (Florian Bricogne)**, **Overflow (Olivier Goût)**, **DManu78 (David Manuel)**, **Cheshirecat (Claire Dupas)**, **Shap (Olivier Antoine)** pour leurs conseils avisés autant techniques que musicaux et pour leurs encouragements.
- **BdcIron (Amaury Duran)** pour son aide pour les tests du player sur VG5000 et sa connaissance du ZX Spectrum. 
- **Ced (Cédric Quétier)** pour le fabuleux logo AYT associé à la présentation du *player* sur CPC+
- **Made (Carlos Pardo)** pour le somptueux logo AYT associé à la présentation du player sur "CPC old".













