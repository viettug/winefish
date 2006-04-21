#ifndef __KEY_MAP_H__
#define __KEY_MAP_H__
#include "config.h"

#ifdef SNOOPER2

void keymap_init(void);
void funclist_init();
void rcfile_parse_keys(void *keys_list);

#endif /* SNOOPER2 */
#endif /* __KEY_MAP_H__ */
