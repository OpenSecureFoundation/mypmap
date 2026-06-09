#include <stdio.h>
#include <stdlib.h>
#include "../include/procfs.h"
#include "options.h"
#include "format.h"

int main (int argc, char *argv[])
{
    Options opts;
    int ret = EXIT_SUCCESS;

    if (parse_options(argc, argv, &opts) != 0)
        return EXIT_FAILURE;

    for (int i = 0; i < opts.pid_count; i++)
    {
        int pid = opts.pids[i];
        ProcInfo info;
        MapList *maps;

        maps = read_maps(pid);
        if (!maps || read_proc_info(pid, &info) != 0)
        {
            fprintf(stderr, "mypmap: impossible de lire le PID %d\n", pid);
            if (maps) maplist_free(maps);
            ret = EXIT_FAILURE;
            continue;
        }

        if (opts.show_extended || opts.show_very_extended || opts.show_very_very_extended)
            read_smaps(pid, maps);
        
        print_output(&info, maps, &opts);
        maplist_free(maps);

        if (i < opts.pid_count - 1)
            putchar('\n');
    }

    free_options(&opts);
    return ret;
}