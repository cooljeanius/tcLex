/*
 * tcLexRE.c
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#ifndef HAVE_UNISTD_H
# define HAVE_UNISTD_H
#endif /* !HAVE_UNISTD_H */

#include <tcl.h>
#ifdef HAVE_TCLINT_H
# include <tclInt.h>
#else
# include "tclInt.h"
#endif /* HAVE_TCLINT_H */
#include <tclRegexp.h>


#include "tcLex.h"
#include "tcLexRE.h"

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
# include "RE80.c"
#else
# if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 1)
#  include "RE81.c"
# else
#  include "RE82.c"
# endif /* Tcl8.1 */
#endif /* Tcl8.0 */

/* EOF */
