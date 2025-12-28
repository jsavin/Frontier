/*
 * test_dump_system_paths.c
 *
 * Diagnostic tool to examine the on-disk representation of system.paths
 * address values in a v7 database.
 *
 * This investigates the "bare new() missing valueroutine" issue by:
 * 1. Reading the raw packed bytes of each system.paths entry
 * 2. Showing what stringtoaddress() resolves them to
 * 3. Checking if the resolved tables have the expected structure
 */

#include <stdio.h>

#include "../Common/headers/standard.h"
#include "../Common/headers/memory.h"
#include "../Common/headers/strings.h"
#include "../Common/headers/db.h"
#include "../Common/headers/lang.h"
#include "../Common/headers/langinternal.h"
#include "../Common/headers/langexternal.h"
#include "../Common/headers/tablestructure.h"
#include "../Common/headers/tableverbs.h"
#include "../Common/headers/kernelverbs.h"
#include "../Common/headers/kernelverbdefs.h"
#include "../Common/headers/process.h"
#include "../Common/headers/langpack.h"
#include "../Common/headers/langhash.h"
#include "../Common/headers/logging.h"

/* Forward declarations */
static void dump_hex_bytes(const char *label, const unsigned char *bytes, size_t len);
static void analyze_system_paths_entry(hdlhashtable pathstable, bigstring name);

static void dump_hex_bytes(const char *label, const unsigned char *bytes, size_t len) {
	printf("  %s (%zu bytes): ", label, len);
	for (size_t i = 0; i < len; i++) {
		printf("%02x ", bytes[i]);
		if ((i + 1) % 16 == 0 && i + 1 < len)
			printf("\n    ");
	}
	printf("\n");
}

static void analyze_system_paths_entry(hdlhashtable pathstable, bigstring name) {
	hdlhashnode hnode;
	tyvaluerecord val;
	char cname[256];

	copyptocstring(name, cname);

	/* Lookup the entry */
	if (!hashtablelookup(pathstable, name, &val, &hnode)) {
		printf("  ERROR: Failed to lookup %s\n", cname);
		return;
	}

	printf("\n=== Entry: %s ===\n", cname);
	printf("  Value type: %d", val.valuetype);

	if (val.valuetype == addressvaluetype) {
		printf(" (addressvaluetype)\n");

		/* Show the string component */
		hdlstring hstring = val.data.addressvalue;
		if (hstring != nil && *hstring != nil) {
			bigstring bs;
			texthandletostring(hstring, bs);
			char cpath[512];
			copyptocstring(bs, cpath);
			printf("  String path: '%s'\n", cpath);

			/* Dump raw bytes of the handle */
			size_t handlesize = GetHandleSize((Handle)hstring);
			dump_hex_bytes("Raw handle bytes", (const unsigned char *)*hstring, handlesize);
		} else {
			printf("  String path: (nil handle)\n");
		}

		/* Try to resolve the address */
		hdlhashtable resolved_table;
		bigstring resolved_name;

		fllangerror = false;
		boolean resolved = getaddressvalue(val, &resolved_table, resolved_name);

		if (resolved && !fllangerror) {
			char cresolvedname[256];
			copyptocstring(resolved_name, cresolvedname);
			printf("  Resolved to: table=%p, name='%s'\n",
				(void *)resolved_table, cresolvedname);

			/* Try to get the table value */
			hdlhashtable final_table;
			if (langgettableval(resolved_table, resolved_name, &final_table)) {
				printf("  Final table: %p\n", (void *)final_table);

				/* Check if it's an external table */
				tyvaluerecord tableval;
				if (hashtablelookup(resolved_table, resolved_name, &tableval, &hnode)) {
					if (tableval.valuetype == externalvaluetype) {
						hdlexternalvariable hv = (hdlexternalvariable)tableval.data.externalvalue;
						printf("  External table: id=%d, flinmemory=%d\n",
							(**hv).id, (**hv).flinmemory);

						/* Check for valueroutine in the table */
						if ((**hv).flinmemory) {
							hdlhashtable ht = (hdlhashtable)(**hv).variabledata;
							bigstring bsvalueroutine;
							copystring("\pvalueroutine", bsvalueroutine);
							tyvaluerecord vrval;

							if (hashtablelookup(ht, bsvalueroutine, &vrval, &hnode)) {
								printf("  ✓ Has 'valueroutine' entry (type=%d)\n", vrval.valuetype);
							} else {
								printf("  ✗ Missing 'valueroutine' entry\n");
							}
						}
					}
				}
			} else {
				printf("  ERROR: Failed to get table value\n");
			}
		} else {
			printf("  ERROR: Failed to resolve address (fllangerror=%d)\n", fllangerror);
		}

		fllangerror = false;

	} else {
		printf(" (not an address)\n");
	}
}

