#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H
#endif

#include <tcl.h>
#ifdef HAVE_TCLINT_H
#include <tclInt.h>
#else
#include "tclInt.h"
#endif
#include <tclRegexp.h>


#include "tcLex.h"
#include "tcLexRE.h"

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
    #include "RE80.c"
#else
    #if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 1)
	#include "RE81.c"
    #else
	#include "RE82.c"
    #endif
#endif

