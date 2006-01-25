/* $Id$ */

/* Winefish LaTeX Editor (based on Bluefish)
 * Copyright (C) 2006 kyanh <kyanh@o2.pl>
 */

#ifndef __OUTPUTBOX_KA_H_
#define __OUTPUTBOX_KA_H_

#include "outputbox_cfg.h"

#ifdef __KA_BACKEND__

#include <sys/wait.h> /* wait(), waitid() */

void run_command( Toutputbox *ob );
void finish_execute( Toutputbox *ob );
sig_atomic_t child_exit_status;

#endif /* __KA_BACKEND__ */

#endif /* __OUTPUTBOX_KA_H_ */
