/**
 *      CTextMovingMap
 *      ==============
 * 
 *      Démo de carte déroulante texte en C pour Oric
 *      par laurentd75, Jan. 2019
 * 
 *      Version: 1.2
 *         Date: 10/01/19
 * 
 *      Historique:
 *      -----------------------------------------------------------------------------------------
 *      v1.0    - première version, accès aux éléments individuels du tableau représentant la carte
 *      v1.1    - optimisation affichage carte: accès uniquement à 1 élément du tableau (coin supérieur
 *                gauche de la partie visible) et manipulation de pointeur pour accéder aux éléments
 *                suivants à afficher. 
 *                => Gain en rapidité: x4 !!!
 *      v1.1.1  - petite correction de bug dans init_map() (confusion entre MAP_XSIZE et MAP_YSIZE 
 *                 dans une des boucles)
 *      v1.2    - optimisation initialisation carte: limitation accès individuels aux éléments du tableau
 *                au strict minimum, manipulation de pointeurs pour le reste
 *                => gain de temps significatif (x2 ?) à l'initialisation
 * 
 *  ========( branche "2_cells_per_byte" basée sur v1.2 branche master ) ====================================
 *  = Expérimentation: compactage du tableau: 1 case (1 octet) représente 2 cellules horizontales:
 *  = quartet inférieur (bits 0 à 3) = cellule de n° impair
 *  = quartet supérieur (bits 4 à 7) = cellule de n° pair
 *  =
 *  =========================================================================================================
 */ 


#include <stdio.h>
#include <sys/graphics.h>
//#include <lib.h> // désativé, car définition de fonction rand() buggée ici


/* ================== TYPES ================== */
typedef unsigned char uchar;
typedef unsigned char bool; // boolean


/* ================== CONSTANTES ================== */

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef FALSE
#define FALSE ((bool) 0)
#define TRUE  (!FALSE)
#endif

// Adresse de début de l'écran TEXT
#define TEXT_SCREEN 0xBB80

// Dimensions de l'écran
#define SCREEN_WIDTH  40
#define SCREEN_HEIGHT 28

// Dimensions X et Y de la fenêtre d'affichage
#define WIN_XSIZE  30
#define WIN_YSIZE  15

// Coordonnées du coin supérieur gauche de la fenêtre d'affichage
#define WX  5
#define WY  5

// Dimensions X et Y de la carte
#define MAP_XSIZE   100
#define MAP_YSIZE   100

// Manipulation des quartets d'une cellule
// Le quartet supérieur ("poids fort") représentera TOUJOURS une cellule de n° PAIR
// Le quartet inférieur ("poids faible") représentera TOUJOURS une cellule de n° IMPAIR
#define get_high_quartet(val)  ((val & 0xF0) >> 4)
#define get_low_quartet(val)    (val & 0x0F)

// déterminer si un nombre (n° de case) est pair ou impair: il suffit juste de tester le bit 0 du nombre...
#define is_even(x) ((x & 1) == 0)
#define is_odd(x)  ((x & 1) != 0)

// renvoie le quartet de la valeur correspondant au  n° de cellule (quartet inférieur ou supérieur selon si le N° est impair ou pair)
#define get_cellvalue(x, val) (is_odd(x) ?  get_low_quartet(val) :  get_high_quartet(val))

#define get_map_cell_value(y,x) (is_odd(x) ?  \
                                       get_low_quartet(map[y][x/2])  \
                                    :  get_high_quartet(map[y][x/2]))


// Caractères utilisés pour la carte, le joueur, et le contour de la fenêtre
#define C_EMPTY ' '    // empty cell = SPACE = (uchar) 32
#define C_WALL  '#'
#define C_TREE  '^'
#define C_WATER  '%'
#define C_HILL1  '/'
#define C_HILL2  '\\'  // Rappel: en C il faut doubler le caractère antislash, qui est sinoninterprété
                     // comme un caractère 'escape' introduisant une séquence de contrôle (ex: '\n')
#define C_PLAYER '*'
#define C_CHECKERBOARD 126 // caractère 'damier'


// Valeurs de chaque type de case, sur 4 bits maxi (donc 16 valeurs max possibles, de 0 à 15)
#define V_EMPTY ((char) 0)
#define V_WALL  ((char) 1)
#define V_TREE  ((char) 2)
#define V_WATER ((char) 3)
#define V_HILL1 ((char) 4)
#define V_HILL2 ((char) 5)

