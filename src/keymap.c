#ifdef SNOOPER2

#include <gtk/gtk.h>

#include "rcfile.h"
#include "snooper2.h"

static void rcfile_parse_keys(void *keys_list) {
	gchar *filename;
	GList *keys_configlist=NULL;
	init_prop_arraylist(&keys_configlist, keys_list, "map:",2, TRUE);
	filename = g_strconcat(g_get_current_dir(), "/keymap", NULL);
	if (!parse_config_file(keys_configlist, filename)) {
		DEBUG_MSG("rcfile_parse_keys: failed; cannot locate 'keys' file in current directory\n");
	}
	free_configlist(keys_configlist);
	g_free(filename);
}

/* this function should be called afted the func_hashtable was initialized */
void keymap_init(void) {
	g_print("keys_init:hello\n");
	GList *keys_list = NULL;
	rcfile_parse_keys(&keys_list);
	
	Tsnooper *snooper =  SNOOPER(main_v->snooper);

	snooper->key_hashtable = g_hash_table_new((GHashFunc)g_str_hash, (GEqualFunc)g_str_equal);

	GList *tmp2list = g_list_first(keys_list);
	while (tmp2list) {
		gchar **strarr;
		strarr = (gchar **) tmp2list->data;
		if (count_array(strarr) == 2) {
			if (!g_hash_table_lookup(snooper->key_hashtable, strarr[0])) {
				if (strlen(strarr[1])) {
					gchar *keyseq, *func_name;
					keyseq = g_strdup(strarr[1]);
					func_name = g_strdup(strarr[0]);
					/* TODO: check if func valid.... */
					g_hash_table_insert(snooper->key_hashtable, keyseq, func_name);
					/* please donot free tmpstr ;) */
					DEBUG_MSG("keys_init: map [func=%s] to [keys_seq=%s]\n", func_name, keyseq);
				}
			}
			/* else: replace the old by the new one */
		}
		tmp2list = g_list_next(tmp2list);
	}

	/* we donot need keys_list anymore */
	{
		GList *tmplist = g_list_first(keys_list);
		while(tmplist) {
			void *cli = tmplist->data;
			g_free(cli);
			tmplist = g_list_next(tmplist);
		}
		g_list_free(keys_list);
	}
}

#endif /* SNOOPER2 */
