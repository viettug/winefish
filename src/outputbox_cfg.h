#ifndef __OUTPUTBOX_CFG_H_
#define __OUTPUTBOX_CFG_H_

#include "config.h"
/*
 kyanh's backend: <=== unmaintained !!!
 	- good, but for *nix* only
 	- trouble with dvips
 	- require gtk2.2
 	- use `deprecated' functions: g_io_channel_*
 
 bluefish's backend:
 	- good
 	- TODO: better handling (OB_FETCHING)
 	- require gtk2.4
*/

/* #define __KA_BACKEND__ 1 */
#define __BF_BACKEND__ 1

#ifndef HAVE_ATLEAST_GTK_2_4
#undef __BF_BACKEND__
#define __KA_BACKEND__ 1
#endif /* HAVE_ATLEAST_GTK_2_4 */

#endif /* __OUTPUTBOX_CFG_H_ */
