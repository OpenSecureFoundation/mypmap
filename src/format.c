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

static int entry_in_range(const MapEntry *e, const Options *opts)
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

/* ================================================
 * Mode -XX : absolument tous les champs smaps
 *
 * Colonnes (abréviations dans l'en-tête) :
 *   KPS = KernelPageSize   MPS = MMUPageSize
 *   PssDty = Pss_Dirty     SC/SD = Shared_Clean/Dirty
 *   PC/PD = Private_Clean/Dirty  LzFr = LazyFree
 *   AnonHP = AnonHugePages ShmPM = ShmemPmdMapped
 *   FilPM = FilePmdMapped  ShHtlb = Shared_Hugetlb
 *   PvHtlb = Private_Hugetlb    THP = THPeligible
 * ================================================ */

void print_XXX(const ProcInfo *info, const MapList *list,
               const Options *opts)
{
    if (!opts->quiet)
    {
        printf("%d:   %s\n", info->pid,
               info->cmdline[0] ? info->cmdline : info->name);

        printf("%-16s %-4s %8s %5s %8s "
               "%6s %4s %4s %6s %6s %6s "
               "%12s %12s %13s %13s "
               "%10s %9s %6s %8s "
               "%13s %14s %13s %14s %15s "
               "%6s %7s %6s %3s  %-24s  %s\n",
               "Address", "Perm", "Offset", "Dev", "Inode",
               "Size", "KPS", "MPS", "Rss", "Pss", "PssDty",
               "SC", "SD", "PC", "PD",
               "Referenced", "Anonymous", "KSM", "LzFr",
               "AnonHP", "ShmPM", "FilPM", "ShHtlb", "PvHtlb",
               "Swap", "SwapPss", "Locked", "THP", "VmFlags", "Mapping");
    }

    /* Totaux cumulés sur toutes les entrées visibles */
    unsigned long t_size = 0, t_kps  = 0, t_mps  = 0;
    unsigned long t_rss  = 0, t_pss  = 0, t_pssd = 0;
    unsigned long t_sc   = 0, t_sd   = 0, t_pc   = 0, t_pd   = 0;
    unsigned long t_ref  = 0, t_anon = 0, t_ksm  = 0, t_lz   = 0;
    unsigned long t_ahp  = 0, t_spmd = 0, t_fpmd = 0;
    unsigned long t_shtl = 0, t_pvtl = 0;
    unsigned long t_swap = 0, t_spss = 0, t_lock = 0;

    for (size_t i = 0; i < list->count; i++)
    {
        const MapEntry *e = &list->entries[i];
        if (!entry_in_range(e, opts))
            continue;

        printf("%016lx %-4s %08lx %02x:%02x %8lu "
               "%6lu %4lu %4lu %6lu %6lu %6lu "
               "%12lu %12lu %13lu %13lu "
               "%10lu %9lu %6lu %8lu "
               "%13lu %14lu %13lu %14lu %15lu "
               "%6lu %7lu %6lu %3d  %-24s  %s\n",
               e->addr_start, e->perms, e->offset,
               e->dev_major, e->dev_minor, e->inode,
               e->size_kb, e->kernel_page_size_kb, e->mmu_page_size_kb,
               e->rss_kb, e->pss_kb, e->pss_dirty_kb,
               e->shared_clean_kb, e->shared_dirty_kb,
               e->private_clean_kb, e->private_dirty_kb,
               e->referenced_kb, e->anonymous_kb,
               e->ksm_kb, e->lazy_free_kb,
               e->anon_huge_pages_kb, e->shmem_pmd_mapped_kb,
               e->file_pmd_mapped_kb, e->shared_hugetlb_kb, e->private_hugetlb_kb,
               e->swap_kb, e->swap_pss_kb, e->locked_kb,
               e->thp_eligible,
               e->vm_flags[0] ? e->vm_flags : "-",
               get_mapping_name(e, opts));

        t_size += e->size_kb;           t_kps  += e->kernel_page_size_kb;
        t_mps  += e->mmu_page_size_kb;  t_rss  += e->rss_kb;
        t_pss  += e->pss_kb;            t_pssd += e->pss_dirty_kb;
        t_sc   += e->shared_clean_kb;   t_sd   += e->shared_dirty_kb;
        t_pc   += e->private_clean_kb;  t_pd   += e->private_dirty_kb;
        t_ref  += e->referenced_kb;     t_anon += e->anonymous_kb;
        t_ksm  += e->ksm_kb;            t_lz   += e->lazy_free_kb;
        t_ahp  += e->anon_huge_pages_kb; t_spmd += e->shmem_pmd_mapped_kb;
        t_fpmd += e->file_pmd_mapped_kb; t_shtl += e->shared_hugetlb_kb;
        t_pvtl += e->private_hugetlb_kb; t_swap += e->swap_kb;
        t_spss += e->swap_pss_kb;        t_lock += e->locked_kb;
    }

    if (!opts->quiet)
    {
        /* "total kB" (8 chars) est aligné sous la colonne Address (46 chars
         * = 16 + 1 + 4 + 1 + 8 + 1 + 5 + 1 + 8 + 1) ; on comble l'écart
         * avec %-46s pour que Size tombe sur la bonne position. */
        printf("%-46s"
               "%6lu %4lu %4lu %6lu %6lu %6lu "
               "%12lu %12lu %13lu %13lu "
               "%10lu %9lu %6lu %8lu "
               "%13lu %14lu %13lu %14lu %15lu "
               "%6lu %7lu %6lu\n",
               "total kB",
               t_size, t_kps, t_mps,
               t_rss, t_pss, t_pssd,
               t_sc, t_sd, t_pc, t_pd,
               t_ref, t_anon, t_ksm, t_lz,
               t_ahp, t_spmd, t_fpmd, t_shtl, t_pvtl,
               t_swap, t_spss, t_lock);
    }
}

/* =======================
 * Dispatcher principal
 * ======================== */

void print_output(const ProcInfo *info, const MapList *list,
                  const Options *opts)
{
    if (opts->show_very_very_extended)
        print_XXX(info, list, opts);
    else if (opts->show_very_extended)
        print_XX(info, list, opts);
    else if (opts->show_extended)
        print_extended(info, list, opts);
    else if (opts->show_device)
        print_device(info, list, opts);
    else
        print_standard(info, list, opts);
}
