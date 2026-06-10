#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include "options.h"

static const struct option long_opts[] =
{
    {"extended",       no_argument,       NULL, 'x'},
    {"very-extended",  no_argument,       NULL, 'X'},
    {"device",         no_argument,       NULL, 'd'},
    {"quiet",          no_argument,       NULL, 'q'},
    {"show-path",      no_argument,       NULL, 'p'},
    {"range",          required_argument, NULL, 'A'},
    {"read-rc",        no_argument,       NULL, 'c'},
    {"read-rc-from",   required_argument, NULL, 'C'},
    {"version",        no_argument,       NULL, 'V'},
    {"help",           no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
};

/* Affiche le message d'usage sur le flux donné (stdout pour -h, stderr pour les erreurs). */
static void usage(const char *prog, FILE *flux)
{
    fprintf(flux,
        "Usage: %s [OPTIONS] PID...\n"
        "\n"
        "Affiche la carte mémoire d'un ou plusieurs processus (réimplémentation de pmap).\n"
        "\n"
        "Options d'affichage :\n"
        "  -x, --extended         Affiche RSS et Dirty (lecture de /proc/PID/smaps)\n"
        "  -X, --very-extended    Affiche tous les champs smaps de base\n"
        "  -XX                    Affiche TOUS les champs smaps fournis par le noyau\n"
        "                         (KernelPageSize, AnonHugePages, VmFlags, etc.)\n"
        "  -d, --device           Affiche offset, numéro de device et inode\n"
        "\n"
        "Options de filtrage :\n"
        "  -q, --quiet            Supprime les en-têtes et les totaux\n"
        "  -p, --show-path        Affiche le chemin complet des mappings fichier\n"
        "  -A lo,hi               Restreint l'affichage à la plage d'adresses [lo, hi]\n"
        "                         (valeurs hexadécimales, ex: -A 7f000000,7fffffff)\n"
        "\n"
        "Options de configuration :\n"
        "  -c, --read-rc          Lit la configuration des colonnes depuis ~/.pmaprc\n"
        "                         (une colonne smaps par ligne, ex: Rss, Pss, Swap...)\n"
        "                         Active automatiquement le mode -X si non spécifié\n"
        "  -C, --read-rc-from <f> Identique à -c mais lit le fichier <f> à la place\n"
        "\n"
        "Autres :\n"
        "  -V, --version          Affiche la version et quitte\n"
        "  -h, --help             Affiche cette aide et quitte\n"
        "\n"
        "Exemples :\n"
        "  %s 1234\n"
        "  %s -x 1234 5678\n"
        "  %s -XX -q 1234\n"
        "  %s -c -X 1234\n"
        "  %s -C /tmp/ma_config.rc 1234\n",
        prog, prog, prog, prog, prog, prog);
}

static int parse_range(const char *arg, Options *opts)
{
    char *comma = strchr(arg, ',');
    if(!comma) return -1;
    *comma = '\0';
    opts->addr_lo = strtoul(arg,    NULL,16);
    opts->addr_hi = strtoul(comma + 1,  NULL, 16);
    *comma = ',';

    if (opts->addr_lo >= opts->addr_hi) return -1;
    opts->filter_addr = 1;
    return 0;
}

/* Lit un fichier rc et remplit rc->noms[]/rc->nb.
 * Ignore les lignes vides et les commentaires (lignes commençant par #).
 * Retourne 0 si le fichier a été lu, -1 s'il est inaccessible. */
int lire_fichier_rc(const char *chemin, RcConfig *rc)
{
    FILE *f = fopen(chemin, "r");
    if (!f) {
        fprintf(stderr, "mypmap: impossible d'ouvrir le fichier rc '%s' : %s\n",
                chemin, strerror(errno));
        return -1;
    }

    rc->nb = 0;
    char ligne[256];

    while (fgets(ligne, sizeof(ligne), f)) {
        /* Supprimer saut de ligne et espaces finaux */
        size_t len = strlen(ligne);
        while (len > 0 && (ligne[len - 1] == '\n' || ligne[len - 1] == '\r'
                           || ligne[len - 1] == ' ' || ligne[len - 1] == '\t'))
            ligne[--len] = '\0';

        /* Avancer jusqu'au premier caractère non-blanc */
        const char *debut = ligne;
        while (*debut == ' ' || *debut == '\t') debut++;

        /* Ignorer les lignes vides et les commentaires */
        if (*debut == '\0' || *debut == '#') continue;

        if (rc->nb >= RC_MAX_COLONNES) {
            fprintf(stderr,
                    "mypmap: fichier rc '%s' : trop de colonnes (max %d), "
                    "les suivantes sont ignorées\n", chemin, RC_MAX_COLONNES);
            break;
        }

        strncpy(rc->noms[rc->nb], debut, RC_NOM_MAX - 1);
        rc->noms[rc->nb][RC_NOM_MAX - 1] = '\0';
        rc->nb++;
    }

    fclose(f);
    return 0;
}

int parse_options(int argc, char *argv[], Options *opts)

{
    memset(opts, 0, sizeof(*opts));

    int c;
    while ((c = getopt_long(argc, argv, "xXdqpA:cC:Vh", long_opts, NULL)) != -1)
    {
        switch (c)
        {
            case 'x':
                opts->show_extended = 1;
                break;
            case 'X':
                /* Deux -X consécutifs (ou -XX) activent le mode très étendu. */
                if (opts->show_very_extended)
                    opts->show_very_very_extended = 1;
                else
                    opts->show_very_extended = 1;
                break;
            case 'd': opts->show_device = 1; break;
            case 'q': opts->quiet      = 1; break;
            case 'p': opts->show_path  = 1; break;
            case 'A':
                if (parse_range(optarg, opts) != 0)
                {
                    fprintf(stderr, "mypmap: -A invalide (format: lo,hi en hex)\n");
                    return -1;
                }
                break;
            case 'c':
                opts->use_rc = 1;
                break;
            case 'C':
                opts->rc_path = strdup(optarg);
                if (!opts->rc_path) {
                    fprintf(stderr, "mypmap: mémoire insuffisante\n");
                    return -1;
                }
                break;
            case 'V':
                printf("mypmap 1.0.0\n");
                exit(EXIT_SUCCESS);
            case 'h':
                usage(argv[0], stdout);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0], stderr);
                return -1;
        }
    }

    /* Charger le fichier rc si -c ou -C a été fourni */
    if (opts->use_rc || opts->rc_path) {
        const char *chemin = opts->rc_path;
        char chemin_defaut[512];

        /* -c sans -C : construire le chemin ~/.pmaprc */
        if (!chemin) {
            const char *home = getenv("HOME");
            if (!home) {
                fprintf(stderr,
                        "mypmap: -c : variable HOME non définie, "
                        "impossible de localiser ~/.pmaprc\n");
            } else {
                int n = snprintf(chemin_defaut, sizeof(chemin_defaut),
                                 "%s/.pmaprc", home);
                if (n > 0 && (size_t)n < sizeof(chemin_defaut))
                    chemin = chemin_defaut;
            }
        }

        if (chemin)
            lire_fichier_rc(chemin, &opts->rc_config);

        /* Si des colonnes ont été chargées et qu'aucun mode étendu
         * n'est actif, activer -X pour que le fichier rc ait un effet. */
        if (opts->rc_config.nb > 0
            && !opts->show_very_extended
            && !opts->show_very_very_extended)
            opts->show_very_extended = 1;
    }

    opts->pid_count = argc - optind;
    if (opts->pid_count <= 0)
    {
        fprintf(stderr, "mypmap: aucun PID spécifié\n");
        usage(argv[0], stderr);
        return -1;
    }

    opts->pids = malloc((size_t)opts->pid_count * sizeof(int));
    if (!opts->pids) 
    return -1;

    for (int i = 0; i < opts->pid_count; i++)
    {
        char *end;
        long v = strtol(argv[optind + i], &end, 10);
        if (*end != '\0' || v<=0)
        {
            fprintf(stderr , "mypmap: PID invalide '%s'\n" , argv[optind + i]);
            free(opts->pids);
            opts->pids = NULL;
            return -1;
        }
        opts->pids[i] = (int)v;
    }
    return 0;
}
void free_options(Options *opts)
{
    if (opts) {
        free(opts->pids);
        free(opts->rc_path);
    }
}