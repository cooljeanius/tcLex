/*
 * This is a slightly modified portion of Tcl's regexp.c file. Please read
 * this file for further information (legal issues, comments...). The reasons
 * why we do that instead of calling Tcl regexp functions directly are:
 *
 *   - in some cases (eg. incremental processing) we need further information
 *     about what has/hasn't been matched. If the entire string has been
 *     consumed, there is overrun. If the string didn't match, we need to
 *     check if it is due to an overrun. Since Tcl functions doesn't provide
 *     any info about that, and the regexp package doesn't handle this kind
 *     of cases (it hasn't been designed for incremental processing), we
 *     need to redefine the matching functions.
 *   - our regexps need to match at the beginning of strings. Tcl regexps
 *     can match further.
 *   - we may need to do that anyway if we plan to add input from files or
 *     channels.
 *
 * Also note that we only redefine the regexp exec functions, and not the
 * regexp compile (we still use Tcl's). Debug options have been stripped.
 */


/*
 * First, copy needed definitions. Comments stripped
 */


#define	END	0	/* no	End of program. */
#define	BOL	1	/* no	Match "" at beginning of line. */
#define	EOL	2	/* no	Match "" at end of line. */
#define	ANY	3	/* no	Match any one character. */
#define	ANYOF	4	/* str	Match any character in this string. */
#define	ANYBUT	5	/* str	Match any character not in this string. */
#define	BRANCH	6	/* node	Match this alternative, or the next... */
#define	BACK	7	/* no	Match "", "next" ptr points backward. */
#define	EXACTLY	8	/* str	Match this string. */
#define	NOTHING	9	/* no	Match empty string. */
#define	STAR	10	/* node	Match this (simple) thing 0 or more times. */
#define	PLUS	11	/* node	Match this (simple) thing 1 or more times. */
#define	OPEN	20	/* no	Mark this point in input as start of #n. */
			/*	OPEN+1 is number 1, etc. */
#define	CLOSE	(OPEN+NSUBEXP)	/* no	Analogous to OPEN. */


#define	OP(p)	(*(p))
#define	NEXT(p)	(((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define	OPERAND(p)	((p) + 3)


#ifndef CHARBITS
#define	UCHARAT(p)	((int)*(unsigned char *)(p))
#else
#define	UCHARAT(p)	((int)*(p)&CHARBITS)
#endif


static char regdummy;


#define	MAGIC	0234


static char *		regnext _ANSI_ARGS_((char *p));

struct regexec_state;

static int 		regtry _ANSI_ARGS_((regexp *prog, char *string,
			    struct regexec_state *restate));
static int 		regmatch _ANSI_ARGS_((char *prog,
			    struct regexec_state *restate));
static int 		regrepeat _ANSI_ARGS_((char *p,
			    struct regexec_state *restate));


static char *
regnext(p)
register char *p;
{
	register int offset;

	if (p == &regdummy)
		return(NULL);

	offset = NEXT(p);
	if (offset == 0)
		return(NULL);

	if (OP(p) == BACK)
		return(p-offset);
	else
		return(p+offset);
}


/*
 * Then add or modify functions and structures
 */

struct regexec_state  {
    char *reginput;
    char *regbol;
    char **regstartp;
    char **regendp;
    /* Addition */
    int  bLines;  /* Line-sensitive processing (activates ^$) */
    int  bNocase; /* Case-insensitive matching */
    int  overrun; /* Tell if an overrun occured */
    /* End addition */
};

int stringLength(string, restate)
  const char *string;
  struct regexec_state *restate;
{
  char *str;
  for (str = (char*)string; *str; str++) {
    if (restate->bLines && *str == '\n')
      break;
  }
  return (str-string);
}


int compareStrings(string, matchString, count, restate)
  const char *string;
  const char *matchString;
  size_t count;
  struct regexec_state *restate;
{
  size_t i=0;
  while (i<count) {
    if (matchString[i] == '\0') {
      restate->overrun = 1;
      return 1;
    }
    if (   (string[i] != matchString[i])
        && (!restate->bNocase || tolower(string[i]) != tolower(matchString[i]))) {
      return 1;
    }
    i++;
  }
  return 0;
}
   
char* findChar(string, c, restate)
  const char *string;
  int c;
  struct regexec_state *restate;
{
  for (;*string; string++) {
    if (   (*string == (char)c)
        || (restate->bNocase && tolower(*string) == tolower((char)c))) {
      return (char*)string;
    }
  }
  return NULL;
}



static int			/* 0 failure, 1 success */
regtry(prog, string, restate)
regexp *prog;
char *string;
struct regexec_state *restate;
{
	register int i;
	register char **sp;
	register char **ep;

