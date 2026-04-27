#include <stdio.h>
#include <string.h>

#include "frontier.h"
#include "shell.h"

#if !defined(FRONTIER_HEADLESS)
#error "frontier-cli/stubs/headless_shell.c should only be compiled in headless mode"
#endif

static void bigstring_to_cstring(const bigstring bs, char *out, size_t out_size) {
	size_t len = (size_t)bs[0];
	if (len >= out_size)
		len = out_size - 1;
	memcpy(out, &bs[1], len);
	out[len] = '\0';
}

boolean shellvisittypedwindows(short id, shellwindowvisitcallback visit, ptrvoid refcon) {
	(void)id;
	(void)visit;
	(void)refcon;
	return true;
}

void shellerrormessage(bigstring bs) {
	char buffer[256];
	bigstring_to_cstring(bs, buffer, sizeof buffer);
	fprintf(stderr, "shellerrormessage: %s\n", buffer);
}
