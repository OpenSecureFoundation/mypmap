#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "format.h"
#include "procfs.h"
#include "options.h"

/* ======================
 * Utilitaires internes
 * ====================== */

static const char *get_mapping_name(const MapEntry *e, const Options *opts)
{
    (void)opts;
    if (e->pathname[0] != '\0' && e->pathname[0] != ' ')
        return e->pathname;
    return "[ anon ]";
}

static unsigned long entry_kbytes(const MapEntry *e)
{
    return e->size_kb;
}

int entry_in_range(const MapEntry *e, const Options *opts)
{
    if (!opts->filter_addr)
        return 1;
    return (e->addr_start >= opts->addr_lo &&
            e->addr_end <= opts->addr_hi);
}

/* =================
 * Mode par défaut
 * ================= */

void print_standard(const ProcInfo *info, const MapList *list,
                    const Options *opts)
{
    if (!opts->quiet)
        printf("%d:   %s\n", info->pid, info->cmdline[0] ? info->cmdline : info->name);

    unsigned long total_kb = 0;

    for (size_t i = 0; i < list->count; i++)
    {
        const MapEntry *e = &list->entries[i];
        if (!entry_in_range(e, opts))
            continue;

        unsigned long kb = entry_kbytes(e);
        total_kb += kb;

        printf("%016lx %6luK %-5s %s\n",
               e->addr_start,
               kb,
               e->perms,
               get_mapping_name(e, opts));
    }

    if (!opts->quiet)
        printf(" total %16luK\n", total_kb);
}

/* ==================
 * Mode étendu -x
 * ================== */

void print_extended(const ProcInfo *info, const MapList *list,
                    const Options *opts)
{
    if (!opts->quiet)
    {
        printf("%d:   %s\n", info->pid,
               info->cmdline[0] ? info->cmdline : info->name);
        printf("%-16s %7s %7s %7s %-5s %s\n",
               "Address", "Kbytes", "RSS", "Dirty", "Mode", "Mapping");
    }

    unsigned long total_kb = 0;
    unsigned long total_rss = 0;
    unsigned long total_dirty = 0;

    for (size_t i = 0; i < list->count; i++)
    {
        const MapEntry *e = &list->entries[i];
        if (!entry_in_range(e, opts))
            continue;

        unsigned long kb = entry_kbytes(e);
        total_kb += kb;
        total_rss += e->rss_kb;
        total_dirty += e->private_dirty_kb;

        printf("%016lx %7lu %7lu %7lu %-5s %s\n",
               e->addr_start,
               kb,
               e->rss_kb,
               e->private_dirty_kb,
               e->perms,
               get_mapping_name(e, opts));
    }

    if (!opts->quiet)
        printf("%-16s %7lu %7lu %7lu\n",
               "total kB", total_kb, total_rss, total_dirty);
}

/* ==========
 * Mode -X
 * ==========*/

void print_XX(const ProcInfo *info, const MapList *list,
              const Options *opts)
{
    if (!opts->quiet)
    {
        printf("%d:   %s\n", info->pid,
               info->cmdline[0] ? info->cmdline : info->name);

        printf("%-16s %-4s %8s %5s %8s "
               "%7s %7s %7s "
               "%12s %12s %13s %13s "
               "%10s %9s %7s %7s %6s  %s\n",
               "Address", "Perm", "Offset", "Dev", "Inode",
               "Size", "RSS", "PSS",
               "Shared_Clean", "Shared_Dirty", "Private_Clean", "Private_Dirty",
               "Referenced", "Anonymous", "Swap", "SwapPss", "Locked",
               "Mapping");
    }

    unsigned long t_size = 0, t_rss = 0, t_pss = 0;
    unsigned long t_sc = 0, t_sd = 0, t_pc = 0, t_pd = 0;
    unsigned long t_ref = 0, t_anon = 0, t_swap = 0, t_spss = 0, t_lock = 0;

    for (size_t i = 0; i < list->count; i++)
    {
        const MapEntry *e = &list->entries[i];
        if (!entry_in_range(e, opts))
            continue;

        printf("%016lx %-4s %08lx %02x:%02x %8lu "
               "%7lu %7lu %7lu "
               "%12lu %12lu %13lu %13lu "
               "%10lu %9lu %7lu %7lu %6lu  %s\n",
               e->addr_start,
               e->perms,
               e->offset,
               e->dev_major, e->dev_minor,
               e->inode,
               e->size_kb, e->rss_kb, e->pss_kb,
               e->shared_clean_kb, e->shared_dirty_kb,
               e->private_clean_kb, e->private_dirty_kb,
               e->referenced_kb, e->anonymous_kb,
               e->swap_kb, e->swap_pss_kb, e->locked_kb,
               get_mapping_name(e, opts));

        t_size += e->size_kb;
        t_rss += e->rss_kb;
        t_pss += e->pss_kb;
        t_sc += e->shared_clean_kb;
        t_sd += e->shared_dirty_kb;
        t_pc += e->private_clean_kb;
        t_pd += e->private_dirty_kb;
        t_ref += e->referenced_kb;
        t_anon += e->anonymous_kb;
        t_swap += e->swap_kb;
        t_spss += e->swap_pss_kb;
        t_lock += e->locked_kb;
    }

    if (!opts->quiet)
    {
        printf("%-21s "
               "%7lu %7lu %7lu "
               "%12lu %12lu %13lu %13lu "
               "%10lu %9lu %7lu %7lu %6lu\n",
               "total kB",
               t_size, t_rss, t_pss,
               t_sc, t_sd, t_pc, t_pd,
               t_ref, t_anon, t_swap, t_spss, t_lock);
    }
}

/* ==================
 * Mode device -d
 * ==================*/

void print_device(const ProcInfo *info, const MapList *list,
                  const Options *opts)
{
    if (!opts->quiet)
    {
        printf("%d:   %s\n", info->pid,
               info->cmdline[0] ? info->cmdline : info->name);
        printf("%-16s %7s %-5s %-11s %s\n",
               "Address", "Kbytes", "Mode", "Device", "Mapping");
    }

    unsigned long total_kb = 0;
    unsigned long total_writable_private = 0;

    for (size_t i = 0; i < list->count; i++)
    {
        const MapEntry *e = &list->entries[i];
        if (!entry_in_range(e, opts))
            continue;

        unsigned long kb = entry_kbytes(e);
        total_kb += kb;

        if (e->perms[1] == 'w' && e->perms[3] == 'p')
            total_writable_private += kb;

        printf("%016lx %7luK %-5s %03x:%05x %s\n",
               e->addr_start,
               kb,
               e->perms,
               e->dev_major,
               e->dev_minor,
               get_mapping_name(e, opts));
    }

    if (!opts->quiet)
        printf("mapped: %luK    writeable/private: %luK    shared: 0K\n",
               total_kb, total_writable_private);
}

/* =======================
 * Dispatcher principal
 * ======================== */

void print_output(const ProcInfo *info, const MapList *list,
                  const Options *opts)
{
    if (opts->show_very_extended)
        print_XX(info, list, opts);
    else if (opts->show_extended)
        print_extended(info, list, opts);
    else if (opts->show_device)
        print_device(info, list, opts);
    else
        print_standard(info, list, opts);
}