	restate->reginput = string;
	restate->regstartp = prog->startp;
	restate->regendp = prog->endp;

    sp = prog->startp;
	ep = prog->endp;
	for (i = NSUBEXP; i > 0; i--) {
		*sp++ = NULL;
		*ep++ = NULL;
	}
	if (regmatch(prog->program + 1,restate)) {
		prog->startp[0] = string;
		prog->endp[0] = restate->reginput;
        /* Addition */
        if (*prog->endp[0] == '\0')
          restate->overrun = 1;
        /* End addition */
		return(1);
	} else
		return(0);
}

static int			/* 0 failure, 1 success */
regmatch(prog, restate)
char *prog;
struct regexec_state *restate;
{
    register char *scan;	/* Current node. */
    char *next;		/* Next node. */

    scan = prog;
    while (scan != NULL) {
	next = regnext(scan);

    /* Addition */
	if (*restate->reginput == '\0') {
      restate->overrun = 1;
    }
    if (   restate->bLines
        && restate->regbol != restate->reginput
        && *restate->reginput == '\n') {
      restate->regbol = restate->reginput+1;
    }
    /* End addition */

	switch (OP(scan)) {
	    case BOL:
		if (restate->reginput != restate->regbol) {
		    return 0;
		}
		/* End modification */
		break;
	    case EOL:
		/* Modification */
/*		if (*restate->reginput != '\0') {
		    return 0;
		}*/
		if (   *restate->reginput != '\0'
		    && (restate->bLines && *restate->reginput != '\n')) {
		    return 0;
		}
		/* End modification */
		break;
	    case ANY:
		/* Modification */
/*		if (*restate->reginput == '\0') {
		    return 0;
		}*/
		if (   *restate->reginput == '\0'
		    || (restate->bLines && *restate->reginput == '\n')) {
		    return 0;
		}
		/* End modification */
		restate->reginput++;
		break;
	    case EXACTLY: {
		register int len;
		register char *opnd;

		opnd = OPERAND(scan);
		/* Inline the first character, for speed. */
        /* Modification */
/*		if (*opnd != *restate->reginput) {*/
        if (   *opnd != *restate->reginput
            && (!restate->bNocase || tolower(*opnd) != tolower(*restate->reginput))) {
        /* End modification */
		    return 0 ;
		}
		len = strlen(opnd);

        /* Modification */
/*        if (len > 1 && strncmp(opnd, restate->reginput, (size_t) len)
			!= 0) {*/
        if (len > 1 && compareStrings(opnd, restate->reginput, (size_t) len, restate)) {
        /* End modification */
		    return 0;
		}
		restate->reginput += len;
		break;
	    }
	    case ANYOF:
        /* Modification */
/*		if (*restate->reginput == '\0'
			|| strchr(OPERAND(scan), *restate->reginput) == NULL) {*/
		if (*restate->reginput == '\0'
			|| findChar(OPERAND(scan), *restate->reginput, restate) == NULL) {
        /* End modification */
		    return 0;
		}
		restate->reginput++;
		break;
	    case ANYBUT:
/*		if (*restate->reginput == '\0'
			|| strchr(OPERAND(scan), *restate->reginput) != NULL) {*/
		if (*restate->reginput == '\0'
			|| (restate->bLines && *restate->reginput == '\n')
			|| findChar(OPERAND(scan), *restate->reginput, restate) != NULL) {
        /* End modification */
		    return 0;
		}
		restate->reginput++;
		break;
	    case NOTHING:
		break;
	    case BACK:
		break;
	    case OPEN+1:
	    case OPEN+2:
	    case OPEN+3:
	    case OPEN+4:
	    case OPEN+5:
	    case OPEN+6:
	    case OPEN+7:
	    case OPEN+8:
	    case OPEN+9: {
		register int no;
		register char *save;

	doOpen:
		no = OP(scan) - OPEN;
		save = restate->reginput;

		if (regmatch(next,restate)) {
		    /*
		     * Don't set startp if some later invocation of the
		     * same parentheses already has.
		     */
		    if (restate->regstartp[no] == NULL) {
			restate->regstartp[no] = save;
		    }
		    return 1;
		} else {
		    return 0;
		}
	    }
	    case CLOSE+1:
	    case CLOSE+2:
	    case CLOSE+3:
	    case CLOSE+4:
	    case CLOSE+5:
	    case CLOSE+6:
	    case CLOSE+7:
	    case CLOSE+8:
	    case CLOSE+9: {
		register int no;
		register char *save;

	doClose:
		no = OP(scan) - CLOSE;
		save = restate->reginput;

		if (regmatch(next,restate)) {
				/*
				 * Don't set endp if some later
				 * invocation of the same parentheses
				 * already has.
				 */
		    if (restate->regendp[no] == NULL)
			restate->regendp[no] = save;
		    return 1;
		} else {
		    return 0;
		}
	    }
	    case BRANCH: {
		register char *save;

		if (OP(next) != BRANCH) { /* No choice. */
		    next = OPERAND(scan); /* Avoid recursion. */
		} else {
		    do {
			save = restate->reginput;
			if (regmatch(OPERAND(scan),restate))
			    return(1);
			restate->reginput = save;
			scan = regnext(scan);
		    } while (scan != NULL && OP(scan) == BRANCH);
		    return 0;
		}
		break;
	    }
	    case STAR:
	    case PLUS: {
		register char nextch;
		register int no;
		register char *save;
		register int min;

		/*
		 * Lookahead to avoid useless match attempts
		 * when we know what character comes next.
		 */
		nextch = '\0';
		if (OP(next) == EXACTLY)
		    nextch = *OPERAND(next);
		min = (OP(scan) == STAR) ? 0 : 1;
		save = restate->reginput;
		no = regrepeat(OPERAND(scan),restate);
		while (no >= min) {
		    /* If it could work, try it. */
		    /* Modification */
/*		    if (nextch == '\0' || *restate->reginput == nextch)*/
		    if (   nextch == '\0'
                || *restate->reginput == nextch
                || (restate->bNocase && tolower(*restate->reginput) == tolower(nextch)))
		    /* End modification */
			if (regmatch(next,restate))
			    return(1);
		    /* Couldn't or didn't -- back up. */
		    no--;
		    restate->reginput = save + no;
		}
		return(0);
	    }
	    case END:
		return(1);	/* Success! */
	    default:
		if (OP(scan) > OPEN && OP(scan) < OPEN+NSUBEXP) {
		    goto doOpen;
		} else if (OP(scan) > CLOSE && OP(scan) < CLOSE+NSUBEXP) {
		    goto doClose;
		}
		TclRegError("memory corruption");
		return 0;
	}

	scan = next;
    }

