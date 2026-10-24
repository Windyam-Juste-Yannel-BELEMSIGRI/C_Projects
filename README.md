# Fillit : guide de résolution détaillé

Ce guide explique, étape par étape, comment fonctionne la solution fournie dans `fillit.c`. Il couvre le raisonnement, les structures de données, chaque fonction, et des exemples concrets.

## Sommaire

1. [Le problème en bref](#1-le-problème-en-bref)
2. [Vue d'ensemble](#2-vue-densemble)
3. [Structures de données](#3-structures-de-données)
4. [Étape 1 : lire le fichier](#4-étape-1--lire-le-fichier)
5. [Étape 2 : valider et convertir les pièces](#5-étape-2--valider-et-convertir-les-pièces)
6. [Étape 3 : résoudre (backtracking)](#6-étape-3--résoudre-backtracking)
7. [Étape 4 : le `main` et l'affichage](#7-étape-4--le-main-et-laffichage)
8. [Le Makefile](#8-le-makefile)
9. [Exemples de tests](#9-exemples-de-tests)
10. [Performance et pistes d'amélioration](#10-performance-et-pistes-damélioration)
11. [Pièges fréquents](#11-pièges-fréquents)
12. [Pistes de recherche](#12-pistes-de-recherche)

---

## 1. Le problème en bref

**Entrée** : un fichier décrivant des Tetriminos (pièces de Tetris de 4 blocs), chacun sur 4 lignes de 4 caractères (`#` = bloc, `.` = vide), séparés par une ligne vide.

**Sortie** : le plus petit carré contenant toutes les pièces, sans chevauchement et **sans rotation**. Chaque pièce est affichée avec une lettre (`A`, `B`, `C`…) selon son ordre dans le fichier. Les cases vides sont des `.`.

**Règles à retenir :**

| Règle du sujet | Conséquence |
|---|---|
| Aucune rotation | Une seule orientation par pièce |
| Pièces placées dans l'ordre du fichier | Pas de permutation des pièces à explorer |
| Chaque pièce le plus haut, puis le plus à gauche | Ordre de parcours des positions imposé |
| Erreur dans le fichier | Afficher `error` et quitter |
| Nombre d'arguments différent de 1 | Afficher l'usage et quitter |
| Maximum 26 pièces | Lettres `A` à `Z` |

Exemple d'entrée et de sortie :

Fichier `sample.fillit` (4 pièces, séparées par une ligne vide) :

```
....
##..
.#..
.#..

....
####
....
....

#...
###.
....
....

....
##..
.##.
....
```

Commande et sortie (pièces `A`, `B`, `C`, `D` dans l'ordre du fichier) :

```
$> ./fillit sample.fillit
DDAA
CDDA
CCCA
BBBB
```

---

## 2. Vue d'ensemble

Le programme suit cinq phases :

```
  Lire le fichier  (read_file)
        │
        ▼
  Valider + convertir chaque pièce  (parse_all, parse_piece)
        │                                  │ invalide
        │                                  ▼
        │                              "error"
        ▼
  Calculer la taille de départ du carré  (main)
        │
        ▼
  ┌──► Essayer de poser toutes les pièces  (solve, can_place, put, same)
  │         │ échec                │ succès
  │         ▼                      ▼
  └─ agrandir le carré (size++)   afficher le carré  (main)
```

Idée centrale : on essaie un carré de taille `size`. S'il n'existe aucune solution, on passe à `size + 1`. Le premier carré qui marche est forcément le plus petit.

---

## 3. Structures de données

### 3.1 `t_piece` : une pièce

```c
typedef struct s_piece
{
	int		x[4];     /* colonnes des 4 blocs */
	int		y[4];     /* lignes des 4 blocs   */
	int		pos;      /* position où la pièce est posée (index linéaire) */
	char	letter;   /* 'A', 'B', 'C'... */
}			t_piece;
```

Une pièce est stockée comme **4 couples `(x[k], y[k])`**, normalisés : le plus petit `x` vaut 0 et le plus petit `y` vaut 0. On ne garde que la forme, pas sa position dans la grille 4×4 du fichier.

Exemple : cette pièce du fichier

```
....
##..
.#..
.#..
```

a ses blocs bruts en `(0,1) (1,1) (1,2) (1,3)` (colonne, ligne). Après normalisation (on soustrait `min x = 0` et `min y = 1`) :

```
x[] = {0, 1, 1, 1}
y[] = {0, 0, 1, 2}
```

qui dessine :

```
##
.#
.#
```

Le champ `pos` sert à une optimisation (pièces identiques, voir §6.5). Le champ `letter` est la lettre écrite sur le plateau.

### 3.2 `g_board` : le plateau

```c
static char g_board[BOARD_MAX][BOARD_MAX];   /* 32 × 32 */
```

Un tableau 2D de caractères. On n'en utilise que `size × size` cases. Une case contient `'.'` si elle est vide, ou la lettre de la pièce qui l'occupe.

Il est global (`static`, visible uniquement dans ce fichier) pour ne pas avoir à le passer en paramètre à chaque appel récursif. Taille fixe de 32 : le côté ne dépasse jamais cette valeur avec 26 pièces maximum.

### 3.3 Les constantes

```c
#define MAX_PIECES 26     /* lettres A..Z */
#define BUF_SIZE 600      /* 26 × 21 − 1 = 545 octets max, avec marge */
#define BOARD_MAX 32      /* côté maximal du plateau */
```

### 3.4 Le tampon de lecture

```c
char buf[BUF_SIZE];
```

Le fichier entier est lu dans ce tableau en une fois, puis analysé. Pas de `malloc` : aucune fuite mémoire possible.

---

## 4. Étape 1 : lire le fichier

### `read_file(const char *path, char *buf)`

**Rôle** : ouvrir le fichier, le lire entièrement dans `buf`, renvoyer le nombre d'octets lus (ou -1 en cas d'erreur).

```c
fd = open(path, O_RDONLY);
if (fd < 0)
	return (-1);                       /* fichier inexistant */
total = 0;
while ((ret = read(fd, buf + total, BUF_SIZE - total)) > 0)
	total += ret;                      /* on accumule les octets */
close(fd);
if (ret < 0 || total >= BUF_SIZE)
	return (-1);                       /* erreur de lecture, ou fichier trop gros */
return (total);
```

Pourquoi une boucle ? `read` peut renvoyer moins d'octets que demandé. On lit donc jusqu'à ce qu'il renvoie 0 (fin de fichier).

Pourquoi `total >= BUF_SIZE` est une erreur ? Un fichier valide fait au plus 545 octets. S'il atteint 600, c'est qu'il est trop grand, donc invalide.

---

## 5. Étape 2 : valider et convertir les pièces

### 5.1 `parse_all(const char *path, t_piece *p)`

**Rôle** : lire le fichier, vérifier sa structure globale, remplir le tableau de pièces. Renvoie le nombre de pièces, ou -1 en cas d'erreur.

#### Pourquoi la taille vaut `21 × n − 1`

Une pièce = 4 lignes × (4 caractères + 1 `\n`) = **20 octets**. Entre deux pièces : une ligne vide = **1 octet** (`\n`). Pour `n` pièces, il y a `n − 1` séparateurs (pas de séparateur après la dernière) :

```
20 × n + (n − 1) = 21 × n − 1
```

| n | Taille attendue |
|---|---|
| 1 | 20 |
| 2 | 41 |
| 3 | 62 |
| 26 | 545 |

Donc un fichier est valide seulement si `total % 21 == 20`, et alors `n = (total + 1) / 21`.

```c
total = read_file(path, buf);
if (total < 0 || total % 21 != 20)
	return (-1);
n = (total + 1) / 21;
if (n > MAX_PIECES)
	return (-1);
```

Ce simple test détecte d'un coup : fichier vide, ligne vide en trop à la fin, séparateur manquant, ligne trop courte ou trop longue.

#### Boucle sur les pièces

La pièce `k` commence à l'octet `21 × k`. On la valide, on vérifie que l'octet de séparation est bien un `\n` (sauf pour la dernière), et on lui assigne sa lettre :

```c
while (++k < n)
{
	if (!parse_piece(buf + 21 * k, &p[k]))
		return (-1);
	if (k < n - 1 && buf[21 * k + 20] != '\n')
		return (-1);
	p[k].letter = 'A' + k;
}
```

### 5.2 `parse_piece(const char *s, t_piece *p)`

**Rôle** : valider une pièce (20 caractères) et la convertir en coordonnées normalisées. Renvoie 1 si valide, 0 sinon.

#### Contrôles effectués

```c
while (++i < 20)
{
	if (i % 5 == 4 && s[i] != '\n')   /* indices 4, 9, 14, 19 : retour à la ligne */
		return (0);
	if (i % 5 == 4)
		continue ;
	if (s[i] != '.' && s[i] != '#')   /* seulement . ou # */
		return (0);
	if (s[i] == '#' && n++ < 4)       /* on enregistre les 4 premiers blocs */
	{
		p->x[n - 1] = i % 5;          /* colonne */
		p->y[n - 1] = i / 5;          /* ligne   */
		links += (i % 5 > 0 && s[i - 1] == '#') + (i >= 5 && s[i - 5] == '#');
	}
}
if (n != 4 || links < 3)
	return (0);
```

Détails :

- **Position dans la grille** : le caractère d'indice `i` est à la colonne `i % 5` et à la ligne `i / 5` (5 car chaque ligne fait 4 caractères + `\n`).
- **Le test `n++ < 4`** : `n` compte les `#` rencontrés. S'il y en a plus de 4, `n` dépasse 4 sans que l'on enregistre le bloc (pas de débordement du tableau), et le test final `n != 4` rejette la pièce.
- **Les liens (`links`)** : pour chaque `#`, on regarde son voisin de **gauche** (`s[i-1]`) et son voisin du **haut** (`s[i-5]`). Chaque contact entre deux blocs est ainsi compté **une seule fois**.

#### Pourquoi `links >= 3` garantit une pièce connectée

Il faut que les 4 blocs forment un seul morceau.

```
Pièce valide (L)       Pièce valide (carré)    Pièce invalide
   #.                     ##                      #.
   #.   3 contacts        ##   4 contacts         #.   1 contact
   ##                                             ..
                                                  .#
```

Sur une grille, trois blocs ne peuvent pas former un triangle. Donc 4 blocs avec 3 contacts forment un chemin ou une étoile, jamais deux groupes séparés. Avec moins de 3 contacts, au moins un bloc est isolé.

#### Normalisation

Après validation, on décale la pièce dans le coin haut-gauche :

```c
min[0] = 4;  min[1] = 4;
/* trouver le plus petit x et le plus petit y */
/* puis soustraire : */
p->x[i] -= min[0];
p->y[i] -= min[1];
```

Cela permet à deux écritures différentes d'une même pièce de donner **exactement les mêmes coordonnées**. Exemple avec la même pièce en S écrite à trois hauteurs différentes :

```
.##. | .... | ....
##.. | .##. | ....
.... | ##.. | .##.
.... | .... | ##..
```

Les coordonnées brutes sont `(1,0) (2,0) (0,1) (1,1)`, puis `(1,1) (2,1) (0,2) (1,2)`, puis `(1,2) (2,2) (0,3) (1,3)`. Après normalisation, les trois donnent :

```
x[] = {1, 2, 0, 1}
y[] = {0, 0, 1, 1}
```

Le plus simple à retenir : **on ne garde que la forme, pas l'endroit où elle était écrite**.

---

## 6. Étape 3 : résoudre (backtracking)

### 6.1 Principe

On pose les pièces une par une, dans l'ordre. Pour chaque pièce, on essaie chaque place du plateau (ligne par ligne, de gauche à droite). Si une pièce ne trouve plus de place, on **revient en arrière** : on retire la pièce précédente et on la décale.

C'est comme un labyrinthe : on avance, et en cas d'impasse on revient au dernier carrefour pour prendre un autre chemin.

### 6.2 Position et coordonnées

Les places du plateau sont numérotées ligne par ligne :

```
size = 3 :   0 1 2
             3 4 5
             6 7 8
```

```
pos = y * size + x      ⟺      x = pos % size ,  y = pos / size
```

Parcourir `pos = 0, 1, 2, …` donne exactement l'ordre « le plus haut, puis le plus à gauche » demandé par le sujet. La première solution trouvée est donc la bonne, sans code supplémentaire.

### 6.3 `can_place(t_piece *p, int x, int y, int size)`

**Rôle** : dire si la pièce rentre avec son coin haut-gauche en `(x, y)`.

```c
while (++k < 4)
{
	cx = x + p->x[k];
	cy = y + p->y[k];
	if (cx >= size || cy >= size || g_board[cy][cx] != '.')
		return (0);
}
return (1);
```

Pour chacun des 4 blocs, on calcule sa position réelle sur le plateau. Le placement est refusé si le bloc dépasse du carré, ou tombe sur une case déjà occupée.

### 6.4 `put(t_piece *p, int pos, int size, char c)`

**Rôle** : écrire le caractère `c` sur les 4 cases de la pièce posée à la position `pos`.

```c
g_board[pos / size + p->y[k]][pos % size + p->x[k]] = c;
```

Cette fonction sert à la fois à **poser** (`c` = lettre de la pièce) et à **retirer** (`c = '.'`). Une seule fonction pour les deux, donc moins de risque d'erreur.

### 6.5 `same(t_piece *a, t_piece *b)`

**Rôle** : dire si deux pièces ont exactement la même forme (les 8 coordonnées identiques).

Grâce à la normalisation, la comparaison est triviale. Elle sert à l'optimisation des pièces identiques, expliquée juste après.

### 6.6 `solve(t_piece *p, int n, int i, int size)`

**Rôle** : essayer de poser les pièces `i` à `n − 1`, en supposant que les pièces `0` à `i − 1` sont déjà posées. Renvoie 1 en cas de succès, 0 sinon.

```c
static int	solve(t_piece *p, int n, int i, int size)
{
	int	pos;

	if (i == n)
		return (1);                          /* toutes posées : succès */
	pos = 0;
	if (i > 0 && same(&p[i], &p[i - 1]))
		pos = p[i - 1].pos + 1;              /* optimisation pièces identiques */
	while (pos < size * size)
	{
		if (can_place(&p[i], pos % size, pos / size, size))
		{
			put(&p[i], pos, size, p[i].letter);   /* je pose */
			p[i].pos = pos;
			if (solve(p, n, i + 1, size))         /* je continue */
				return (1);
			put(&p[i], pos, size, '.');           /* je retire (retour en arrière) */
		}
		pos++;
	}
	return (0);                               /* aucune place : échec */
}
```

Lecture en français :

1. Si toutes les pièces sont posées, c'est gagné.
2. Pour chaque position, de la première à la dernière :
   - si la pièce `i` rentre ici, on la pose ;
   - on essaie de poser les pièces suivantes ;
   - si ça échoue, on retire la pièce `i` et on essaie la position suivante.
3. Si aucune position ne marche, on renvoie 0 : l'appelant devra changer son propre choix.

**Invariant important** : quand `solve` renvoie 0, le plateau est exactement dans l'état où il était à l'entrée. C'est ce qui rend le retour en arrière fiable : si on oublie de retirer une pièce, des « pièces fantômes » polluent les essais suivants.

#### L'optimisation des pièces identiques

Si deux pièces consécutives sont identiques, les placer dans l'ordre A puis B, ou B puis A, donne la même figure aux lettres près.

```
A en place 0, B en place 2     ≡     A en place 2, B en place 0
A A B B                              B B A A
A A B B                              B B A A
```

On impose donc que B commence **après** la place de A (`pos = p[i-1].pos + 1`). On évite d'explorer des doublons, et la première solution trouvée reste la même.

### 6.7 Exemple détaillé : un carré et un L

Pièces : `A` = carré 2×2, `B` = L (3 de haut, 2 de large). Il y a 8 cases à placer, donc `size` de départ = 3 (car 3² = 9 ≥ 8).

```
A :  ##     B :  #.
     ##          #.
                 ##
```

Déroulement :

```
1. solve(i=0) : A essaie pos 0 → (0,0) libre : on pose A.
        A A .
        A A .
        . . .

2.   solve(i=1) : B essaie toutes ses places
        pos 0 (x=0,y=0) → (0,0) déjà pris par A           ✗
        pos 1 (x=1,y=0) → (1,0) déjà pris par A           ✗
        pos 2 : B dépasse à droite                         ✗
        pos 3 à 8 : B dépasse en bas                       ✗
        → solve(1) renvoie 0 : échec

3. Retour en arrière : on retire A.
        . . .
        . . .
        . . .

4. A essaie pos 1 → (1,0) : on pose A.
        . A A
        . A A
        . . .

5.   solve(i=1) : B essaie pos 0 → (0,0), (0,1), (0,2), (1,2) : tous libres → on pose B.
        B A A
        B A A
        B B .

6.     solve(i=2) : i == n → succès.
```

Résultat affiché :

```
BAA
BAA
BB.
```

(La lettre `A` désigne le carré, première pièce du fichier, et `B` le L.)

### 6.8 Si 3×3 n'avait pas suffi

Avec une barre verticale (4 de haut) et un carré, `size = 3` est le point de départ, mais la barre ne rentre dans aucune place (elle dépasse toujours). `solve` renvoie 0 immédiatement, et le `main` passe à `size = 4`.

---

## 7. Étape 4 : le `main` et l'affichage

```c
int	main(int argc, char **argv)
{
	/* 1. vérifier les arguments */
	if (argc != 2)
	{
		write(1, "usage: ./fillit source_file\n", 28);
		return (1);
	}
	/* 2. lire et valider le fichier */
	if ((n = parse_all(argv[1], p)) < 1)
		return (error());                 /* affiche "error\n" */
	/* 3. initialiser le plateau avec des '.' */
	/* 4. taille de départ */
	size = 2;
	while (size * size < 4 * n)
		size++;
	/* 5. résoudre, en agrandissant si besoin */
	while (!solve(p, n, 0, size))
		size++;
	/* 6. afficher */
	i = -1;
	while (++i < size)
	{
		write(1, g_board[i], size);
		write(1, "\n", 1);
	}
	return (0);
}
```

### Pourquoi la taille de départ est `size² ≥ 4n`

`n` pièces occupent `4n` cases. Un carré de moins de `4n` cases ne peut pas les contenir. Inutile de l'essayer.

```
n = 2  →  8 cases   →  size = 3   (9 ≥ 8)
n = 4  →  16 cases  →  size = 4   (16 ≥ 16)
n = 7  →  28 cases  →  size = 6   (36 ≥ 28, mais 25 < 28)
```

### Pourquoi on ne réinitialise pas le plateau entre deux tailles

Quand `solve` échoue, toutes les pièces posées ont été retirées (voir l'invariant du §6.6). Le plateau est donc déjà vide quand on passe à `size + 1`.

### Affichage

On écrit chaque ligne avec `write(1, g_board[i], size)` puis un retour à la ligne. Les cases non utilisées par une pièce restent des `.`.

---

## 8. Le Makefile

```make
NAME = fillit
SRC = fillit.c
OBJ = $(SRC:.c=.o)
CFLAGS = -Wall -Wextra -Werror

all: $(NAME)

$(NAME): $(OBJ)
	gcc $(CFLAGS) -o $(NAME) $(OBJ)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
```

| Élément | Rôle |
|---|---|
| `NAME` | Nom de l'exécutable (imposé : `fillit`) |
| `SRC` | Liste des fichiers sources, à compléter quand le code est découpé |
| `OBJ = $(SRC:.c=.o)` | Transforme chaque `x.c` en `x.o` |
| `CFLAGS` | Flags imposés par le sujet. Aucun autre n'est autorisé, surtout pas `-O2` |
| `all` | Règle par défaut : construit l'exécutable |
| `$(NAME): $(OBJ)` | Lie les fichiers objets en l'exécutable final |
| `%.o: %.c` | Règle générique : compile un `.c` en `.o` (`$<` = source, `$@` = cible) |
| `clean` | Supprime les `.o` |
| `fclean` | Supprime les `.o` et l'exécutable |
| `re` | Repart de zéro (`fclean` puis `all`) |
| `.PHONY` | Indique que ces noms sont des actions, pas des fichiers |

Les lignes de commandes doivent commencer par une **tabulation**, pas des espaces.

`make` ne recompile que ce qui a changé. Si tout est à jour, il doit répondre « Nothing to be done » (c'est vérifié lors de l'évaluation).

---

## 9. Exemples de tests

### Cas valides

| Entrée | Sortie attendue |
|---|---|
| Une barre verticale seule | Carré 4×4 avec `A` en colonne 0 |
| Deux carrés | Carré 4×4 : `AABB / AABB / .... / ....` |
| L'exemple du sujet (4 pièces) | `DDAA / CDDA / CCCA / BBBB` |
| L'exemple du sujet (8 pièces) | `ABBBB. / ACCCEE / AFFCEE / A.FFGG / HHHDDG / .HDD.G` |

### Cas invalides (doivent afficher `error`)

| Cas | Pourquoi |
|---|---|
| Fichier inexistant | `open` échoue |
| Fichier vide | `0 % 21 ≠ 20` |
| Ligne vide en trop à la fin | taille = `21n` au lieu de `21n − 1` |
| Deux lignes vides entre deux pièces | taille incorrecte |
| Ligne de 3 caractères | taille incorrecte |
| Caractère autre que `#`, `.` ou `\n` | rejeté par `parse_piece` |
| 5 `#` dans une pièce | `n != 4` |
| 3 `#` dans une pièce | `n != 4` |
| Deux blocs séparés | `links < 3` |
| Plus de 26 pièces | `n > MAX_PIECES` |

### Commandes utiles

```sh
make
./fillit fichier.fillit              # résolution
./fillit                             # affiche l'usage
./fillit a b                         # affiche l'usage
./fillit inexistant                  # affiche error
./fillit fichier.fillit | cat -e     # voir les retours à la ligne ($)
```

---

## 10. Performance et pistes d'amélioration

Cette version est **correcte** mais pas optimisée. Sur 12 pièces variées, elle peut mettre plusieurs secondes. Le temps explose avec le nombre de pièces, car le nombre de combinaisons possibles croît très vite.

### Pistes, de la plus simple à la plus avancée

1. **Ignorer les positions impossibles.** Si la pièce dépasse du carré à droite (`x + largeur > size`), inutile de tester les autres positions de la même ligne : sauter à la ligne suivante.
2. **Élaguer par comptage.** Si le nombre de cases libres est inférieur à `4 × pièces_restantes`, abandonner la branche.
3. **Bitmasks.** Représenter chaque ligne du plateau par un entier (1 bit par case). Tester une collision devient un `&` binaire au lieu de 4 lectures de tableau.
4. **Précalculer** la largeur et la hauteur de chaque pièce au moment du parsing.

Aucune de ces optimisations ne change le résultat : elles ne font qu'éviter des tests inutiles.

---

## 11. Pièges fréquents

| Piège | Conséquence |
|---|---|
| Oublier de retirer la pièce après un échec | Pièces fantômes, résultats faux |
| Oublier `close(fd)` | Fuite de descripteur |
| Ne pas gérer `read` qui renvoie moins que demandé | Lecture partielle du fichier |
| Compter chaque contact deux fois | Seuil de 3 liens faux |
| Accepter une ligne vide à la fin du fichier | Le sujet l'interdit (voir `invalid_sample`) |
| Afficher `error` sur la sortie d'erreur | Le sujet demande la sortie standard |
| Utiliser une fonction non autorisée (`printf`, `strlen`…) | Seuls `exit`, `open`, `close`, `write`, `read`, `malloc`, `free` sont permis |
| Oublier de redessiner `.` dans les cases vides | La sortie doit contenir des `.` |

### Pour respecter la Norme 42

Le code fourni est dans un seul fichier pour la lisibilité. La Norme impose notamment : 25 lignes maximum par fonction, 5 fonctions maximum par fichier, pas de `for`, déclarations en début de fonction, 80 colonnes maximum. Il faudra donc découper le code, par exemple :

```
fillit.h      → structure t_piece, prototypes, constantes
main.c        → main, error
parse.c       → read_file, parse_all, parse_piece
solve.c       → can_place, put, same, solve
```

Pensez à lancer `norminette` pour vérifier.

---

## 12. Pistes de recherche

Pour progresser seul sur ce type de problèmes :

- **backtracking algorithm** et **N-Queens problem** : le meilleur entraînement.
- **sudoku solver backtracking** : même mécanique, sur un problème familier.
- **recursion explained** : le retour en arrière repose sur la récursivité.
- **polyomino packing / tetromino packing** : le nom mathématique du problème.
- **exact cover problem**, **Algorithm X**, **Dancing Links** : pour aller plus loin.
- **bitwise operators in C**, **bitmask** : pour la version rapide.
- **pruning / symmetry breaking** : techniques d'élagage.
- **norminette** : `pip install norminette` fonctionne hors de l'école.

### Méthode générale

1. Traduire chaque phrase du sujet en contrainte.
2. Résoudre à la main un exemple minuscule.
3. Choisir une représentation qui simplifie les tests (ici : coordonnées normalisées).
4. Chercher des propriétés simples à vérifier (ici : `21n − 1`).
5. Écrire d'abord une version **correcte**, puis une version **rapide**.
