#ifndef SCRIPT_PORTABLE_H
#define SCRIPT_PORTABLE_H

#if defined(FRONTIER_HEADLESS)
void headless_init_script_compiler(void);
void headless_clear_last_lang_error(void);
const unsigned char *headless_get_last_lang_error(void);
#endif

#endif /* SCRIPT_PORTABLE_H */