// tableau indexé des caractères C_xxx (indexé par constantes V_xxx)
char c_values[] = { C_EMPTY, C_WALL, C_TREE, C_WATER, C_HILL1, C_HILL2};

#define get_cvalue(v_value) (c_values[v_value])

// pour affecter une double valeur, on passera TOUJOUTS les quartets dans l'ordre (SUPERIEUR, INFERIEUR)
// pour correspondre à l'ordre des cases (1ere case PAIRE? 2e case IMPAIRE)
#define combine_cellvalues(highval, lowval)    ((highval << 4) | lowval)
#define set_cellvalues(addr, highval, lowval)  ((*addr) = combine_cellvalues(highval, lowval))

// Scan codes des touches du clavier
#define KEY_LEFT  172
#define KEY_RIGHT 188
#define KEY_DOWN  180
#define KEY_UP    156
#define KEY_SPACE 132
#define KEY_ESC   169

#define NO_KEY 0x38 // NO KEY PRESSED


/* ================== VARIABLES GLOBALES ================== */

// Représentation de la carte, chaque case est un caractère (char)
// BRANCHE "2_cells_per_byte": on regroupe 2 valeurs en 1 octet => dimension X du tableau = XSIZE/2
// (condition: que XSIZE soit pair... ce qui simplifie aussi grandement tous les calculs)
char map[MAP_YSIZE][MAP_XSIZE/2];

/* adresse contenant la dernière touche pressée du clavier */
uchar *keyb_norm_key_press_addr = (uchar *) 0x208;


/* ================== DECLARATION DES FONCTIONS (PROTOTYPES) ================== */
void  init_map();
void  display_window();
void  play_map();
void  display_title_screen();
void  wait_spacekey();
uchar rnd(uchar max);
uchar get_valid_keypress();
void  test_keys();
void  hide_cursor();
void  show_cursor();


/* ================== IMPLEMENTATION DES FONCTIONS ================== */

/**
 * main(): Point d'entrée du programme
 */  
void main() {
	//test_keys();

    cls(); paper(4); ink(2);

    hide_cursor();

    display_title_screen();
    init_map();

    cls();
    display_window();

    //test_keys();
    play_map();

	// End game: show the cursor and quit
    show_cursor();
}



/** 
 * init_map(): initialisation du tableau représentant la carte 
 */
