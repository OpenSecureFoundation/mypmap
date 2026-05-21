/* procfs.h */
#ifndef PROCFS_H
#define PROCFS_H

#include <sys/types.h>

typedef struct {
    unsigned long addr_start;
    unsigned long addr_end;
    char          perms[6];
    unsigned long offset;
    unsigned int  dev_major;
    unsigned int  dev_minor;
    unsigned long inode;
    char          pathname[256];

    /* Smaps fields */
    unsigned long size_kb;
    unsigned long rss_kb;
    unsigned long pss_kb;
    unsigned long shared_clean_kb;
    unsigned long shared_dirty_kb;
    unsigned long private_clean_kb;
    unsigned long private_dirty_kb;
    unsigned long referenced_kb;
    unsigned long anonymous_kb;
    unsigned long swap_kb;
    unsigned long swap_pss_kb;
    unsigned long locked_kb;
} MapEntry;

typedef struct {
    MapEntry *entries;
    size_t    count;
    size_t    capacity;
} MapList;

typedef struct {
    pid_t pid;
    char  name[256];
    char  cmdline[1024];
} ProcInfo;

MapList* maplist_create(void);
void     maplist_free(MapList *list);
int      read_proc_info(pid_t pid, ProcInfo *info);
MapList* read_maps(pid_t pid);
int      read_smaps(pid_t pid, MapList *list);
int      parse_maps_line(const char *line, MapEntry *entry);
int      append_entry(MapList *list, const MapEntry *entry);

#endif
