/*
 * regenerate_v7.c - Regenerate Frontier-v6-v7.root from Frontier-v6.root
 *
 * Simple utility to migrate the v6 database to v7 format using the migration tools.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations from db_format.h */
extern bool migrate_32bit_to_64bit(const char *db_path);
extern bool ensure_database_modern(const char *db_path, bool *migrated, char *output_path, size_t output_path_size);

int main(int argc, char **argv) {
	const char *source_db = "Frontier-v6.root";
	const char *dest_db = "Frontier-v6-v7.root";
	char output_path[1024] = {0};
	bool migrated = false;

	printf("Regenerating v7 database from %s\n", source_db);
	printf("Output: %s\n", dest_db);

	/* Copy v6 to v6-v7 first */
	char cmd[2048];
	snprintf(cmd, sizeof(cmd), "cp '%s' '%s'", source_db, dest_db);

	printf("\nStep 1: Copying v6 database...\n");
	if (system(cmd) != 0) {
		fprintf(stderr, "ERROR: Failed to copy database\n");
		return 1;
	}

	printf("Step 2: Migrating to v7 format...\n");
	if (!ensure_database_modern(dest_db, &migrated, output_path, sizeof(output_path))) {
		fprintf(stderr, "ERROR: Migration failed\n");
		return 1;
	}

	if (migrated) {
		printf("✓ Successfully migrated to v7 format\n");
		if (strlen(output_path) > 0) {
			printf("  Output path: %s\n", output_path);
		}
	} else {
		printf("Database was already in v7 format\n");
	}

	return 0;
}
