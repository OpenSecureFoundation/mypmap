#ifndef OPTIONS_H
#define OPTIONS_H

/* Nombre maximum de colonnes acceptées dans un fichier rc */
#define RC_MAX_COLONNES 64
/* Longueur maximale d'un nom de champ smaps dans le fichier rc */
#define RC_NOM_MAX      48

/* Colonnes sélectionnées via un fichier de configuration rc (-c / -C) */
typedef struct {
    char noms[RC_MAX_COLONNES][RC_NOM_MAX]; /* noms de champs smaps, dans l'ordre souhaité */
    int  nb;                                 /* nombre de colonnes valides chargées          */
} RcConfig;

typedef struct
{
    int show_extended;           /* -x  : RSS + Dirty via smaps          */
    int show_very_extended;      /* -X  : tous les champs smaps de base   */
    int show_very_very_extended; /* -XX : absolument tous les champs smaps */
    int show_device;             /* -d  : offset, device, inode           */
    int quiet;                   /* -q  : sans en-tête                    */
    int show_path;               /* -p  : chemin complet                  */
    int filter_addr;             /* -A  : filtre plage d'adresses         */
    unsigned long addr_lo, addr_hi;
    int *pids;
    int pid_count;

    /* Options de configuration des colonnes */
    int   use_rc;       /* -c : lire ~/.pmaprc                       */
    char *rc_path;      /* -C : lire le fichier rc à ce chemin       */
    RcConfig rc_config; /* colonnes chargées depuis le fichier rc    */
}
Options;

int  parse_options(int argc, char *argv[], Options *opts);
void free_options(Options *opts);
int  lire_fichier_rc(const char *chemin, RcConfig *rc);

#endif