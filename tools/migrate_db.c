/* Minimal database migration tool */
#include <stdio.h>
#include <stdlib.h>

/* Forward declaration from db_format.c */
extern int db_migrate_file_to_v7(const char *path);

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s database.root\n", argv[0]);
        fprintf(stderr, "Migrates the database file to v7 format in-place.\n");
        return 1;
    }

    const char *path = argv[1];
    printf("Migrating %s to v7 format...\n", path);

    int result = db_migrate_file_to_v7(path);

    if (result == 0) {
        printf("Migration successful!\n");
        return 0;
    } else {
        fprintf(stderr, "Migration failed with code %d\n", result);
        return 1;
    }
}