    /*
     * We get here only if there's trouble -- normally "case END" is
     * the terminating point.
     */
    TclRegError("corrupted pointers");
    return(0);
}

static int
regrepeat(p, restate)
char *p;
struct regexec_state *restate;
{
	register int count = 0;
	register char *scan;
	register char *opnd;

	scan = restate->reginput;
	opnd = OPERAND(p);
	switch (OP(p)) {
	case ANY:
		/* Modification */
/*		count = strlen(scan);*/
		count = stringLength(scan, restate);
        /* End modification */
		scan += count;
		break;
	case EXACTLY:
		/* Modification */
/*		while (*opnd == *scan) {*/
		while (   *opnd == *scan
               || (restate->bNocase && tolower(*opnd) == tolower(*scan))) {
		/* End modification */
			count++;
			scan++;
		}
		break;
	case ANYOF:
		/* Modification */
/*		while (*scan != '\0' && strchr(opnd, *scan) != NULL) {*/
		while (*scan != '\0' && findChar(opnd, *scan, restate) != NULL) {
		/* End modification */
			count++;
			scan++;
		}
		break;
	case ANYBUT:
		/* Modification */
/*		while (*scan != '\0' && strchr(opnd, *scan) == NULL) {*/
		while (*scan != '\0' && (!restate->bLines || *scan != '\n') && findChar(opnd, *scan, restate) == NULL) {
		/* End modification */
			count++;
			scan++;
		}
		break;
	default:		/* Oh dear.  Called inappropriately. */
		TclRegError("internal foulup");
		count = 0;	/* Best compromise. */
		break;
	}
	restate->reginput = scan;

	return(count);
}


/* This is a modified version of Tcl_RegExpExec and TclRegExec
   that only matches at the beginning of the strings */