void init_map() {
    uchar i, j, k, kx, ky, n;
    uchar xmax_inside; // coord x max entre les murs d'enceinte
    uchar ymax_inside; // coord y max entre les murs d'enceinte
    char *cell_addr1, *cell_addr2; // optimisation v1.2


    // ceinturer la carte de murs ('#') et la remplir de blancs (espaces)
    printf("Ajout des murs d'enceinte...\n");

    // Murs horizontaux haut & bas
    cell_addr1 = &map[0][0];
    cell_addr2 = &map[MAP_YSIZE-1][0];
    for(i=0; i < MAP_XSIZE/2; i++) {
        // map[0][i] = WALL;
        *cell_addr1++ = combine_cellvalues(V_WALL, V_WALL);
        //map[MAP_YSIZE-1][i] = WALL;
        *cell_addr2++ = combine_cellvalues(V_WALL, V_WALL);
    }

    // Murs verticaux gauche/droit, avec case vide après/avant
    cell_addr1 = &map[0][0];
    cell_addr2 = &map[0][MAP_XSIZE/2-1];
    for(i=0; i < MAP_YSIZE; i++) {
        //map[i][0] = WALL;
        *cell_addr1 = combine_cellvalues(V_WALL, V_EMPTY);
        cell_addr1 += MAP_XSIZE/2;
        //map[i][MAP_XSIZE-1] = WALL;
        *cell_addr2 = combine_cellvalues(V_EMPTY, V_WALL);
        cell_addr2 += MAP_XSIZE/2;
    }

    // Initialiser l'intérieur de la carte avec des blancs (= cases 'vides')
    printf("Remplissage de la carte de blancs...\n");
    #define YMAX_INSIDE (MAP_YSIZE-1)
    #define XMAX_INSIDE (MAP_XSIZE/2-2)

    cell_addr1 = &map[1][2];
    for(i = 1; i < YMAX_INSIDE; i++) {
        for(j = 1; j < XMAX_INSIDE; j++) {
            //map[i][j] = EMPTY;
            *cell_addr1++ = combine_cellvalues(V_EMPTY, V_EMPTY);
        }
        cell_addr1 += 2; // sauter double case (XMAX-2,XMAX-1) et double case (0,1) de la ligne suivante
    }

    // Ajouter de 200 a 250 arbres
    // (ATTENTION ici on ne peut pas dépasser 255 car n est un "uchar",
    //  et la fonction rnd() elle-même attend un uchar en parametre !!!)
    n = rnd(200)+51; // ne marche pas si #include <lib.h>
    //n = 230;
    printf("Ajout de %d arbres...\n", n);
    for(k = 0; k < n; k++) {
        kx = rnd(MAP_YSIZE-3)+1;
        ky = rnd(MAP_XSIZE-3)+1;
        map[ky][kx/2] = is_even(kx) ? combine_cellvalues(V_TREE, V_EMPTY) : combine_cellvalues(V_EMPTY, V_TREE);
    }
    // Ajouter de 60 a 100 montagnes
   n = rnd(60)+41; // ne marche pas si #include <lib.h>
   //n = 71;
    printf("Ajout de %d montagnes...\n", n);
    for(k = 0; k < n; k++) {
        kx = rnd(MAP_XSIZE-4)+1;
        ky = rnd(MAP_YSIZE-2)+1;
        //map[ky][kx++] = HILL1;
        //map[ky][kx]   = HILL2; 
        cell_addr1 = &map[ky][kx/2];
        *cell_addr1   =  combine_cellvalues(V_HILL1, V_HILL2); 
    }

    // Ajouter de 30 a 50 lacs de 4x3 cases
    n = rnd(30)+21; // ne marche pas si #include <lib.h>
    //n = 41;
    printf("Ajout de %d lacs...\n", n);
    for(k = 0; k < n; k++) {
        kx = rnd(MAP_XSIZE-8)+1;
        ky = rnd(MAP_YSIZE-5)+1;

        // - 1e ligne du lac
        //map[ky][kx] = WATER; map[ky][kx+1] = WATER; map[ky][kx+2] = WATER;
        //ky++; // Incrémmenter ky pour la ligne suivante
        //kx++; // et incrémenter aussi kx pour décaler d'une case à droite
        cell_addr1 = &map[ky][kx/2];
        *cell_addr1++ = combine_cellvalues(V_WATER, V_WATER); 
        *cell_addr1++ = combine_cellvalues(V_WATER, V_WATER);

        // - 2e ligne du lac
        //map[ky][kx] = WATER; map[ky][kx+1] = WATER; map[ky][kx+2] = WATER;
        //ky++; // Incrémmenter ky pour la ligne suivante
        //kx++; // et incrémenter aussi kx pour décaler d'une case à droite
        cell_addr1 += MAP_XSIZE/2; 
        // on est déjà décalé d'une case à droite après passage à la ligne à cause du dernier 'cell_addr1++'
        *cell_addr1-- = combine_cellvalues(V_WATER, V_EMPTY); 
        *cell_addr1-- = combine_cellvalues(V_WATER, V_WATER);
        *cell_addr1++ = combine_cellvalues(V_EMPTY, V_WATER);
        // noter le dernier "cell_addr1++" pour se remettre en décalage d'une cellule à droite

        // - 3e ligne du lac
        //map[ky][kx] = WATER; map[ky][kx+1] = WATER; map[ky][kx+2] = WATER;
        cell_addr1 += MAP_XSIZE/2; 
        *cell_addr1++ = combine_cellvalues(V_WATER, V_WATER);
        *cell_addr1   = combine_cellvalues(V_WATER, V_WATER);; 
    }
}

/** 
 * display_window(): affichage du contour de la fenêtre d'affichage de la carte
 */
void display_window() {
    uchar y, x;
    char *addr = (char *) (TEXT_SCREEN + WY*SCREEN_WIDTH + WX);

    #define WIN_EXT_WIDTH   (WIN_XSIZE + 2)  // largeur totale de la fenêtre avec son cadre
    #define WIN_EXT_HEIGHT  (WIN_YSIZE + 2)  // hauteur totale de la fenêtre avec son cadre

    // trait haut
    for(x=0; x < WIN_EXT_WIDTH; x++) {
        *addr++ = C_CHECKERBOARD; // caractère damier
    }
    addr += (SCREEN_WIDTH - WIN_EXT_WIDTH);
    // traits verticaux gauche et droit
    for(y=1; y < WIN_EXT_HEIGHT-1; y++) {
        *addr = C_CHECKERBOARD; // caractère damier
        addr += WIN_EXT_WIDTH-1;
        *addr = C_CHECKERBOARD; // caractère damier
        addr += (SCREEN_WIDTH - WIN_EXT_WIDTH + 1);
    }
    // trait bas
    for(x=0; x < WIN_EXT_WIDTH; x++) {
        *addr++ = C_CHECKERBOARD; // caractère damier
    }
    // Affichage des instructions sous la fenêtre:
    gotoxy(WX+4, WY+WIN_EXT_HEIGHT);   printf("Deplacements: fleches");
    gotoxy(WX+4, WY+WIN_EXT_HEIGHT+1); printf("    ESC = quitter");
}

