/*
 * As Tcl 8.2 introduced some changes needed by Expect and also by tcLex
 * (ie matching at the beginning of string and partial match reporting,
 * we don't need to include a patched version of the regexp engine.
 */


static Tcl_RegExp
getRegexp(interp, reObj, flags)
    Tcl_Interp *interp;
    Tcl_Obj    *reObj;
    int        flags;
{
    /* Hack: Corrects a bug in Tcl handling of REG_BOSONLY: REs must be
     * enclosed between non-capturing parentheses */
    static Tcl_ObjType *regexpObjType = NULL;
    Tcl_RegExp regexp;
    TclRegexp *regexpPtr = (TclRegexp *) reObj->internalRep.otherValuePtr;
    int cflags = REG_ADVANCED | REG_BOSONLY | REG_EXPECT
		    | ((flags & LEXER_FLAG_LINES)  ? REG_NEWLINE : 0)
		    | ((flags & LEXER_FLAG_NOCASE) ? REG_ICASE   : 0);

    
    if (regexpObjType == NULL || reObj->typePtr != regexpObjType || regexpPtr->flags != cflags) {
	/* Build a temporary string holding the modified regexp */
	int tmpi;
	char *tmpc;
	char *re;

	re = Tcl_Alloc(reObj->length+5);
	strcpy(re, "(?:");
	strncat(re, reObj->bytes, reObj->length);
	strcat(re, ")");

	tmpi = reObj->length;
	tmpc = reObj->bytes;
	reObj->bytes = re;
	reObj->length = reObj->length+4;

	regexp = Tcl_GetRegExpFromObj(interp, reObj, cflags);

	reObj->bytes = tmpc;
	reObj->length = tmpi;

	Tcl_Free(re);

	regexpObjType = reObj->typePtr;
    } else {
	regexp = (Tcl_RegExp) regexpPtr;
    }

    return regexp;
}




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
    int status, flags = 0;
    Tcl_UniChar *string = Tcl_GetUnicode(stringObj);
    int numChars = Tcl_GetCharLength(stringObj);
    TclRegexp *regexpPtr = (TclRegexp *)getRegexp(interp, rule->reObj, lexer->flags);

    /* Error checking */
    if (regexpPtr == NULL)
	return -1;

    /* Check for beginning of (line|string) */
    flags |= (   (index == start)
	   || (   (regexpPtr->flags & REG_NLANCH)
	       && (string[index-1] == '\n')))
	  ? 0 : REG_NOTBOL;

    /*
     * Perform the regexp match.
     */

    status = Tcl_RegExpExecObj(interp, (Tcl_RegExp)regexpPtr, stringObj, index,
		    regexpPtr->re.re_nsub + 1, flags);

    /* Check overrun */
    if (regexpPtr->details.rm_extend.rm_so == 0
	&& regexpPtr->details.rm_extend.rm_eo == numChars-index) {
	*pOverrun = 1;
    }

    return status;
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
RuleGetRange(interp, lexer, rule, stringObj, index, rangeIndex, startPtr, endPtr)
    Tcl_Interp  *interp;
    TcLex_Lexer	*lexer;
    TcLex_Rule  *rule;
    Tcl_Obj	*stringObj; /* Unused */
    int		index;
    int		rangeIndex;
    int		*startPtr;
    int		*endPtr;
{
    TclRegexp *regexpPtr = (TclRegexp *)getRegexp(interp, rule->reObj, lexer->flags);

    /* Error checking */
    if (regexpPtr == NULL) {
	*startPtr = *endPtr = -1;
	return;
    }

    if ((size_t) rangeIndex > regexpPtr->re.re_nsub) {
	*startPtr = *endPtr = -1;
    } else {
	*startPtr = regexpPtr->matches[rangeIndex].rm_so+index;
	*endPtr   = regexpPtr->matches[rangeIndex].rm_eo+index-1;
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

Tcl_ObjCmdProc *regexpObjCmd = NULL;
Tcl_Obj *regexpObj = NULL,
	*lineObj   = NULL,
	*nocaseObj = NULL,
	*dashObj   = NULL,
	*stringObj = NULL;
int
RuleCompileRegexp(interp, rule, reObj, flags)
    Tcl_Interp	*interp;
    TcLex_Rule	*rule;
    Tcl_Obj	*reObj;
    int		flags;
{
    int i, reLength;
    char *reString = Tcl_GetStringFromObj(reObj, &reLength);
    Tcl_RegExp re = getRegexp(interp, reObj, flags);          /* Compiled regexp */

    /* Error checking */
    if (re == NULL) {
	return TCL_ERROR;
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
}


void
RuleFree(rule)
    TcLex_Rule *rule;
{
    Tcl_Free((char*)rule->conditionsIndices);
    if (rule->reObj)
	Tcl_DecrRefCount(rule->reObj);
    /* Do not free re because it belongs to reObj */
    if (rule->matchVars)
	Tcl_DecrRefCount(rule->matchVars);
    if (rule->script)
	Tcl_DecrRefCount(rule->script);
}
