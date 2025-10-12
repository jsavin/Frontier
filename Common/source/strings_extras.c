#include "frontier.h"
#include "strings.h"

void bs_from_c(const char *cstr, bigstring out) {
    copyctopstring(cstr, out);
}

void c_from_bs(const bigstring in, char *out, unsigned long out_sz) {
    if (out == 0 || out_sz == 0) return;
    unsigned long n = (unsigned long) in[0];
    if (n >= out_sz) n = out_sz - 1;
    for (unsigned long i = 0; i < n; ++i)
        out[i] = (char) in[i + 1];
    out[n] = '\0';
}