int
RuleExec(interp, lexer, rule, stringObj, index, start, pOverrun)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
    TcLex_Rule  *rule;
    Tcl_Obj	*stringObj;
    int		index;
    int		start;
    int		*pOverrun; /* Used to report overrun conditions */
{
    char *string = Tcl_GetStringFromObj(stringObj, NULL);
    int match;
#if 0
    /* Deactivate regmust -- see below */
    register char *s;
#endif
    regexp *prog = (regexp *) rule->re;
    struct regexec_state state;
    struct regexec_state *restate= &state;

    TclRegError((char *) NULL);

    /* Be paranoid... */
    if (prog == NULL || string == NULL) {
	TclRegError("NULL parameter");
	return(0);
    }

    /* Check validity of program. */
    if (UCHARAT(prog->program) != MAGIC) {
	TclRegError("corrupted program");
	return(0);
    }

    /* Addition */
    restate->bLines  = lexer->flags & LEXER_FLAG_LINES;
    restate->bNocase = lexer->flags & LEXER_FLAG_NOCASE;
    restate->overrun = 0;
    /* End addition */

#if 0 
    /*
     * Deactivate the regmust feature. It is a performance hog
     * for large strings: if the regmust string isn't present,
     * the entire string is matched again and again every time
     * the rule is matched.
     */
     
    /* If there is a "must appear" string, look for it. */
    if (prog->regmust != NULL) {
	s = string+index;
	/* Modification */
/*    while ((s = strchr(s, prog->regmust[0])) != NULL) {*/
	while ((s = findChar(s, prog->regmust[0], restate)) != NULL) {
/*	  if (strncmp(s, prog->regmust, (size_t) prog->regmlen)*/
	    if (compareStrings(s, prog->regmust, (size_t) prog->regmlen, restate)
		  == 0)
	/* End modification */
		break;	/* Found it. */
	    s++;
	}
	if (s == NULL)	/* Not present. */
	    return(0);
    }
#endif
    
    /* Mark beginning of line for ^ . */

    /* Modification */ 
/*  restate->regbol = start;*/
    restate->regbol = (   (index == start)
		       || (   (restate->bLines)
			   && (*(string-1) == '\n')))
		      ? string+index : string+start;
    /* End modification */

    match = regtry(prog, string+index, restate);
    *pOverrun = restate->overrun;

    if (TclGetRegError() != NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "error while matching regular expression: ",
	    TclGetRegError(), (char *) NULL);
	return -1;
    }
    return match;
}


/*
 *--------------------------------------------------------------
 *
 * RuleGetRange --
 *
 *	This procedure returns the matching range of re's i-th
 *	subexpression.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Returns range indices in the given pointers.
 *
 *--------------------------------------------------------------
 */

void
RuleGetRange(interp, lexer, rule, stringObj, index, ruleIndex, startPtr, endPtr)
    Tcl_Interp  *interp;
    TcLex_Lexer	*lexer;
    TcLex_Rule  *rule;
    Tcl_Obj	*stringObj; 
    int		index;
    int		ruleIndex;
    int		*startPtr;
    int		*endPtr;
{
    char *string = Tcl_GetStringFromObj(stringObj, NULL);
    char *s, *e;
    Tcl_RegExpRange(rule->re, ruleIndex, &s, &e);

    if (s == NULL) {
	*startPtr = *endPtr = -1;
    } else {
	*startPtr = s-string;
	*endPtr   = e-string-1;
    }
}


/*
 *--------------------------------------------------------------
 *
 * RuleCompileRegexp --
 *
 *	This procedure compiles the given regexp.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Modifies the given rule.
 *
 *--------------------------------------------------------------
 */

int
RuleCompileRegexp(interp, rule, reObj, flags)
    Tcl_Interp	*interp;
    TcLex_Rule	*rule;
    Tcl_Obj	*reObj;
    int		flags;
{
    int i, reLength;
    char *reString = Tcl_GetStringFromObj(reObj, &reLength);
    Tcl_RegExp re;          /* Compiled regexp */

    /*
     * Compile the regexp
     */

    TclRegError((char *) NULL);
    re = (Tcl_RegExp)TclRegComp(reString);
    if (TclGetRegError() != NULL) {
	/* Error */
	goto regexpCompileError;
    }

    /*
     * Initialize the rule
     */

    /* Roughly count the number of ranges, ie the parentheses */
    rule->nbRanges = 1;
    for (i=0; i < reLength; i++) {
	if (reString[i] == '(') rule->nbRanges++;
    }

    rule->re    = re;
    rule->reObj = reObj;
    Tcl_IncrRefCount(reObj);

    return TCL_OK;


    /*
     * Error handling
     */

regexpCompileError:
    Tcl_AppendResult(interp, "couldn't compile regular expression pattern: ",
		           TclGetRegError(), (char *) NULL);
    return TCL_ERROR;
}


void
RuleFree(rule)
    TcLex_Rule *rule;
{
    Tcl_Free((char*)rule->conditionsIndices);
    if (rule->reObj)
	Tcl_DecrRefCount(rule->reObj);
    Tcl_Free((char*)rule->re);
    if (rule->matchVars)
	Tcl_DecrRefCount(rule->matchVars);
    if (rule->script)
	Tcl_DecrRefCount(rule->script);
}