/** 
 * play_map(): affichage de la carte et boucle principale de gestion du clavier et des déplacements
 */
void play_map() { 
    uchar x, y;    // coordonnees ABSOLUES du personnage dans la map
    uchar px,py;   // coordonnees RELATIVES du personnage dans la fenêtre d'affichage
    uchar xv, yv;  // coordonnées de départ (offset) de la map pour l'affichage dans la fenêtre
    uchar i, j;
    char *addr;
    // optimisation v1.1: current_cell_addr = adresse case courante du tableau de la carte à afficher,
    //      initialisée à chaque "balayage" avec la coordonnée du coin supérieur gauche 
    //      de la partie visible du tableau de la carte à afficher
    char *current_cell_addr; // optimisation v1.1
    
    #define ADDR_INFOLINE1 (char *) (TEXT_SCREEN + (WY-2)*SCREEN_WIDTH + WX+2)
    #define ADDR_INFOLINE2 (char *) (ADDR_INFOLINE1 + SCREEN_WIDTH)

    uchar key = NO_KEY;
    bool end = FALSE;

    x = 50; y = 50;

    // Boucle principale:
    // - affichage de la partie visible de la carte dans la fenêtre
    // - gestion du clavier pour les déplacements et la 'fin de partie'
    while(!end) {
        // Calcul coordonnées coin supérieur gauche (xv, yv) de la partie de la carte à afficher
        // en fonction des coordonnées (x,y) courantes du 'joueur'
        if(x <= WIN_XSIZE/2) xv = 0; else xv = x - WIN_XSIZE/2;
        if(y <= WIN_YSIZE/2) yv = 0; else yv = y - WIN_YSIZE/2;
        //  Corrections pour affichage extrémités droite et inferieure de la carte
        if(x >= MAP_XSIZE-WIN_XSIZE/2) xv = MAP_XSIZE-WIN_XSIZE;
        if(y >= MAP_YSIZE-WIN_YSIZE/2) yv = MAP_YSIZE-WIN_YSIZE;
        // Affichage de la partie de la partie visible de la carte dans la fenêtre
        addr = (char *) (TEXT_SCREEN + (WY+1)*SCREEN_WIDTH + WX+1);
        current_cell_addr = &map[yv][xv/2]; // optimisation v1.1
        for(i=yv; i < (yv+WIN_YSIZE); i++) {
            for(j=xv; j < (xv+WIN_XSIZE); j++) {
                // *addr++ = map[i][j];
                
                /*
                gotoxy(WX, WY+WIN_EXT_HEIGHT+3);
                printf("i/y=%d, j/x=%d ", i, j);
                gotoxy(WX, WY+WIN_EXT_HEIGHT+4);
                printf("cell addr=%x, val=%x, char=%c", 
                               current_cell_addr, 
                               *current_cell_addr,
                               get_cvalue(get_cellvalue(j, *current_cell_addr)));
                wait_spacekey();
                */
               
                *addr++ = get_cvalue(get_cellvalue(j, *current_cell_addr));
                if(is_even(j)) *current_cell_addr++; // incrémenter cellule tableau uniquement après case impaire

            }
            addr += (SCREEN_WIDTH - WIN_XSIZE);
            current_cell_addr += (MAP_YSIZE - WIN_XSIZE/2); // optimisation v1.1
        }
        // Affichage personnage: PX et PY sont les coordonnees relatives
        if(x > WIN_XSIZE/2) px = x - xv; else px = x;
        if(y > WIN_YSIZE/2) py = y - yv; else py = y;
        addr = (char *) (TEXT_SCREEN + (WY+1+py)*SCREEN_WIDTH + WX+1+px);
        *addr = C_PLAYER;

        // affichage infos coordonnées courantes du 'joueur'
        sprintf(ADDR_INFOLINE1, "X=%d, PX=%d, Y=%d, PY=%d    ", x, px, y, py);
        sprintf(ADDR_INFOLINE2, "XV=%d, YV=%d  ", xv, yv);

        // Gestion du clavier pour les déplacements et la 'fin de partie'
        key = get_valid_keypress();
        switch(key) {
            case KEY_ESC:
                end = TRUE;
                break;
            case KEY_LEFT:
                if(x > 0 && get_map_cell_value(y, x-1) == V_EMPTY) x--;
                break;
            case KEY_RIGHT:
                if(x < (MAP_XSIZE-1) && get_map_cell_value(y, x+1) == V_EMPTY) x++;
                break;
            case KEY_UP:
                if(y > 0 && get_map_cell_value(y-1, x) == V_EMPTY) y--;
                break;
            case KEY_DOWN:
                if(y < (MAP_YSIZE-1) && get_map_cell_value(y+1, x) == V_EMPTY) y++;
                break;
        }
    }

}

