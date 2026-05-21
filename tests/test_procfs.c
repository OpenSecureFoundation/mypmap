#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "../include/procfs.h"

/* ---- Compteurs de tests ---- */
static int tests_run = 0;
static int tests_pass = 0;

#define ASSERT_EQ(a, b, msg)                                     \
    do                                                           \
    {                                                            \
        tests_run++;                                             \
        if ((a) == (b))                                          \
        {                                                        \
            printf("  [PASS] %s\n", msg);                        \
            tests_pass++;                                        \
        }                                                        \
        else                                                     \
        {                                                        \
            printf("  [FAIL] %s — attendu %lu, obtenu %lu\n",    \
                   msg, (unsigned long)(b), (unsigned long)(a)); \
        }                                                        \
    } while (0)

#define ASSERT_STR(a, b, msg)                                                   \
    do                                                                          \
    {                                                                           \
        tests_run++;                                                            \
        if (strcmp((a), (b)) == 0)                                              \
        {                                                                       \
            printf("  [PASS] %s\n", msg);                                       \
            tests_pass++;                                                       \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            printf("  [FAIL] %s — attendu '%s', obtenu '%s'\n", msg, (b), (a)); \
        }                                                                       \
    } while (0)

/* ===========================
 * Tests parse_maps_line()
 * =============================*/

void test_parse_normal_line(void)
{
    printf("\n[TEST] Ligne normale avec pathname\n");

    const char *line =
        "7f1a2b3c4000-7f1a2b3c5000 r--p 00000000 08:01 12345 /usr/lib/libc.so.6\n";

    MapEntry e;
    int ret = parse_maps_line(line, &e);

    ASSERT_EQ(ret, 0, "retour == 0");
    ASSERT_EQ(e.addr_start, 0x7f1a2b3c4000UL, "addr_start correct");
    ASSERT_EQ(e.addr_end, 0x7f1a2b3c5000UL, "addr_end correct");
    ASSERT_STR(e.perms, "r--p", "perms correct");
    ASSERT_EQ(e.offset, 0UL, "offset correct");
    ASSERT_EQ(e.dev_major, 0x08U, "dev_major correct");
    ASSERT_EQ(e.dev_minor, 0x01U, "dev_minor correct");
    ASSERT_EQ(e.inode, 12345UL, "inode correct");
    ASSERT_STR(e.pathname, "/usr/lib/libc.so.6", "pathname correct");
}

void test_parse_anonymous_line(void)
{
    printf("\n[TEST] Ligne anonyme (pas de pathname)\n");

    const char *line =
        "7fff12340000-7fff12360000 rw-p 00000000 00:00 0 \n";

    MapEntry e;
    int ret = parse_maps_line(line, &e);

    ASSERT_EQ(ret, 0, "retour == 0");
    ASSERT_EQ(e.addr_start, 0x7fff12340000UL, "addr_start correct");
    ASSERT_STR(e.perms, "rw-p", "perms correct");
    ASSERT_EQ(e.inode, 0UL, "inode == 0 (anonyme)");
    /* pathname doit être vide */
    ASSERT_EQ((int)(e.pathname[0] == '\0' || e.pathname[0] == ' '), 1,
              "pathname vide pour segment anonyme");
}

void test_parse_special_segments(void)
{
    printf("\n[TEST] Segments spéciaux [stack] [heap] [vdso]\n");

    struct
    {
        const char *line;
        const char *expected_name;
    } cases[] = {
        {"7ffd00000000-7ffd00021000 rw-p 00000000 00:00 0          [stack]\n",
         "[stack]"},
        {"0000000000600000-0000000000601000 rw-p 00000000 00:00 0  [heap]\n",
         "[heap]"},
        {"7f1234560000-7f1234562000 r-xp 00000000 00:00 0          [vdso]\n",
         "[vdso]"},
        {NULL, NULL}};

    for (int i = 0; cases[i].line; i++)
    {
        MapEntry e;
        int ret = parse_maps_line(cases[i].line, &e);
        ASSERT_EQ(ret, 0, "parse réussi");
        /* Vérification souple : le pathname doit contenir le nom attendu */
        tests_run++;
        if (strstr(e.pathname, cases[i].expected_name) != NULL)
        {
            printf("  [PASS] pathname contient '%s'\n", cases[i].expected_name);
            tests_pass++;
        }
        else
        {
            printf("  [FAIL] pathname '%s' ne contient pas '%s'\n",
                   e.pathname, cases[i].expected_name);
        }
    }
}

