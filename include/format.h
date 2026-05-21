#ifndef FORMAT_H
#define FORMAT_H

#include "procfs.h"
#include "options.h"

void print_output(const ProcInfo *info, const MapList *list, const Options *opts);

#endif