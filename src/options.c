#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        "Autres :\n"
        "  -V, --version          Affiche la version et quitte\n"
        "  -h, --help             Affiche cette aide et quitte\n"
        "\n"
        "Exemples :\n"
        "  %s 1234\n"
        "  %s -x 1234 5678\n"
        "  %s -XX -q 1234\n",
        prog, prog, prog, prog);
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

int parse_options(int argc, char *argv[], Options *opts)

{
    memset(opts, 0, sizeof(*opts));

    int c;
    while ((c = getopt_long(argc, argv, "xXdqpA:Vh", long_opts, NULL)) != -1)
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
    if(opts) free(opts->pids);
}