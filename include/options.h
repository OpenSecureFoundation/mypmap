#ifndef OPTIONS_H
#define OPTIONS_H

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
}
Options;

int parse_options(int argc, char *argv[], Options *opts);
void free_options(Options *opts);

#endif