void test_parse_malformed_line(void)
{
    printf("\n[TEST] Ligne malformée\n");

    const char *line = "this is not a valid maps line\n";
    MapEntry e;
    int ret = parse_maps_line(line, &e);
    ASSERT_EQ(ret, -1, "retour == -1 pour ligne invalide");
}

/* ============================================================
 * Tests append_entry() et maplist_create/free
 * ============================================================ */

void test_maplist_dynamic(void)
{
    printf("\n[TEST] Tableau dynamique (append + realloc)\n");

    MapList *list = maplist_create();
    assert(list != NULL);
    ASSERT_EQ(list->count, 0UL, "count initial == 0");

    /* Insérer 32 entrées (> 16 initial → doit realloc) */
    for (int i = 0; i < 32; i++)
    {
        MapEntry e;
        memset(&e, 0, sizeof(e));
        e.addr_start = (unsigned long)i * 0x1000;
        e.addr_end = e.addr_start + 0x1000;
        int ret = append_entry(list, &e);
        ASSERT_EQ(ret, 0, "append_entry réussi");
    }

    ASSERT_EQ(list->count, 32UL, "count == 32 après 32 insertions");
    ASSERT_EQ(list->entries[0].addr_start, 0UL, "entrée[0].addr_start == 0");
    ASSERT_EQ(list->entries[31].addr_start, 31UL * 0x1000, "entrée[31].addr_start correct");

    maplist_free(list);
    printf("  [INFO] maplist_free() appelé — vérifier avec Valgrind\n");
}

/* ============================================================
 * Test d'intégration sur le processus courant
 * ============================================================ */

void test_read_maps_self(void)
{
    printf("\n[TEST] read_maps() sur le processus courant\n");

    MapList *list = read_maps(getpid());
    if (!list)
    {
        printf("  [SKIP] read_maps() a échoué (droits insuffisants ?)\n");
        return;
    }

    tests_run++;
    if (list->count > 0)
    {
        printf("  [PASS] read_maps() retourne %zu segments\n", list->count);
        tests_pass++;
    }
    else
    {
        printf("  [FAIL] read_maps() retourne 0 segments\n");
    }

    /* Vérifier que les adresses sont croissantes */
    int ordered = 1;
    for (size_t i = 1; i < list->count; i++)
    {
        if (list->entries[i].addr_start < list->entries[i - 1].addr_start)
            ordered = 0;
    }
    ASSERT_EQ(ordered, 1, "segments ordonnés par adresse croissante");

    maplist_free(list);
}

void test_read_smaps_self(void)
{
    printf("\n[TEST] read_smaps() sur le processus courant\n");

    MapList *list = read_maps(getpid());
    if (!list)
    {
        printf("  [SKIP]\n");
        return;
    }

    int ret = read_smaps(getpid(), list);
    ASSERT_EQ(ret, 0, "read_smaps() réussi");

    /* Vérifier qu'au moins une entrée a un RSS non nul */
    int has_rss = 0;
    for (size_t i = 0; i < list->count; i++)
    {
        if (list->entries[i].rss_kb > 0)
        {
            has_rss = 1;
            break;
        }
    }
    ASSERT_EQ(has_rss, 1, "au moins une entrée a rss_kb > 0");

    maplist_free(list);
}

/* ============================================================
 * main
 * ============================================================ */

int main(void)
{
    printf("============================================\n");
    printf("   Tests unitaires mypmap — procfs\n");
    printf("============================================\n");

    test_parse_normal_line();
    test_parse_anonymous_line();
    test_parse_special_segments();
    test_parse_malformed_line();
    test_maplist_dynamic();
    test_read_maps_self();
    test_read_smaps_self();

    printf("\n============================================\n");
    printf("   Résultats : %d/%d tests passés\n", tests_pass, tests_run);
    printf("============================================\n");

    return (tests_pass == tests_run) ? 0 : 1;
}