int main(int argc, char *argv[]) {
	boolean fl;

	/* Initialize logging */
	log_init();
	log_set_level_for_component(LOG_COMP_DB, LOG_LEVEL_WARN);
	log_set_level_for_component(LOG_COMP_LANG, LOG_LEVEL_TRACE);
	log_set_level_for_component(LOG_COMP_HASH, LOG_LEVEL_WARN);

	printf("=== System Paths Diagnostic Tool ===\n\n");

	/* Initialize Frontier runtime */
	if (!langinitverbs()) {
		printf("ERROR: Failed to initialize language verbs\n");
		return 1;
	}

	/* Open the v7 database */
	const char *dbpath = "databases/Frontier-v6-v7.root";
	bigstring bsdbpath;

	copyctopstring(dbpath, bsdbpath);

	printf("Opening database: %s\n", dbpath);

	fl = dbopenfile(bsdbpath, false /* flreadonly */);
	if (!fl) {
		printf("ERROR: Failed to open database\n");
		return 1;
	}

	printf("Database opened successfully\n");

	/* Get system table */
	hdlhashtable systemtable;
	bigstring bssystem;
	copystring("\psystem", bssystem);

	if (!langexternalvaltohashtable((tyvaluerecord){.valuetype = novaluetype}, &systemtable, HNoNode)) {
		/* Try to find system in root table */
		if (roottable != nil) {
			tyvaluerecord valsystem;
			hdlhashnode hnode;
			if (hashtablelookup(roottable, bssystem, &valsystem, &hnode)) {
				if (!langexternalvaltotable(valsystem, &systemtable, hnode)) {
					printf("ERROR: system is not a table\n");
					goto cleanup;
				}
			} else {
				printf("ERROR: system not found in root table\n");
				goto cleanup;
			}
		} else {
			printf("ERROR: roottable is nil\n");
			goto cleanup;
		}
	}

	printf("System table: %p\n", (void *)systemtable);

	/* Get system.paths */
	bigstring bspaths;
	copystring("\ppaths", bspaths);

	hdlhashtable pathstable;
	tyvaluerecord valpaths;
	hdlhashnode hnode;

	if (!hashtablelookup(systemtable, bspaths, &valpaths, &hnode)) {
		printf("ERROR: system.paths not found\n");
		goto cleanup;
	}

	if (!langexternalvaltotable(valpaths, &pathstable, hnode)) {
		printf("ERROR: system.paths is not a table\n");
		goto cleanup;
	}

	printf("Paths table: %p\n", (void *)pathstable);
	printf("Entry count: %ld\n\n", (long)(**pathstable).hashsize);

	/* Iterate through all entries in system.paths */
	hdlhashnode nomad = (**pathstable).hfirstsort;
	int entry_count = 0;

	while (nomad != nil) {
		bigstring entryname;
		hashgetnodekey(nomad, entryname);

		analyze_system_paths_entry(pathstable, entryname);

		entry_count++;
		nomad = (**nomad).sortedlink;
	}

	printf("\n=== Summary ===\n");
	printf("Total entries analyzed: %d\n", entry_count);

cleanup:
	dbclosefile();

	return 0;
}