/** 
 * display_title_screen(): affichage d'un écran titre
 */
void display_title_screen() {
	text();
	cls(); paper(2); ink(4);

	printf("\n\n\n\n\n");    
	printf("  Demo carte deroulante en C\n");
    printf("  --------------------------\n");
    printf("\n\n\n");
    printf("Initialisation en cours...\n");
}

/** 
 * Attend une pression sur la touche ESPACE 
 */
void wait_spacekey() {
    uchar key = NO_KEY;
    // wait for space key pressed
	for(; key != KEY_SPACE;) {
		key = *keyb_norm_key_press_addr;
	}
    // wait for key release
	for(; key != NO_KEY;) {
		key = *keyb_norm_key_press_addr;
	}
}

/** 
 * rnd(uchar max) : renvoie un nombre aléatoire (entier positif) dans l'intervalle  [0...max[
 *  arg max: borne supérieure délimitant l'intervalle des nombres aléatoires générés
 *           (type unsigned char ==> valeur maxi : 255)
 */
uchar rnd(uchar max) {
    // ATTENTION - ne PAS inclure <lib.h> sinon ça fait bugger la définition de rand()
    // qui renvoie systématiquement zéro !!
    uchar val = (uchar) (rand()/(32768/max));
    //printf("rnd(%d)=%d -- press space  \n", max, val); wait_spacekey();
    return val;
}

/** 
 * get_valid_keypress(): scanne le clavier jusqu'à ce qu'une touche valide soit
 * pressée, et renvoie le code correspondant (type uchar)
 */
uchar get_valid_keypress() {
	uchar key = NO_KEY;
	uchar debounce;
    bool valid = FALSE;

	do {
		key = *keyb_norm_key_press_addr;
		valid = (	(key == KEY_LEFT)
				 || (key == KEY_RIGHT)
				 || (key == KEY_UP)
				 || (key == KEY_DOWN)
                 || (key == KEY_ESC)
                );
	} while(!valid);
	// debounce: wait until key is released
    // wait for key release
	//debounce = key;
	//for(; debounce != NO_KEY;) {
	//	debounce = *keyb_norm_key_press_addr;
    //	}
	// finally, return code of valid key pressed
	return key;
}



// ======================================================================
// Test Keys: display scan code of pressed key (press SPACE to exit test)
// ======================================================================
void test_keys() {
    unsigned char *keyb_norm_key_press_addr = (unsigned char *) 0x208;
    unsigned char key      = NO_KEY; // 0x38 = NO KEY PRESSED
    unsigned char prev_key = NO_KEY; // 0x38 = NO KEY PRESSED
    
    printf("Keypress test -- press SPACE to end\n");

    for(; key != KEY_SPACE;) { // loop until SPACE is pressed to quit
        key = *keyb_norm_key_press_addr;
        if(key != NO_KEY) {	// 0x38 = NO KEY PRESSED
            if(key != prev_key) {
                // Do not log repeat key presses
                printf("Key pressed: dec=%d, hex=0x%x\n", key, key);
                prev_key = key;
            }
        }
    }  
}

/**
 * hide_cursor(): cache le curseur 
 */ 
void hide_cursor() {
	// Hide the cursor: clear bit 0 @ 0x26A
	uchar *addr = (uchar *) 0x26A;
	*addr &= 0xFE;
}

/**
 * hide_cursor(): montre le curseur 
 */ 
void show_cursor() {
	// End game: show cursor: set bit 0 @ 0x26A
	uchar *addr = (uchar *) 0x26A;
	*addr |= 1;
}

