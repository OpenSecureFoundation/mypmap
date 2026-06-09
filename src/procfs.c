/* procfs.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/procfs.h"
#include <unistd.h>

MapList* maplist_create(void) {
    MapList *list = malloc(sizeof(MapList));
    if (!list) return NULL;
    list->capacity = 16;
    list->count = 0;
    list->entries = malloc(list->capacity * sizeof(MapEntry));
    if (!list->entries) { free(list); return NULL; }
    return list;
}

void maplist_free(MapList *list) {
    if (list) {
        free(list->entries);
        free(list);
    }
}

int read_proc_info(pid_t pid, ProcInfo *info)
{
    char  chemin[256];
    FILE *f;
    info->pid = pid;
    info->name[0] = '\0';
    info->cmdline[0] = '\0';

    snprintf(chemin, sizeof(chemin), "/proc/%d/status", pid);
    f = fopen(chemin, "r");
    if (!f) return -1;

    char ligne[256];
    while (fgets(ligne, sizeof(ligne), f)) {
        if (sscanf(ligne, "Name: %255s", info->name) == 1) {
            break;
        }
    }
    fclose(f);

    snprintf(chemin, sizeof(chemin), "/proc/%d/cmdline", pid);
    f = fopen(chemin, "r");
    if (f) {
        int len = fread(info->cmdline, 1, 1023, f);
        for (int i = 0; i < len; i++)
            if (info->cmdline[i] == '\0') info->cmdline[i] = ' ';
        info->cmdline[len] = '\0';
        fclose(f);
    }
    return 0;
}

int parse_maps_line(const char *ligne, MapEntry *entree)
{
    memset(entree, 0, sizeof(MapEntry));
    int res = sscanf(ligne, "%lx-%lx %5s %lx %x:%x %lu %255s",
        &entree->addr_start, &entree->addr_end, entree->perms,
        &entree->offset, &entree->dev_major, &entree->dev_minor,
        &entree->inode, entree->pathname);
    
    if (res < 7) return -1;
    entree->size_kb = (entree->addr_end - entree->addr_start) / 1024;
    return 0;
}

int append_entry(MapList *list, const MapEntry *entree)
{
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        MapEntry *tmp = realloc(list->entries, new_cap * sizeof(MapEntry));
        if (!tmp) return -1;
        list->entries = tmp;
        list->capacity = new_cap;
    }
    list->entries[list->count++] = *entree;
    return 0;
}

MapList* read_maps(pid_t pid)
{
    char chemin[256];
    sprintf(chemin, "/proc/%d/maps", pid);
    FILE *f = fopen(chemin, "r");
    if (!f) return NULL;

    MapList *list = maplist_create();
    if (!list) { fclose(f); return NULL; }

    char ligne[512];
    MapEntry entree;
    while (fgets(ligne, sizeof(ligne), f)) {
        if (parse_maps_line(ligne, &entree) == 0) {
            append_entry(list, &entree);
        }
    }
    fclose(f);
    return list;
}

int read_smaps(pid_t pid, MapList *list) {
    char chemin[256];
    sprintf(chemin, "/proc/%d/smaps", pid);
    FILE *f = fopen(chemin, "r");
    if (!f) return -1;

    char   ligne[512];
    size_t idx  = 0;

    while (fgets(ligne, sizeof(ligne), f) && idx < list->count) {
        unsigned long val  = 0;
        int           ival = 0;

        /* Chaque segment smaps commence par la ligne "Size:" */
        if (strstr(ligne, "Size:") == ligne) idx++;
        if (idx == 0) continue;

        MapEntry *e = &list->entries[idx - 1];

        /* --- Champs de base (-x, -X) --- */
        if      (sscanf(ligne, "Rss: %lu",           &val) == 1) e->rss_kb           = val;
        else if (sscanf(ligne, "Pss: %lu",           &val) == 1) e->pss_kb           = val;
        else if (sscanf(ligne, "Shared_Clean: %lu",  &val) == 1) e->shared_clean_kb  = val;
        else if (sscanf(ligne, "Shared_Dirty: %lu",  &val) == 1) e->shared_dirty_kb  = val;
        else if (sscanf(ligne, "Private_Clean: %lu", &val) == 1) e->private_clean_kb = val;
        else if (sscanf(ligne, "Private_Dirty: %lu", &val) == 1) e->private_dirty_kb = val;
        else if (sscanf(ligne, "Referenced: %lu",    &val) == 1) e->referenced_kb    = val;
        else if (sscanf(ligne, "Anonymous: %lu",     &val) == 1) e->anonymous_kb     = val;
        else if (sscanf(ligne, "Swap: %lu",          &val) == 1) e->swap_kb          = val;
        else if (sscanf(ligne, "SwapPss: %lu",       &val) == 1) e->swap_pss_kb      = val;
        else if (sscanf(ligne, "Locked: %lu",        &val) == 1) e->locked_kb        = val;

        /* --- Champs très étendus (-XX) --- */
        else if (sscanf(ligne, "KernelPageSize: %lu",  &val)  == 1) e->kernel_page_size_kb  = val;
        else if (sscanf(ligne, "MMUPageSize: %lu",     &val)  == 1) e->mmu_page_size_kb     = val;
        else if (sscanf(ligne, "Pss_Dirty: %lu",       &val)  == 1) e->pss_dirty_kb         = val;
        else if (sscanf(ligne, "LazyFree: %lu",        &val)  == 1) e->lazy_free_kb         = val;
        else if (sscanf(ligne, "AnonHugePages: %lu",   &val)  == 1) e->anon_huge_pages_kb   = val;
        else if (sscanf(ligne, "ShmemPmdMapped: %lu",  &val)  == 1) e->shmem_pmd_mapped_kb  = val;
        else if (sscanf(ligne, "FilePmdMapped: %lu",   &val)  == 1) e->file_pmd_mapped_kb   = val;
        else if (sscanf(ligne, "Shared_Hugetlb: %lu",  &val)  == 1) e->shared_hugetlb_kb    = val;
        else if (sscanf(ligne, "Private_Hugetlb: %lu", &val)  == 1) e->private_hugetlb_kb   = val;
        else if (sscanf(ligne, "KSM: %lu",             &val)  == 1) e->ksm_kb               = val;
        else if (sscanf(ligne, "THPeligible: %d",      &ival) == 1) e->thp_eligible         = ival;
        else if (strncmp(ligne, "VmFlags:", 8) == 0) {
            /* Extraire la liste de flags : tout ce qui suit "VmFlags: " */
            const char *flags = ligne + 8;
            while (*flags == ' ') flags++;
            strncpy(e->vm_flags, flags, sizeof(e->vm_flags) - 1);
            e->vm_flags[sizeof(e->vm_flags) - 1] = '\0';
            /* Supprimer le saut de ligne final */
            size_t len = strlen(e->vm_flags);
            if (len > 0 && e->vm_flags[len - 1] == '\n')
                e->vm_flags[len - 1] = '\0';
        }
    }
    fclose(f);
    return 0;
}
