/* 
 * tcLex.c --
 *
 *	This module implements a lexer (or lexical analyzer) command
 *	for Tcl. A lexer is used to parse text for processing with
 *	the Tcl language. It is intended to ease the task of people
 *	already using Tcl for regexp-based text processing.
 *
 * Inspired by Unix and GNU lex and flex
 *
 * Copyright (c) 1998 Frédéric BONNET <frederic.bonnet@ciril.fr>
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef HAVE_UNISTD_H
#define HAVE_UNISTD_H
#endif

#include "tcLex.h"
#include "tcLexInt.h"
#include "tcLexRE.h"

#include <tclInt.h>

/*******************************************************
 *                                                     *
 *          Conditions management functions            *
 *                                                     *
 *******************************************************/

/*
 *--------------------------------------------------------------
 *
 * ConditionIndex --
 *
 *	This procedure returns the index of a lexer's condition from
 *	a string.
 *
 * Results:
 *	0 for the initial condition;
 *	-1 if the condition doesn't exist;
 *	Else a positive integer corresponding to an index.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ConditionIndex(lexer, string)
    TcLex_Lexer	*lexer;
    char	*string; /* Condition to search for */
{
    int		i;
    char	*cond;

    /* 
     * Initial condition
     */

    if (   strcmp(string, "initial") == 0
	|| strcmp(string, "0") == 0) {
	return 0;
    }

    /* 
     * Search for the condition
     */

    for (i=0; i < lexer->nbInclusiveConditions+lexer->nbExclusiveConditions; i++) {
	cond = Tcl_GetStringFromObj(lexer->conditions[i], NULL);
	if (strcmp(string, cond) == 0) {
		return i+1;
	}
    }

    /* 
     * None found, error
     */

    return -1;
}


/*
 *--------------------------------------------------------------
 *
 * ConditionFromIndex --
 *
 *	This procedure returns the condition from its index.
 *
 * Results:
 *	A Tcl_Obj* containing the condition's string, or NULL if
 *	the condition doesn't exist.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static Tcl_Obj *
ConditionFromIndex(lexer, index)
    TcLex_Lexer	*lexer;
    int		index;  /* Index of the condition */
{ 
    /* 
     * Sanity check
     */

    if (   index < 0 
	|| index > lexer->nbInclusiveConditions+lexer->nbExclusiveConditions)
	return NULL;

    /* 
     * Initial condition
     */

    if (index == 0) 
	return InitialCond;

    return lexer->conditions[index-1];
}


/*
 *--------------------------------------------------------------
 *
 * ConditionIsExclusive --
 *
 *	Check if the condition (given its index) is exclusive.
 *
 * Results:
 *	-1 if the condition doesn't exist;
 *	 0 if the condition is inclusive;
 *	 1 if the condition is inclusive.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ConditionIsExclusive(lexer, index)
    TcLex_Lexer	*lexer;
    int		index;  /* Index of the condition */
{
    /*
     * Sanity check
     */

    if (   index < 0 
	|| index > lexer->nbInclusiveConditions+lexer->nbExclusiveConditions)
	return -1;

    if (index <= lexer->nbInclusiveConditions)
	return 0;

    return 1;
}


/*
 *--------------------------------------------------------------
 *
 * ConditionCurrent --
 *
 *	Return the current condition, ie the top of the stack.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
ConditionCurrent(interp, lexer)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
{
    TcLex_State	*statePtr = lexer->states[lexer->curState];

    if (statePtr->conditionsStackLength == 0)
	Tcl_SetObjResult(interp, InitialCond);
    else
	Tcl_SetObjResult(interp, 
	    ConditionFromIndex(lexer,
		statePtr->conditionsStack[statePtr->conditionsStackLength-1]));
	
    return TCL_OK;
}


/*******************************************************
 *                                                     *
 *             State management functions              *
 *                                                     *
 *******************************************************/

/*
 *--------------------------------------------------------------
 *
 * StateNew --
 *
 *	Create a new state for the given lexer, associated with the
 *	given string.
 *
 * Results:
 *	The index of the newly created state.
 *
 * Side effects:
 *	The lexer's <states> array is modified.
 *
 *--------------------------------------------------------------
 */

static int
StateNew(lexer, string)
    TcLex_Lexer	*lexer;
    Tcl_Obj	*string;    /* String being lexed in the new state */
{
    int		i, n;              /* Index of the new state */
    TcLex_State	*statePtr; /* Newly created state    */

    /* 
     * If states already exist...
     */

    if (lexer->states) {
	/*
	 * Search for a free slot in the <states> array
	 */

	for (n=0; n < lexer->nbStates; n++)
	    if (!lexer->states[n])
		goto init;

	/*
	 * No slot found, so grow the array
	 */

	n = lexer->nbStates++;
	lexer->states = (TcLex_State **)Tcl_Realloc((char *)lexer->states, sizeof(TcLex_State *) * lexer->nbStates);

    } else {
	/*
	 * Create one slot in the <states> array
	 */

	lexer->nbStates = 1;
	n = 0;
	lexer->states = (TcLex_State **)Tcl_Alloc(sizeof(TcLex_State *) * lexer->nbStates);
    }

init:
    /*
     * Initialize the state 
     */

    statePtr = lexer->states[n] = (TcLex_State *)Tcl_Alloc(sizeof(TcLex_State));

    statePtr->conditionsStack       = NULL;
    statePtr->conditionsStackLength = 0;
    statePtr->lastMatched           = -1;
    statePtr->bFailed               = Tcl_Alloc(lexer->nbRules);
    for (i=0; i<lexer->nbRules; i++)
	statePtr->bFailed[i] = 0;

    statePtr->inputBuffer.chars = NULL;
    StateSetString(lexer, n, string);
    
    statePtr->inputBuffer.index     = 0;
    statePtr->inputBuffer.bMatchedEmpty = 0;
    
    statePtr->currentBuffer         = &statePtr->inputBuffer;

    statePtr->bRejected             = 0;


    statePtr->startIndices = statePtr->endIndices = NULL;
    statePtr->pendingResult = NULL;

    return n;
}


/*
 * Tcl8.1 compatibily procs: Unicode object type.
 * This object uses a Unicode string in a Tcl_DString as their
 * internal representation
 */

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 1)

Tcl_FreeInternalRepProc FreeUnicode;
Tcl_DupInternalRepProc DupUnicode;
Tcl_UpdateStringProc UpdateUnicode;
Tcl_SetFromAnyProc SetUnicodeFromAny;

Tcl_ObjType UnicodeObjType = {
    "unicode",

    FreeUnicode,	/* freeIntRepProc */
    DupUnicode,		/* dupIntRepProc */
    UpdateUnicode,	/* updateIntRepProc */
    SetUnicodeFromAny	/* setFromAnyProc */
};

void
FreeUnicode(objPtr)
    Tcl_Obj *objPtr;
{
    register Tcl_DString *string = (Tcl_DString *) objPtr->internalRep.otherValuePtr;

    /* Free the DString */
    Tcl_DStringFree(string);
    Tcl_Free((char *) string);
}

void
DupUnicode(srcPtr, dupPtr)
    Tcl_Obj *srcPtr;
    Tcl_Obj *dupPtr;
{
    register Tcl_DString *srcString = (Tcl_DString *) srcPtr->internalRep.otherValuePtr;
    register Tcl_DString *dupString;

    /* Duplicate the DString */
    dupString = (Tcl_DString *) Tcl_Alloc(sizeof(Tcl_DString));
    Tcl_DStringInit(dupString);
    Tcl_DStringAppend(dupString, Tcl_DStringValue(srcString),
	Tcl_DStringLength(srcString));

    dupPtr->internalRep.otherValuePtr = (char *) dupString;
    dupPtr->typePtr = &UnicodeObjType;
}

void
UpdateUnicode(objPtr)
    Tcl_Obj *objPtr;
{
    register Tcl_DString *string = (Tcl_DString *) objPtr->internalRep.otherValuePtr;
    Tcl_DString strUtf;

    /*
     * Set the UTF string from the internal Unicode DString
     */

    /* Generate an UTF representation in a DString */
    Tcl_DStringInit(&strUtf);
    Tcl_UniCharToUtfDString((Tcl_UniChar *) Tcl_DStringValue(string),
	Tcl_DStringLength(string)/sizeof(Tcl_UniChar), &strUtf);

    /* Allocate and fill the UTF string */
    objPtr->length = Tcl_DStringLength(&strUtf);
    objPtr->bytes = Tcl_Alloc((unsigned) objPtr->length + 1);
    strcpy(objPtr->bytes, Tcl_DStringValue(&strUtf));

    Tcl_DStringFree(&strUtf);
}

int
SetUnicodeFromAny(interp, objPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    register Tcl_DString *string;

    /*
     * Free the old internalRep before setting the new one.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    /* Set the internal Unicode string from the UTF string */
    string = (Tcl_DString *) Tcl_Alloc(sizeof(Tcl_DString));
    Tcl_DStringInit(string);
    Tcl_UtfToUniCharDString(objPtr->bytes, objPtr->length, string);

    objPtr->internalRep.otherValuePtr = (char *) string;
    objPtr->typePtr = &UnicodeObjType;
    return TCL_OK;
}

int
Tcl_GetCharLength(objPtr)
    Tcl_Obj *objPtr;
{
    register Tcl_DString *string;
    
    if (objPtr->typePtr != &UnicodeObjType) {
	SetUnicodeFromAny(NULL, objPtr);
    }

    string = (Tcl_DString *) objPtr->internalRep.otherValuePtr;
    return Tcl_DStringLength(string)/sizeof(Tcl_UniChar);
}

Tcl_UniChar *
Tcl_GetUnicode(objPtr)
    Tcl_Obj *objPtr;
{
    register Tcl_DString *string;
    
    if (objPtr->typePtr != &UnicodeObjType) {
	SetUnicodeFromAny(NULL, objPtr);
    }

    string = (Tcl_DString *) objPtr->internalRep.otherValuePtr;
    return (Tcl_UniChar *) Tcl_DStringValue(string);
}

Tcl_Obj *
Tcl_NewUnicodeObj(unicode, numChars)
    Tcl_UniChar *unicode;
    int numChars;
{
    register Tcl_Obj *objPtr;
    register Tcl_DString *string;

    objPtr = Tcl_NewObj();
    objPtr->bytes = NULL;
    
    string = (Tcl_DString *) Tcl_Alloc(sizeof(Tcl_DString));
    Tcl_DStringInit(string);
    Tcl_DStringAppend(string, (char *) unicode, numChars*sizeof(Tcl_UniChar));

    objPtr->internalRep.otherValuePtr = (char *) string;
    objPtr->typePtr = &UnicodeObjType;
    return objPtr;
}

#endif





/*
 *--------------------------------------------------------------
 *
 * StateSetString --
 *
 *	Given its index, modify a lexer state's string.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The state is modified.
 *
 *--------------------------------------------------------------
 */

static void
StateSetString(lexer, n, string)
    TcLex_Lexer	*lexer;
    int		n;          /* Index of the state */
    Tcl_Obj	*string;    /* New string being lexed */
{
    TcLex_State	*statePtr = lexer->states[n];
    if (statePtr->inputBuffer.chars)
	Tcl_DecrRefCount(statePtr->inputBuffer.chars);
    statePtr->inputBuffer.chars = string;
    Tcl_IncrRefCount(statePtr->inputBuffer.chars);
}


/*
 *--------------------------------------------------------------
 *
 * StateGetString --
 *
 *	Given its index, modify a lexer state's string.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The state is modified.
 *
 *--------------------------------------------------------------
 */

static Char *
StateGetString(lexer, n, lengthPtr)
    TcLex_Lexer	*lexer;
    int		n;          /* Index of the state */
    int		*lengthPtr; /* Length of string */
{
    TcLex_State	*statePtr = lexer->states[n];

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
    return Tcl_GetStringFromObj(statePtr->currentBuffer->chars, lengthPtr);
#else
    if (lengthPtr)
	*lengthPtr = Tcl_GetCharLength(statePtr->currentBuffer->chars);
    return Tcl_GetUnicode(statePtr->currentBuffer->chars);
#endif
}


/*
 *--------------------------------------------------------------
 *
 * StateDelete --
 *
 *	Delete a state given its index.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	The lexer's <states> array is modified.
 *
 *--------------------------------------------------------------
 */

static void
StateDelete(lexer, n)
    TcLex_Lexer	*lexer;
    int		n;      /* Index of the state */
{
    TcLex_State	*statePtr = lexer->states[n];
    int		nb;

    /*
     * Free the state's structures
     */

    Tcl_Free((char*)statePtr->conditionsStack);
    Tcl_Free((char*)statePtr->bFailed);
    Tcl_DecrRefCount(statePtr->inputBuffer.chars);
    Tcl_Free((char*)statePtr->startIndices);
    Tcl_Free((char*)statePtr->endIndices);
    if (statePtr->pendingResult)
	Tcl_DecrRefCount(statePtr->pendingResult);

    lexer->states[n] = NULL;

    /*
     * Modify the states array: delete the upper empty elements
     */

    nb = lexer->nbStates;
    while (nb && !lexer->states[nb-1]) /* Search for nonempty elements */
	nb--;                            /* at the end of the array. */
    if (nb != lexer->nbStates) {       /* If any, shrink the array. */
	lexer->states = (TcLex_State **)Tcl_Realloc((char *)lexer->states, sizeof(TcLex_State *) * lexer->nbStates);
	lexer->nbStates = nb;
    }
}   



/*******************************************************
 *                                                     *
 *             Lexer management functions              *
 *                                                     *
 *******************************************************/


/*
 *--------------------------------------------------------------
 *
 * LexerDelete --
 *
 *	This procedure deletes a TcLex_Lexer structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tcl_Obj objects owned by the structure are freed.
 *
 *--------------------------------------------------------------
 */

void
LexerDelete(lexer)
    TcLex_Lexer	*lexer;
{
    int		i;
    
    /*
     * Free conditions
     */

    if (lexer->conditions) {
	for (i=0; i < lexer->nbInclusiveConditions+lexer->nbExclusiveConditions; i++) {
	    Tcl_DecrRefCount(lexer->conditions[i]);
	}
	Tcl_Free((char *)lexer->conditions);
    }
    
    /*
     * Free active rules
     */

    for (i=0; i < lexer->nbInclusiveConditions+lexer->nbExclusiveConditions+1; i++) {
	Tcl_Free((char *)lexer->activeRules[i]);
    }
    Tcl_Free((char *)lexer->activeRules);

    /*
     * Free result variable
     */

    if (lexer->resultVariable)  Tcl_DecrRefCount(lexer->resultVariable);

    /*
     * Free args
     */

    if (lexer->args) Tcl_DecrRefCount(lexer->args);

    /*
     * Free scripts
     */

    if (lexer->preScript)  Tcl_DecrRefCount(lexer->preScript);
    if (lexer->midScript)  Tcl_DecrRefCount(lexer->midScript);
    if (lexer->postScript) Tcl_DecrRefCount(lexer->postScript);

    /*
     * Free rules
     */

    if (lexer->rules) {
	for (i=0; i < lexer->nbRules; i++) {
	    RuleFree(lexer->rules+i);
	}
	Tcl_Free((char *)lexer->rules);
    }
    
    /*
     * Free states
     */

    if (lexer->states) {
	TcLex_State *statePtr;
	for (i=0; i < lexer->nbStates; i++) {
	    if (statePtr = lexer->states[i]) {
		Tcl_Free((char*)statePtr->conditionsStack);
		Tcl_Free((char*)statePtr->bFailed);
		Tcl_DecrRefCount(statePtr->inputBuffer.chars);
		Tcl_Free((char*)statePtr->startIndices);
		Tcl_Free((char*)statePtr->endIndices);
		if (statePtr->pendingResult)
		  Tcl_DecrRefCount(statePtr->pendingResult);
		
		Tcl_Free((char *)statePtr);
	    }
	}
	Tcl_Free((char *)lexer->states);
    }
    
    /*
     * Free lexer
     */

    Tcl_Free((char *)lexer);

    return;
}


/*
 *--------------------------------------------------------------
 *
 * LexerNew --
 *
 *	This procedure creates a new TcLex_Lexer structure.
 *
 * Results:
 *	A pointer to the newly allocated TcLex_Lexer, or NULL
 *	if failed.
 *
 * Side effects:
 *	Some Tcl_Obj's are shared by the new structure.
 *
 *--------------------------------------------------------------
 */

TcLex_Lexer *
LexerNew(interp, inclusiveConditions, exclusiveConditions, resultVariable,
	 preScript, midScript, postScript, args, flags, rulec, rulev)
    Tcl_Interp		*interp;
    Tcl_Obj		*inclusiveConditions;
    Tcl_Obj		*exclusiveConditions;
    Tcl_Obj		*resultVariable;
    Tcl_Obj		*preScript;
    Tcl_Obj		*midScript;
    Tcl_Obj		*postScript;
    Tcl_Obj		*args;
    int			flags;
    int			rulec;
    Tcl_Obj *CONST	*rulev;
{
    TcLex_Lexer		*lexer;
    int			i, j, k, l;
    int			nbRules;
    int			nbConds;
    int			firstScript;      /* Used to handle script    */
    Tcl_Obj		*firstConditions; /* and conditions shortcuts */
    int			cc;               /* Used to get   */
    Tcl_Obj		**cv;             /* list elements */
    char		ruleNb[32];       /* Rule number reported when an 
					   * error occurs */

    /*
     * Sanity check
     */
     
    /* Rule list must be a multiple of 4: <conditions> <regexp> <match vars> <script> */
    if (rulec % 4 != 0)
	/* Error */
	goto misformedRuleError;
    
    nbRules = rulec/4;

    /* Conditions and arguments must be valid lists */
    {
	Tcl_ObjType *listObjTypePtr = Tcl_GetObjType("list");

	if (   inclusiveConditions && Tcl_ConvertToType(interp, inclusiveConditions, listObjTypePtr) == TCL_ERROR
		|| exclusiveConditions && Tcl_ConvertToType(interp, exclusiveConditions, listObjTypePtr) == TCL_ERROR
		|| args                && Tcl_ConvertToType(interp, args,                listObjTypePtr) == TCL_ERROR)
	 /* Error */
	    goto listConvertError;
    }
    

    /*
     * Allocate lexer
     */

    lexer = (TcLex_Lexer *)Tcl_Alloc(sizeof(TcLex_Lexer));
    
    /*
     * Initialization
     */

    lexer->flags = flags;

    /*
     * Allocate conditions
     */

    lexer->conditions = NULL;

    /* Inclusive conditions */
    if (inclusiveConditions) {
	Tcl_ListObjGetElements(interp, inclusiveConditions, &cc, &cv);
	lexer->nbInclusiveConditions = cc;
	
	/* Allocate the conditions array... */
	lexer->conditions = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *) * cc);
	
	/* ... then fill it */
	for (k=0; k < cc; k++) {
	    lexer->conditions[k] = cv[k];
	    Tcl_IncrRefCount(cv[k]);
	}
    } else {
	lexer->nbInclusiveConditions = 0;
    }

    /* Exclusive conditions */
    if (exclusiveConditions) {
	Tcl_ListObjGetElements(interp, exclusiveConditions, &cc, &cv);
	lexer->nbExclusiveConditions = cc;
	
	/* (Re)allocate the conditions array... */
	if (lexer->conditions)
	    lexer->conditions = (Tcl_Obj **)Tcl_Realloc((char *)lexer->conditions, 
		                    sizeof(Tcl_Obj *) * (lexer->nbInclusiveConditions+cc));
	else
	    lexer->conditions = (Tcl_Obj **)Tcl_Alloc(sizeof(Tcl_Obj *) * cc);

	/* ... then fill it */
	for (k=0; k < cc; k++) {
	    lexer->conditions[k+lexer->nbInclusiveConditions] = cv[k];
	    Tcl_IncrRefCount(cv[k]);
	}
    } else {
	lexer->nbExclusiveConditions = 0;
    }

    /*
     * Initialize active conditions
     */

    nbConds = lexer->nbInclusiveConditions+lexer->nbExclusiveConditions+1;
    lexer->activeRules = (int **)Tcl_Alloc(sizeof(int *) * nbConds);
    for (i=0; i < nbConds; i++) {
	/* Pre-allocation, we'll shrink later */
	lexer->activeRules[i] = (int *)Tcl_Alloc(sizeof(int) * nbRules);
	for (j=0; j < nbRules; j++)
	    lexer->activeRules[i][j] = -1;
    }

    /*
     * Initialize result variable
     */

    if (resultVariable) {
	lexer->resultVariable = resultVariable;
	Tcl_IncrRefCount(resultVariable);
    } else {
	lexer->resultVariable = NULL;
    }

    /*
     * Initialize args
     */

    if (args) {
	lexer->args = args;
	Tcl_IncrRefCount(args);
    } else {
	lexer->args = NULL;
    }
    
    /*
     * Initialize scripts
     */

    if (preScript) {
	lexer->preScript = preScript;
	Tcl_IncrRefCount(preScript);
    } else {
	lexer->preScript = NULL;
    }
    if (midScript) {
	lexer->midScript = midScript;
	Tcl_IncrRefCount(midScript);
    } else {
	lexer->midScript = NULL;
    }
    if (postScript) {
	lexer->postScript = postScript;
	Tcl_IncrRefCount(postScript);
    } else {
	lexer->postScript = NULL;
    }

    /*
     * Initialize states information
     */

    lexer->nbStates = 0;
    lexer->states   = NULL;
    lexer->curState = -1;

    /* 
     * Allocate and initalize rules
     */

    lexer->rules = (TcLex_Rule *)Tcl_Alloc(sizeof(TcLex_Rule) * nbRules);
    lexer->nbRules = nbRules;
    for (i=0; i < nbRules; i++) {
	lexer->rules[i].conditionsIndices       = (int *)NULL;
	lexer->rules[i].conditionsIndicesLength = 0;
	lexer->rules[i].re                      = (Tcl_RegExp)NULL;
	lexer->rules[i].reObj                   = (Tcl_Obj *)NULL;
	lexer->rules[i].matchVars               = (Tcl_Obj *)NULL;
	lexer->rules[i].script                  = (Tcl_Obj *)NULL;
    }

    firstScript     = -1;   /* Used for "-" script shortcut */
    firstConditions = NULL; /* Used for "-" conditions shortcut */
    for (i=0,j=0; i < rulec; i+=4, j++) {
	Tcl_Obj    *conditions; /* Conditions list */

	/* 
	 * Store regexps
	 */

	if (RuleCompileRegexp(interp, &lexer->rules[j], rulev[i+1], flags) == TCL_ERROR)
	    goto regexpCompileError;

	/*
	 * Handle "-" conditions shortcut
	 */

	if (strcmp(Tcl_GetStringFromObj(rulev[i], NULL), "-") == 0) {
	    if (firstConditions == NULL)
		/* Error */
		goto missingFirstConditionError;
	    conditions = firstConditions;
	} else {
	    firstConditions = conditions = rulev[i];
	}

	if (Tcl_ListObjGetElements(interp, conditions, &cc, &cv) == TCL_ERROR)
	    /* Error */
	    goto conditionsConvertError;
	
	/* Allocate the conditions array... */
	lexer->rules[j].conditionsIndicesLength = cc;
	lexer->rules[j].conditionsIndices = (int *)Tcl_Alloc(sizeof(int) * cc);
	
	/* ... then fill it (with conditions indices) */
	for (k=0; k < cc; k++) {
	    int ci = ConditionIndex(lexer, Tcl_GetStringFromObj(cv[k], NULL));
	    if (   ci == -1
		&& strcmp(Tcl_GetStringFromObj(cv[k], NULL), "*") != 0)
		 /* Error, the condition doesn't exist and is not a wildcard */
		 goto unknownConditionError;
	    lexer->rules[j].conditionsIndices[k] = ci;
	}

	/*
	 * Match vars
	 */

	lexer->rules[j].matchVars = rulev[i+2];
	Tcl_IncrRefCount(rulev[i+2]);

	/*
	 * Handle "-" script shortcut
	 */

	if (strcmp(Tcl_GetStringFromObj(rulev[i+3], NULL), "-") == 0) {
	    if (firstScript == -1)
		firstScript = j;
	} else {
	    for (k=(firstScript == -1 ? j : firstScript); k <= j; k++) {
		lexer->rules[k].script = rulev[i+3];
		Tcl_IncrRefCount(rulev[i+3]);
	    }
	    firstScript = -1;
	}
    }
    
    if (firstScript != -1)
	/* Error, we used script shortcuts till the end without a script */
	goto missingScriptError;
    
    /*
     * Fill the active rules array
     */

    /* Scan all the rules */
    for (i=0; i < nbRules; i++) {
	char bActive;
	int cond;

	bActive = 0;

	if (lexer->rules[i].conditionsIndicesLength == 0) {
	    /* No conditions, only initial and inclusive conditions will be active */
	    for (k=0; k < lexer->nbInclusiveConditions+1; k++) {
		for (l=0; ; l++) {
		    if (lexer->activeRules[k][l] == -1) {
			lexer->activeRules[k][l] = i;
			break;
		    }
		}
	    }
	    continue;
	}

	for (j=0; j < lexer->rules[i].conditionsIndicesLength; j++) {
	    cond = lexer->rules[i].conditionsIndices[j];

	    if (cond == -1) { /* Match all conditions */
		/* Add the rule to all conditions */
		for (k=0; k < nbConds; k++) {
		    for (l=0; ; l++) {
			if (   lexer->activeRules[k][l] == i     /* Maybe the rule is already there */
			    || lexer->activeRules[k][l] == -1) {
			    lexer->activeRules[k][l] = i;
			    break;
			}
		  }
		}
		break;
	    }

	    /* Add the rule to the condition <cond> */
	    for (l=0; ; l++) {
		if (   lexer->activeRules[cond][l] == i     /* Maybe the rule is already there */
		    || lexer->activeRules[cond][l] == -1) {
		    lexer->activeRules[cond][l] = i;
		    break;
		}
	    }
	}
    }

    /*
     * Shrink the activeRules sub-arrays
     */

    for (i=0; i < nbConds; i++) {
	for (j=0; j < nbRules && lexer->activeRules[i][j] != -1; j++);
	if (j < nbRules-1)
	    /* Shrink the array */
	    lexer->activeRules[i] = (int *)Tcl_Realloc((char *)lexer->activeRules[i],
						sizeof(int) * (j+1));
    }

    return lexer;


    /*
     * Error handling
     */

    /* These errors occur at the beginning and need no cleanup */

listConvertError:
    /* Error string is already set by Tcl */
    return NULL;

misformedRuleError:
    Tcl_SetResult(interp, "misformed rule: should be \"conditions regexp matchvars script\"", TCL_STATIC);
    return NULL;


    /* These error need cleanup */

regexpCompileError:
    sprintf(ruleNb, "%d", j);
    Tcl_AddErrorInfo(interp, "\n    (rule ");
    Tcl_AddErrorInfo(interp, ruleNb);
    Tcl_AddErrorInfo(interp, ")");
    goto cleanup;

missingFirstConditionError:
    Tcl_SetResult(interp, "invalid condition \"-\"", TCL_STATIC);
    sprintf(ruleNb, "%d", j);
    Tcl_AddErrorInfo(interp, "\n    (rule ");
    Tcl_AddErrorInfo(interp, ruleNb);
    Tcl_AddErrorInfo(interp, ")");
    goto cleanup;

conditionsConvertError:
    /* Error string is already set by Tcl, just add error info */
    sprintf(ruleNb, "%d", j);
    Tcl_AddErrorInfo(interp, "\n    (rule ");
    Tcl_AddErrorInfo(interp, ruleNb);
    Tcl_AddErrorInfo(interp, ")");
    goto cleanup;

unknownConditionError:
    Tcl_AppendResult(interp, "unknown condition \"",
		           Tcl_GetStringFromObj(cv[k], NULL), "\"", (char *) NULL);
    sprintf(ruleNb, "%d", j);
    Tcl_AddErrorInfo(interp, "\n    (rule ");
    Tcl_AddErrorInfo(interp, ruleNb);
    Tcl_AddErrorInfo(interp, ")");
    goto cleanup;

missingScriptError:
    Tcl_SetResult(interp, "missing script for last rule", TCL_STATIC);
    goto cleanup;

cleanup:
    LexerDelete(lexer);
    return NULL;
}


/*
 *--------------------------------------------------------------
 *
 * LexerCmdDeleteProc --
 *
 *	This procedure is invoked to delete the Tcl commands
 *	created by "lexer".
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The Tcl command and all its structures are freed.
 *
 *--------------------------------------------------------------
 */

void
LexerCmdDeleteProc(clientData)
    ClientData	clientData;
{
    TcLex_Lexer	*lexer = (TcLex_Lexer*)clientData;
    if (lexer->nbStates != 0) {
	/* Lexer is active */
	/* Nothing to do, all is handled by Tcl. Life is beautiful :-)) */
	return;
    }

    /*
     * Just delete the lexer structure
     */

    LexerDelete((TcLex_Lexer *)clientData);
}   


/*
 *--------------------------------------------------------------
 *
 * LexerCurrentObjCmd --
 *
 *	This procedure is invoked to process the "lexer current" 
 *	Tcl subcommand. See the user documentation for details on
 *	what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
LexerCurrentObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*currentLexer;

    /*
     * Sanity check
     */

    if (objc != 2)
	goto numberArgsError;
    
    currentLexer = LexerGetCurrent(interp);
    if (currentLexer) {
	if (((Command*)currentLexer->command)->flags==CMD_IS_DELETED)
	    /* If command has been deleted, the token is meaningless. Return no result */
	    Tcl_ResetResult(interp);
	else
	    Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_GetCommandName(interp, currentLexer->command), -1));
	return TCL_OK;
    } else {
	Tcl_SetResult(interp, "no active lexer", TCL_STATIC);
	return TCL_ERROR;
    }

    
    /*
     * Error handling
     */

numberArgsError:
    Tcl_WrongNumArgs(interp, 2, objv, NULL);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerCreateObjCmd --
 *
 *	This procedure is invoked to process the "lexer create" 
 *	Tcl subcommand. See the user documentation for details on
 *	what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates a new Tcl command in the current interpreter,
 *	replacing any existing one. Allocate related structures.
 *
 *--------------------------------------------------------------
 */

int
LexerCreateObjCmd(clientData, interp, objc, objv, start)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
    int		start;
{
    static char * options[] = {
	"-inclusiveconditions",		"-ic",
	"-exclusiveconditions",		"-ec",
	"-resultvariable",
	"-prescript",	    "-midscript",	"-postscript",
	"-args",	    "-lines",		"-nocase",	"-longest", 
	"--",
	NULL
    };
    static enum {
	OPTION_INCLUSIVECONDITIONS,	OPTION_IC,
	OPTION_EXCLUSIVECONDITIONS,	OPTION_EC,
	OPTION_RESULTVARIABLE,
	OPTION_PRESCRIPT,   OPTION_MIDSCRIPT,	OPTION_POSTSCRIPT,
	OPTION_ARGS,	    OPTION_LINES,	OPTION_NOCASE,	OPTION_LONGEST,
	OPTION_END
    } optionIndex;
    int		i;
    char	*option;     /*                           */
    int		length;      /* Used for argument parsing */
    TcLex_Lexer	*lexer;
    Tcl_Obj	*lexerName;
    char	*name;
    Tcl_Obj	*inclusiveConditions = NULL,
		*exclusiveConditions = NULL;
    Tcl_Obj	*resultVariable = NULL;
    Tcl_Obj	*preScript  = NULL,
		*midScript  = NULL,
		*postScript = NULL,
		*args       = NULL;
    int		flags = 0;
    int		rulec;
    Tcl_Obj	*CONST *rulev;
    

    /*
     * Sanity check
     */

    if (objc <= start+1)
	goto numberArgsError;


    /*
     * Initialization
     */
    
    lexerName = objv[start];
    name = Tcl_GetStringFromObj(lexerName, NULL);


    /*
     * Argument parsing
     */  

    i = start+1;

    for (;;) {
	if (i >= objc)
	    goto numberArgsError;
	
	option = Tcl_GetStringFromObj(objv[i], &length);

	/* 
	 * Check for end of switches
	 */

	if (length < 2 || option[0] != '-' || option[1] == '-') {
	    if (option[1] == '-')
		i++;
	    break;
	}

	/*
	 * Get the switch
	 */

	if (Tcl_GetIndexFromObj(interp, objv[i], options, "switch", 0, &optionIndex) == TCL_ERROR)
	    return TCL_ERROR;

	switch (optionIndex) {
	    case OPTION_INCLUSIVECONDITIONS:
	    case OPTION_IC:
		if (++i >= objc)
		  goto optionValueMissingError;

		inclusiveConditions = objv[i++];
		Tcl_IncrRefCount(inclusiveConditions);
		break;

	    case OPTION_EXCLUSIVECONDITIONS:
	    case OPTION_EC:
		if (++i >= objc)
		  goto optionValueMissingError;

		exclusiveConditions = objv[i++];
		Tcl_IncrRefCount(exclusiveConditions);
		break;

	    case OPTION_RESULTVARIABLE:
		if (++i >= objc)
		  goto optionValueMissingError;

		resultVariable = objv[i++];
		Tcl_IncrRefCount(resultVariable);
		break;

	    case OPTION_PRESCRIPT:
		if (++i >= objc)
		  goto optionValueMissingError;

		preScript = objv[i++];
		Tcl_IncrRefCount(preScript);
		break;

	    case OPTION_MIDSCRIPT:
		if (++i >= objc)
		  goto optionValueMissingError;

		midScript = objv[i++];
		Tcl_IncrRefCount(midScript);
		break;

	    case OPTION_POSTSCRIPT:
		if (++i >= objc)
		  goto optionValueMissingError;

		postScript = objv[i++];
		Tcl_IncrRefCount(postScript);
		break;

	    case OPTION_ARGS:
		if (++i >= objc)
		  goto optionValueMissingError;

		args = objv[i++];
		Tcl_IncrRefCount(args);
		break;

	    case OPTION_LINES:
		flags |= LEXER_FLAG_LINES;
		i++;
		break;

	    case OPTION_NOCASE:
		flags |= LEXER_FLAG_NOCASE;
		i++;
		break;

	    case OPTION_LONGEST:
		flags |= LEXER_FLAG_LONGEST;
		i++;
		break;
	}
    }

    /*
     * There should be arguments remaining (rules)
     */

    if (i >= objc)
	goto numberArgsError;

    /*
     * We can have rules as a single list or as the remaining arguments
     */

    if (objc-i == 1) {
	/* Single list */
	if (Tcl_ListObjGetElements(interp, objv[i], &rulec, (Tcl_Obj ***)&rulev) == TCL_ERROR)
	    goto rulesListConvertError;
    } else {
	rulec = objc-i;
	rulev = objv+i;
    }
    
    /*
     * Create the lexer
     */

    if ((lexer = LexerNew(interp, inclusiveConditions, exclusiveConditions,
			  resultVariable, preScript, midScript, postScript,
			  args, flags, 
			  rulec, rulev)) == (VOID *)NULL)
	return TCL_ERROR;
    
    /*
     * Create the lexer Tcl command
     */

    if (strncmp(name, "::", 2) != 0) { /* Lexer isn't created in the global namespace */
	lexerName = Tcl_NewStringObj(Tcl_GetCurrentNamespace(interp)->fullName, -1);
	Tcl_AppendStringsToObj(lexerName, "::", name, NULL);
	name = Tcl_GetStringFromObj(lexerName, NULL);
    }
    Tcl_IncrRefCount(lexerName);
    lexer->command = Tcl_CreateObjCommand(interp, name,
			(Tcl_ObjCmdProc *)LexerObjCmd,
			(ClientData)lexer,
			(Tcl_CmdDeleteProc *)LexerCmdDeleteProc);
    Tcl_DecrRefCount(lexerName);

    Tcl_ResetResult(interp);

    return TCL_OK;


    /*
     * Error handling
     */

rulesListConvertError:
    /* Error string is already set by Tcl */
    return TCL_ERROR;

optionValueMissingError:
    Tcl_AppendResult(interp, "value for \"", option, "\" missing", NULL);
    return TCL_ERROR;

numberArgsError:
    Tcl_WrongNumArgs(interp, start, objv, "name ?switches? rule ... ?rule?");
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerMainObjCmd --
 *
 *	This procedure is invoked to process the "lexer" Tcl
 *	command. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on subcommand.
 *
 *--------------------------------------------------------------
 */

int
LexerMainObjCmd(clientData, interp, objc, objv)
    ClientData     clientData;
    Tcl_Interp     *interp;
    int            objc;
    Tcl_Obj *CONST objv[];
{
    static char * options[] = {
	"create",	"current", 
	NULL
    };
    static enum {
	OPTION_CREATE,	OPTION_CURRENT
    } optionIndex;
    static Tcl_ObjCmdProc * commands[] = {
	(Tcl_ObjCmdProc *)NULL,
	(Tcl_ObjCmdProc *)LexerCurrentObjCmd
    };


    /*
     * Sanity check
     */

    if (objc < 2)
	goto numberArgsError;

     
    /*
     * Get the switch
     */

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0, &optionIndex) == TCL_ERROR) {
	/*
	 * For now, an unknown subcommand is interpreted as the name of a new lexer
	 * to create. This is for compatibility issues, and is likely to change when
	 * more subcommands will be added in a future major version.
	 */
	Tcl_ResetResult(interp);
	return LexerCreateObjCmd(clientData, interp, objc, objv, 1);
    }


    if (optionIndex == OPTION_CREATE)
	return LexerCreateObjCmd(clientData, interp, objc, objv, 2);

    return (commands[optionIndex])(clientData, interp, objc, objv);


    /*
     * Error handling
     */

numberArgsError:
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
    return TCL_ERROR;
}




/*******************************************************
 *                                                     *
 *             Lexer evaluation functions              *
 *                                                     *
 *******************************************************/

/*
 *--------------------------------------------------------------
 *
 * RuleTry --
 *
 *	This procedure tests if the given rule match the string at
 *	the current index.
 *
 * Results:
 *	An integer code:
 *	    LEXER_RULETRY_ERROR   : an error has occured
 *	    LEXER_RULETRY_NOMATCH : the rule didn't match
 *	    LEXER_RULETRY_MATCHED : the rule matched
 *	    LEXER_RULETRY_OVERRUN : the rule didn't match but the
 *				    string was overrun
 *
 * Side effects:
 *	Modifies the state's matched indices.
 *
 *--------------------------------------------------------------
 */

#define LEXER_RULETRY_ERROR      -1
#define LEXER_RULETRY_NOMATCH     0
#define LEXER_RULETRY_MATCHED     1
#define LEXER_RULETRY_OVERRUN     2

int
RuleTry(interp, lexer, iRule, bCheckOverrun, pMinLength)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
    int		iRule;         /* Index of the rule to try */
    int		bCheckOverrun; /* Whether to check for string overrun */
			       /* (eg. for incremental processing) */
    int		*pMinLength;   /* if -longest, minimum # chara to match */
{
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		len;
    Char	*str = StateGetString(lexer, lexer->curState, &len);
    int		bol;
    TcLex_Rule  *rule = &lexer->rules[iRule];
    int		i, s, e;
    int		overrun;
    
    if (   (lexer->flags & LEXER_FLAG_LINES)
	&& (str[statePtr->currentBuffer->index-1] == '\n'))
	bol = statePtr->currentBuffer->index;
    else
	bol = 0;

    switch (RuleExec(interp, lexer, rule,
		statePtr->currentBuffer->chars, statePtr->currentBuffer->index, bol,
		&overrun)) {
	case -1:
	    return LEXER_RULETRY_ERROR;

	case 0:
	    if (bCheckOverrun && overrun)
		return LEXER_RULETRY_OVERRUN;
	    return LEXER_RULETRY_NOMATCH;
    }

    if (bCheckOverrun && overrun)
	return LEXER_RULETRY_OVERRUN;
    
    /*
     * Get info about the matched string
     */

    RuleGetRange(interp, lexer, rule, statePtr->currentBuffer->chars, statePtr->currentBuffer->index, 0, &s, &e);
    if (   (lexer->flags & LEXER_FLAG_LONGEST)
	&& (e-s+1 <= *pMinLength))
	/* We want a longer match */
	return LEXER_RULETRY_NOMATCH;

    if (e < s) { /* Empty string matched */
	if (   iRule == statePtr->lastMatched
	    || statePtr->currentBuffer->bMatchedEmpty)
	    /* The previous rule can't match an empty string again.
	     * Also we can't match two consecutive empty strings
	     * else we'll run into an infinite loop */
	    return LEXER_RULETRY_NOMATCH;

	statePtr->currentBuffer->bMatchedEmpty = 1;
    } else {
	statePtr->currentBuffer->bMatchedEmpty = 0;
    }
    *pMinLength = e-s+1;

    /*
     * Get matched string and substrings' indices
     */

    statePtr->nbRanges     = lexer->rules[iRule].nbRanges;
    statePtr->startIndices = (int *)Tcl_Realloc((char *)statePtr->startIndices, statePtr->nbRanges * sizeof(int));
    statePtr->endIndices   = (int *)Tcl_Realloc((char *)statePtr->endIndices, statePtr->nbRanges * sizeof(int));

    for (i=0; i < statePtr->nbRanges; i++) {
	RuleGetRange(interp, lexer, rule, statePtr->currentBuffer->chars, statePtr->currentBuffer->index, i, &statePtr->startIndices[i], &statePtr->endIndices[i]);
    }

    statePtr->currentBuffer->nextIndex = statePtr->endIndices[0]+1;

    /* Store the rule's index */
    statePtr->lastMatched = iRule;

    return LEXER_RULETRY_MATCHED;
}


/*
 *--------------------------------------------------------------
 *
 * RuleEvalDefault --
 *
 *	This procedure evaluates the default rule.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Consumes one char from the lexed string.
 *
 *--------------------------------------------------------------
 */

void
RuleEvalDefault(interp, lexer)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
{
    TcLex_State	*statePtr = lexer->states[lexer->curState];

    /*
     * Consume one char
     */

    statePtr->currentBuffer->bMatchedEmpty = 0; /* Non-empty match */

    statePtr->currentBuffer->nextIndex = statePtr->currentBuffer->index+1;

    statePtr->nbRanges     = 1;
    statePtr->startIndices = (int *)Tcl_Realloc((char*)statePtr->startIndices, statePtr->nbRanges * sizeof(int));
    statePtr->endIndices   = (int *)Tcl_Realloc((char*)statePtr->endIndices, statePtr->nbRanges * sizeof(int));

    statePtr->startIndices[0] = statePtr->currentBuffer->index;
    statePtr->endIndices[0]   = statePtr->currentBuffer->nextIndex-1;

    /* Store -1 as the rule's index */
    statePtr->lastMatched = -1;
}


/*
 *--------------------------------------------------------------
 *
 * LexerMatchVars --
 *
 *	This procedure is used to set a series of variables to
 *	the matched subranges values.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The specified variables get modified.
 *
 *--------------------------------------------------------------
 */

int
LexerMatchVars(interp, lexer, bIndices, matchc, matchv)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
    int		bIndices;
    int		matchc;
    Tcl_Obj	*CONST matchv[];
{
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		i, result;
    Char	*str;

    if (!bIndices) {
#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
	str = Tcl_GetStringFromObj(statePtr->currentBuffer->chars, NULL)+statePtr->startIndices[0];
#else
	str = Tcl_GetUnicode(statePtr->currentBuffer->chars)+statePtr->startIndices[0];
#endif
    }

    /* 
     * Set every matchvar to its value
     */

    result = TCL_OK;
    for (i=0; i < matchc; i++) {
	Tcl_Obj *val;
	if (bIndices) {
	    /* We want indices */
	    Tcl_Obj *res[2];
	    if (i < statePtr->nbRanges) {
		res[0] = Tcl_NewIntObj(statePtr->startIndices[i]);
		res[1] = Tcl_NewIntObj(statePtr->endIndices[i]);
	    } else {
		res[0] = Tcl_NewIntObj(-1);
		res[1] = Tcl_NewIntObj(-1);
	    }
	    val = Tcl_NewListObj(2, res);
	} else {        
	    /* We want strings */
	    if (i >= statePtr->nbRanges || statePtr->startIndices[i] < 0) {
		/* Substring not matched, return empty string */
		val = Tcl_NewObj();
	    } else {
		Char *s = str+statePtr->startIndices[i]-statePtr->startIndices[0];
		Char *e = str+statePtr->endIndices[i]-statePtr->startIndices[0]+1;
#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
		val = Tcl_NewStringObj(s, e-s);
#else
		val = Tcl_NewUnicodeObj(s, e-s);
#endif
	    }
	}
	if (Tcl_ObjSetVar2(interp, matchv[i], NULL, val, TCL_PARSE_PART1 | TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }

    return result;
}

/*
 *--------------------------------------------------------------
 *
 * LexerEval --
 *
 *	This procedure is used to eval the lexer with a given
 *	string. It can be used on a whole or partial string,
 *	depending on the "op" argument.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
BufferNotStarving(buffer)
    TcLex_Buffer *buffer;
{
    int length;

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
    Tcl_GetStringFromObj(buffer->chars, &length);
#else
    length = Tcl_GetCharLength(buffer->chars);
#endif
    return (buffer->index <= length);
}

int
BufferAtEnd(buffer)
    TcLex_Buffer *buffer;
{
    int length;

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
    Tcl_GetStringFromObj(buffer->chars, &length);
#else
    length = Tcl_GetCharLength(buffer->chars);
#endif
    return (buffer->index >= length);
}

int
LexerEval(interp, lexer, bCheckOverrun)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
    int		bCheckOverrun; /* Whether to check for string overrun */
		                        /* (for incremental processing) */
{
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		result;
    int		nbRules = lexer->nbRules;
    int		i;
    int		iRule, iRule2;
    char	ruleNb[32];    /* Rule number reported when an error occurs */
    int		matchc;        /* Match vars */
    Tcl_Obj	**matchv;      /*            */
    int		cond;          /* Current condition index */
    int		minLength;     /* Minimum string length to match */
    int		matchedRule;

    /*
     * Initialization
     */

    result = TCL_OK; /* Variable holding the result */

    /*
     * Get info about the conditions
     */

    if (statePtr->conditionsStackLength == 0) { /* initial state */
	cond = 0;
    } else {
	cond = statePtr->conditionsStack[statePtr->conditionsStackLength-1];
    }


    /*
     * Main loop
     */

    while (BufferNotStarving(statePtr->currentBuffer)) {
	minLength = -1;
	matchedRule = -1;

	/*
	 * Look for the first matching rule
	 */

	for (iRule2 = 0; iRule2 < nbRules; iRule2++) {
	    int matchRes;

	    /*
	     * Compute the real rule number
	     */

	    iRule = lexer->activeRules[cond][iRule2];
	    if (iRule == -1) /* No more active rules */
		break;
	    
	    /*
	     * If the rule is known to fail (eg. if it has been rejected
	     * or already tested), then continue
	     */

	    if (statePtr->bFailed[iRule])
		continue;

	    /*
	     * Try to match the string
	     */

	    matchRes = RuleTry(interp, lexer, iRule, bCheckOverrun, &minLength);

	    if (matchRes == LEXER_RULETRY_ERROR) {
		/* Error, stop there */
		result = TCL_ERROR;
		goto cleanup;
	    }
	    
	    if (matchRes == LEXER_RULETRY_NOMATCH) { /* No match */
		/* Rule failed, continue */
		statePtr->bFailed[iRule] = 1;
		continue;
	    }

	    if (matchRes == LEXER_RULETRY_OVERRUN) { /* String overrun */
		/* Just return without consuming chars */
		result = TCL_OK;
		goto cleanup;
	    }

	    matchedRule = iRule;
	    if (!(lexer->flags & LEXER_FLAG_LONGEST))
		/* If we don't want the longest match, we take
		   the first: just stop there. */
		break;
	}

	if (matchedRule == -1) { /* No rule matched */
	    /* We reached the end of the buffer */
	    if (BufferAtEnd(statePtr->currentBuffer)) {
		result = TCL_OK;
		goto cleanup;
	    }

	    /* Default rule */
	    statePtr->bRejected = 0;
	    RuleEvalDefault(interp, lexer);

	} else {
	    /*
	     * Set matchvars
	     */

	    Tcl_ListObjGetElements(NULL, lexer->rules[matchedRule].matchVars, &matchc, &matchv);
	    if (LexerMatchVars(interp, lexer, 0, matchc, matchv) == TCL_ERROR) {
		result = TCL_ERROR;
		goto cleanup;
	    }

	    /*
	     * Eval the rule's script
	     */

	    statePtr->bRejected = 0;
	    switch (Tcl_EvalObj(interp, lexer->rules[matchedRule].script)) {
		case TCL_ERROR:
		  goto scriptEvalError;

		case TCL_RETURN:
		  result = TCL_RETURN;
		  goto cleanup;
	    }

	    /*
	     * Conditions may have changed, update info
	     */

	    if (statePtr->conditionsStackLength == 0) { /* initial state */
		cond = 0;
	    } else {
		cond = statePtr->conditionsStack[statePtr->conditionsStackLength-1];
	    }

	    /* We reached the end of the buffer */
	    if (BufferAtEnd(statePtr->currentBuffer)) {
		result = TCL_OK;
		goto cleanup;
	    }

	}

	if (statePtr->bRejected) { /* If the rule has been rejected */
	    statePtr->bFailed[matchedRule] = 1;
	    statePtr->currentBuffer->nextIndex = statePtr->currentBuffer->index;
	} else {
	    /* Clear the rule failure indicators */
	    for (i=0; i<nbRules; i++) statePtr->bFailed[i] = 0;

	    /* Consume the matched string */
	    statePtr->currentBuffer->index = statePtr->currentBuffer->nextIndex;
	}
    }

    goto cleanup;


    /*
     * Error handling
     */

scriptEvalError:
    /* Error string is already set by Tcl, just add error info */
    sprintf(ruleNb, "%d", iRule);
    Tcl_AddErrorInfo(interp, "\n    (rule ");
    Tcl_AddErrorInfo(interp, ruleNb);
    Tcl_AddErrorInfo(interp, ")");
    result = TCL_ERROR;


cleanup:
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * LexerEvalObjCmd --
 *
 *	This procedure is invoked to process the "eval", "start",
 *	"continue" and "finish" subcommands for lexers. See the
 *	user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int LexerEvalObjCmd(interp, lexer, objc, objv, op)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
    int		objc;
    Tcl_Obj	*CONST objv[];
    int		op;
{
    TcLex_Lexer	*oldLexer;
    int		result;
    int		oldState;
    int		curState;
    CallFrame	cf;       /* Used to open a */
    Proc	proc;     /* new call frame */
    Tcl_CmdInfo	cmdInfo;  /* Used to get current namespace */
    Tcl_Obj	*command, *token, *string;
    int		argc = 0;
    Tcl_Obj	*CONST *argv = NULL;


    /*
     * Sanity check
     */

    switch (op) {
	case LEXER_EVAL:
	    if (objc < 3)
		goto numberArgsErrorEval;
	    token  = NULL;
	    string = objv[2];
	    argc = objc-3;
	    break;

	case LEXER_START:
	    if (objc < 4)
		goto numberArgsErrorStart;
	    token  = objv[2];
	    string = objv[3];
	    argc = objc-4;
	    break;

	case LEXER_CONTINUE:
	    if (objc != 4)
		goto numberArgsErrorContinue;
	    token  = objv[2];
	    string = objv[3];
	    break;

	case LEXER_FINISH:
	    if (objc != 4)
		goto numberArgsErrorFinish;
	    token  = objv[2];
	    string = objv[3];
	    break;
    }
    command = objv[0];

    switch (op) {
	case LEXER_CONTINUE:
	case LEXER_FINISH:
	    if (   Tcl_GetIntFromObj(NULL, token, &curState) == TCL_ERROR
		|| curState >= lexer->nbStates
		|| lexer->states[curState] == NULL)
		/* The token (ie state number) is invalid */
		goto badTokenError;

	    /*
	     * If there is a pending result, return now
	     */

	    if (lexer->states[curState]->pendingResult) {
		Tcl_SetObjResult(interp, lexer->states[curState]->pendingResult);
		return TCL_OK;
	    }
    }

    /*
     * Set the current lexer to this one.
     */

    oldLexer = LexerGetCurrent(interp);
    LexerSetCurrent(interp, lexer);

    /*
     * Open a new call frame
     */

    Tcl_GetCommandInfo(interp, Tcl_GetStringFromObj(command, NULL), &cmdInfo);
    if (Tcl_PushCallFrame(interp, (Tcl_CallFrame *)&cf, cmdInfo.namespacePtr, 1) == TCL_ERROR)
	return TCL_ERROR;

    proc.numCompiledLocals = 0;
    proc.firstLocalPtr = 0;
    proc.lastLocalPtr = 0;
    cf.procPtr = &proc;
    cf.objc = objc;
    cf.objv = objv;


    /*
     * Save the old state number
     */

    oldState = lexer->curState;

    /*
     * Initialize the state 
     */

    switch (op) {
	case LEXER_EVAL:
	case LEXER_START:
	    lexer->curState = curState = StateNew(lexer, string);
	    break;

	case LEXER_CONTINUE:
	case LEXER_FINISH:
	    lexer->curState = curState;
	    StateSetString(lexer, curState, string); /* Update the string */

	    /*
	     * Restore old local variables
	     */

	    cf.varTablePtr = lexer->states[curState]->varTablePtr;
    }


    /*
     * Initialize arguments
     */

    if (   lexer->args
	    && (op == LEXER_EVAL || op == LEXER_START)) {
	int i, ac;
	Tcl_Obj **av;

	argv = objv+objc-argc;

	Tcl_ListObjGetElements(interp, lexer->args, &ac, &av);
	if (argc > ac) {
	    Tcl_AppendResult(interp, "called lexer \"", Tcl_GetStringFromObj(command, NULL),
			"\" with too many arguments", NULL);
	    result = TCL_ERROR;
	    goto cleanup;
	}

	for (i=0; i<ac; i++) {
	    int c;
	    Tcl_Obj **v;

	    Tcl_ListObjGetElements(interp, av[i], &c, &v);
	    if (i < argc) { /* Value is specified */
		Tcl_ObjSetVar2(interp, v[0], NULL, argv[i], 0);
	    } else {
		if (c <= 1) { /* No default value */
		  Tcl_AppendResult(interp, "no value given for parameter\"", Tcl_GetStringFromObj(v[0], NULL),
			"\" to \"", Tcl_GetStringFromObj(command, NULL), "\"", NULL);
		  result = TCL_ERROR;
		  goto cleanup;
		}
		Tcl_ObjSetVar2(interp, v[0], NULL, v[1], 0);
	    }
	}
    } else {
	if (argc != 0) {
	    Tcl_WrongNumArgs(interp, 2, objv, "string");
	    result = TCL_ERROR;
	    goto cleanup;
	}
    }


    /*
     * Pre-script
     */

    if ((op == LEXER_EVAL || op == LEXER_START) && lexer->preScript) 
    switch (Tcl_EvalObj(interp, lexer->preScript)) {
	case TCL_ERROR:
	    result = TCL_ERROR;
	    goto cleanup;

	case TCL_RETURN:
	    result = TCL_OK;

	    if (op == LEXER_START || op == LEXER_CONTINUE) {
		/*
		 * Save pending result, we will return it with a "finish" command
		 */

		lexer->states[curState]->pendingResult = Tcl_GetObjResult(interp);
		Tcl_IncrRefCount(lexer->states[curState]->pendingResult);
	    }
	    goto cleanup;

	default:
	    result = TCL_OK;
    }

    /*
     * Eval the lexer
     */

    result = LexerEval(interp, lexer, (op == LEXER_START || op == LEXER_CONTINUE));

    switch (result) {
	case TCL_ERROR:
	    result = TCL_ERROR;
	    goto cleanup;

	case TCL_RETURN:
	    result = TCL_OK;

	    if (op == LEXER_START || op == LEXER_CONTINUE) {
		/*
		 * Save pending result, we will return it with a "finish" command
		 */

		lexer->states[curState]->pendingResult = Tcl_GetObjResult(interp);
		Tcl_IncrRefCount(lexer->states[curState]->pendingResult);
	    }
	    goto cleanup;

	default:
	    result = TCL_OK;
    }

    /*
     * Mid-script
     */

    if ((op == LEXER_START || op == LEXER_CONTINUE) && lexer->midScript) 
    switch (Tcl_EvalObj(interp, lexer->midScript)) {
	case TCL_ERROR:
	    result = TCL_ERROR;
	    goto cleanup;

	case TCL_RETURN:
	    result = TCL_OK;
	    goto cleanup;

	default:
	    result = TCL_OK;
    }

    /*
     * Post-script
     */

    if ((op == LEXER_EVAL || op == LEXER_FINISH) && lexer->postScript) 
    switch (Tcl_EvalObj(interp, lexer->postScript)) {
	case TCL_ERROR:
	    result = TCL_ERROR;
	    goto cleanup;

	case TCL_RETURN:
	    result = TCL_OK;
	    goto cleanup;

	default:
	    result = TCL_OK;
    }

    /*
     * If no result, we use the "-resultvariable" variable content as the result
     */

    if (lexer->resultVariable) {
	Tcl_Obj *res;
	
	if ((res = Tcl_ObjGetVar2(interp, lexer->resultVariable, NULL, TCL_PARSE_PART1 | TCL_LEAVE_ERR_MSG)) == NULL) {
	    result = TCL_ERROR;
	    goto cleanup;
	}
	Tcl_SetObjResult(interp, res);
    }

cleanup:

    /*
     * Restore the old state and delete current
     */

    lexer->curState = oldState;


    switch (op) {
	case LEXER_EVAL:
	case LEXER_FINISH:
	    /*
	     * Delete current state
	     */

	    StateDelete(lexer, curState);
	    break;

	case LEXER_START:
	case LEXER_CONTINUE:
	    if (result == TCL_ERROR) {
		/* An error interrupts the processing */
		StateDelete(lexer, curState);
		break;
	    }

	    /*
	     * Save local variables for continuation
	     */

	    if (result == TCL_OK) {
		lexer->states[curState]->varTablePtr = cf.varTablePtr;
		cf.varTablePtr = NULL;
	    }
    }

    /*
     * Close the call frame
     */

    Tcl_PopCallFrame(interp);

    /*
     * Restore the old current lexer.
     */

    LexerSetCurrent(interp, oldLexer);

    switch (op) {
	case LEXER_START:
	    /*
	     * Set the token variable to the state index for continuation
	     */

	    if (result == TCL_OK) {
		Tcl_Obj *res = Tcl_GetObjResult(interp); /* Save the current result */
		Tcl_IncrRefCount(res);

		/* This sets an error message in the result */
		if (Tcl_ObjSetVar2(interp, token, NULL, Tcl_NewIntObj(curState),
			TCL_PARSE_PART1 | TCL_LEAVE_ERR_MSG) == NULL) {
		    result = TCL_ERROR;
		} else {
		    /* Restore the old result */
		    Tcl_SetObjResult(interp, res);
		    Tcl_DecrRefCount(res);
		}
	    }
    }

    return result;


    /*
     * Error handling
     */

numberArgsErrorEval:
    Tcl_WrongNumArgs(interp, 2, objv, "string");
    return TCL_ERROR;

numberArgsErrorStart:
    Tcl_WrongNumArgs(interp, 2, objv, "tokenname string");
    return TCL_ERROR;

numberArgsErrorContinue:
numberArgsErrorFinish:
    Tcl_WrongNumArgs(interp, 2, objv, "token string");
    return TCL_ERROR;

badTokenError:
    Tcl_AppendResult(interp, "bad token: \"", Tcl_GetStringFromObj(token, NULL), "\"", NULL);
    return TCL_ERROR;
}


/*******************************************************
 *                                                     *
 *             Lexer state info functions              *
 *                                                     *
 *******************************************************/

/*
 *--------------------------------------------------------------
 *
 * LexerBeginObjCmd --
 *
 *	Push a condition on the current state's condition stack.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The conditions stack of the current state is modified.
 *
 *--------------------------------------------------------------
 */

int
LexerBeginObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		ci; /* Index of the condition */

    /* 
     * Sanity check
     */

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "condition");
	return TCL_ERROR;
    }
    
    if ((ci = ConditionIndex(lexer, Tcl_GetStringFromObj(objv[2], NULL))) < 0)
	/* Error */
	goto unknownConditionError;

    /*
     * Add the new condition to the stack: allocate or grow the array
     */

    if (ci == 0) {
	/* 
	 * Initial condition
	 * Empty the whole conditions stack, that way "<lexer> begin initial"
	 * will reset the lexer to the original condition state.
	 */
	statePtr->conditionsStackLength = 0;
	Tcl_Free((char *) statePtr->conditionsStack);
	statePtr->conditionsStack = NULL;
	return TCL_OK;
    }

    statePtr->conditionsStackLength++;

    if (statePtr->conditionsStack)
	statePtr->conditionsStack
	    = (int *)Tcl_Realloc((char *)statePtr->conditionsStack,
		                   sizeof(int) * statePtr->conditionsStackLength);
    else
	statePtr->conditionsStack
	    = (int *)Tcl_Alloc(sizeof(int) * statePtr->conditionsStackLength);
    
    statePtr->conditionsStack[statePtr->conditionsStackLength-1] = ci;

    return TCL_OK;


    /*
     * Error handling
     */

unknownConditionError:
    Tcl_AppendResult(interp, "unknown condition: \"", Tcl_GetStringFromObj(objv[2], NULL), "\"", (char *) NULL);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerEndObjCmd --
 *
 *	Pop a condition from the current state's condition stack.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The conditions stack of the current state is modified.
 *
 *--------------------------------------------------------------
 */

int
LexerEndObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];

    /* 
     * Sanity check
     */

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, NULL);
	return TCL_ERROR;
    }
    
    if (statePtr->conditionsStackLength == 0)
	/* Error */
	goto emptyConditionsStackError;

    /*
     * Delete the last condition from the stack
     *
     * We don'really need to realloc conditionsStack, since it is realloc'd 
     * with ConditionBegin and is freed when the lexer ends. So we just
     * decrement conditionsStackLength.
     */

    statePtr->conditionsStackLength--;
    
    return TCL_OK;


    /*
     * Error handling
     */

emptyConditionsStackError:
    Tcl_SetResult(interp, "empty conditions stack", TCL_STATIC);
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * ConditionsObjCmd --
 *
 *	This procedure is invoked to process the "conditions"
 *	subcommand for lexers. See the user documentation for
 *	details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
LexerConditionsObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    static char	*options[] = {
	"-current",
	NULL};
    static enum {
	OPTION_CURRENT
    } optionIndex;

    if (objc == 2) {
	/*
	 * No subcommands, we return the current conditions stack
	 */

	int     i;
	Tcl_Obj *cond = Tcl_NewObj(); /* List returned */

	/* Build the list */
	for (i=0; i < statePtr->conditionsStackLength; i++) {
	    Tcl_ListObjAppendElement(interp, cond,
		                       ConditionFromIndex(lexer, statePtr->conditionsStack[i]));
	}

	Tcl_SetObjResult(interp, cond);
	return TCL_OK;
    }

    /*
     * Switch to distinct subfunctions depending on the option
     */

    if (Tcl_GetIndexFromObj(interp, objv[2], options, "option", 0, &optionIndex) == TCL_ERROR)
	return TCL_ERROR;

    switch (optionIndex) {
	case OPTION_CURRENT:
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 3, objv, NULL);
		return TCL_ERROR;
	    }
	    return ConditionCurrent(interp, lexer);
    }
} 


/*
 *--------------------------------------------------------------
 *
 * LexerRejectObjCmd --
 *
 *	This procedure is invoked to process the "reject" subcommand
 *	for lexers. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
LexerRejectObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    
    /*
     * Sanity check
     */

    if (objc != 2)
	goto numberArgsError;

    /* Set the reject state tot true */
    statePtr->bRejected = 1;

    return TCL_OK;

    
    /*
     * Error handling
     */

numberArgsError:
    Tcl_WrongNumArgs(interp, 2, objv, "");
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerIndexObjCmd --
 *
 *	This procedure is invoked to process the "index" subcommand
 *	for lexers. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
LexerIndexObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    
    /*
     * Sanity check
     */
    if (objc != 2)
	goto numberArgsError;

    /* Return the current index */
    Tcl_SetObjResult(interp, Tcl_NewIntObj(statePtr->currentBuffer->index));
    return TCL_OK;


    /*
     * Error handling
     */
numberArgsError:
    Tcl_WrongNumArgs(interp, 2, objv, "");
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerInputObjCmd --
 *
 *	This procedure is invoked to process the "input" subcommand
 *	for lexers. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
LexerInputObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		strLen;
    Char	*str = StateGetString(lexer, lexer->curState, &strLen);
    int		nbChars = 1;
    int		oldIndex;
    Tcl_Obj	*val;
    
    /*
     * Sanity check
     */
    if (objc > 3)
	goto numberArgsError;

    if (objc == 3) {
	if (Tcl_GetIntFromObj(interp, objv[2], &nbChars) == TCL_ERROR)
	    return TCL_ERROR;
    }
    if (nbChars < 0)
	nbChars = 0;


    /* Consume and return the next nbChars characters */
    oldIndex = statePtr->currentBuffer->nextIndex;
    statePtr->currentBuffer->nextIndex += nbChars;

    /* TODO: handle overrun. Seems very difficult */
    if (statePtr->currentBuffer->nextIndex > strLen) {
	statePtr->currentBuffer->nextIndex = strLen-1;
    }

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
    val = Tcl_NewStringObj(str+oldIndex, statePtr->currentBuffer->nextIndex-oldIndex);
#else
    val = Tcl_NewUnicodeObj(str+oldIndex, statePtr->currentBuffer->nextIndex-oldIndex);
#endif

    Tcl_SetObjResult(interp, val);
    return TCL_OK;


    /*
     * Error handling
     */

numberArgsError:
    Tcl_WrongNumArgs(interp, 2, objv, "?nbChars?");
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerUnputObjCmd --
 *
 *	This procedure is invoked to process the "unput" subcommand
 *	for lexers. See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
LexerUnputObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer    = (TcLex_Lexer *)clientData;
    TcLex_State	*statePtr = lexer->states[lexer->curState];
    int		nbChars = 1;
    int		oldIndex;
    
    /*
     * Sanity check
     */

    if (objc > 3)
	goto numberArgsError;

    if (objc == 3) {
	if (Tcl_GetIntFromObj(interp, objv[2], &nbChars) == TCL_ERROR)
	    return TCL_ERROR;
    }
    if (nbChars < 0)
	nbChars = 0;


    /* Unput the previous nbChars characters */
    oldIndex = statePtr->currentBuffer->nextIndex;
    statePtr->currentBuffer->nextIndex -= nbChars;
    if (statePtr->currentBuffer->nextIndex < statePtr->currentBuffer->index) {
	statePtr->currentBuffer->nextIndex = statePtr->currentBuffer->index;
    }
    return TCL_OK;


    /*
     * Error handling
     */

numberArgsError:
    Tcl_WrongNumArgs(interp, 2, objv, "?nbChars?");
    return TCL_ERROR;
}


/*
 *--------------------------------------------------------------
 *
 * LexerSetCurrent --
 *
 *	This procedure sets the current active lexer. Used with the
 *	"eval" et al subcommands.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current lexer for the current interpreter gets modified.
 *
 *--------------------------------------------------------------
 */

void
LexerSetCurrent(interp, lexer)
    Tcl_Interp	*interp;
    TcLex_Lexer	*lexer;
{
    int created;
    Tcl_HashEntry *e = Tcl_CreateHashEntry(&CurrentLexersTable, (char*)interp, &created);
    Tcl_SetHashValue(e, (ClientData)lexer);
}

 
/*
 *--------------------------------------------------------------
 *
 * LexerGetCurrent --
 *
 *	This procedure gets the current active lexer. Used with the
 *	"current" subcommand.
 *
 * Results:
 *	A pointer to the current lexer.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

TcLex_Lexer*
LexerGetCurrent(interp)
    Tcl_Interp	*interp;
{
    int created;
    Tcl_HashEntry *e = Tcl_CreateHashEntry(&CurrentLexersTable, (char*)interp, &created);
    return (TcLex_Lexer*)Tcl_GetHashValue(e);
}

 
/*
 *--------------------------------------------------------------
 *
 * LexerObjCmd --
 *
 *	This procedure is invoked to process the Tcl commands
 *	created by "lexer", and serves as a switchbox between the
 *	different subcommands.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depend on subcommand.
 *
 *--------------------------------------------------------------
 */

int
LexerObjCmd(clientData, interp, objc, objv)
    ClientData	clientData;
    Tcl_Interp	*interp;
    int		objc;
    Tcl_Obj	*CONST objv[];
{
    TcLex_Lexer	*lexer = (TcLex_Lexer *)clientData;
    static char *options[] = {
	"eval",		    "start",	    "continue",		"finish",
	"begin",	    "end",
	"conditions",	    "reject",
	"index",	    "input",	    "unput", 
	NULL
    };
    static enum {
	OPTION_EVAL,	    OPTION_START,   OPTION_CONTINUE,	OPTION_FINISH,
	OPTION_BEGIN,	    OPTION_END,
	OPTION_CONDITIONS,  OPTION_REJECT,
	OPTION_INDEX,	    OPTION_INPUT,   OPTION_UNPUT
    } optionIndex;
    static int ops[] = {
	/* Lexer evaluation functions */
	LEXER_EVAL, LEXER_START, LEXER_CONTINUE, LEXER_FINISH,
    };
    static Tcl_ObjCmdProc *commands[] = {
	/* Lexer state functions */
	(Tcl_ObjCmdProc *)LexerBeginObjCmd,
	(Tcl_ObjCmdProc *)LexerEndObjCmd,
	(Tcl_ObjCmdProc *)LexerConditionsObjCmd,
	(Tcl_ObjCmdProc *)LexerRejectObjCmd,
	(Tcl_ObjCmdProc *)LexerIndexObjCmd,
	(Tcl_ObjCmdProc *)LexerInputObjCmd,
	(Tcl_ObjCmdProc *)LexerUnputObjCmd
    };
    
    /*
     * Sanity check
     */

    if (objc < 2)
	goto numberArgsError;

    /*
     * Get the subcommand
     */

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "option", 0, &optionIndex) == TCL_ERROR)
	return TCL_ERROR;

    /*
     * Switchbox
     */

    if (optionIndex < 4) {
	return LexerEvalObjCmd(interp, lexer, objc, objv, ops[optionIndex]);
    } else {
	/*
	 * If subcommand needs state, check if lexer is active
	 */

	if (lexer->curState == -1)
	    goto inactiveLexerError;
	return (commands[optionIndex-4])(clientData, interp, objc, objv);
    }

    
    /*
     * Error handling
     */

inactiveLexerError:
    Tcl_AppendResult(interp, "lexer \"", Tcl_GetStringFromObj(objv[0], NULL), "\" is inactive", NULL);
    return TCL_ERROR;
    
numberArgsError:
    Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
    return TCL_ERROR;
}   


/*
 *----------------------------------------------------------------------
 *
 * Tclex_Init --
 *
 *	This function is used to initialize the package. Here we check
 *	dependencies with other packages (eg Tcl), create new commands,
 *	and register our packages
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	New commands created, and package registered (see documentation).
 *
 *----------------------------------------------------------------------
 */

int Tclex_Init(interp)
    Tcl_Interp *interp;
{  
#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
	return TCL_ERROR;
    }
#endif


    /* Dependencies */
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL)
	return TCL_ERROR;

    /* Initialization */
    InitialCond = Tcl_NewStringObj("initial", -1);
    Tcl_IncrRefCount(InitialCond);

    Tcl_InitHashTable(&CurrentLexersTable, TCL_ONE_WORD_KEYS);


    /* Commands creation */
    Tcl_CreateObjCommand(interp, "lexer", (Tcl_ObjCmdProc *)LexerMainObjCmd,
		               (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    /* Package provided */
    if (Tcl_PkgProvide(interp, "tcLex", TCLEX_VERSION) == TCL_ERROR)
	return TCL_ERROR;

    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Tclex_SafeInit --
 *
 *	This function is used to initialize the package in the special case
 *	of safe interpreters (like the Tcl Plugin). Here we check
 *	dependencies with other packages (eg Tcl), create new commands,
 *	and register our packages
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	New commands created, and package registered (see documentation).
 *
 *----------------------------------------------------------------------
 */

int Tclex_SafeInit(interp)
    Tcl_Interp *interp;
{  
    /* For now, do the same as with unsafe interpreters */
    return Tclex_Init(interp);
}

#ifdef WIN32
/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Windows to invoke the
 *	initialization code for the DLL.  If we are compiling
 *	with Visual C++, this routine will be renamed to DllMain.
 *	routine.
 *
 * Results:
 *	Returns TRUE;
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst; /* Library instance handle. */
    DWORD reason;    /* Reason this function is being called. */
    LPVOID reserved; /* Not used. */
{
    return TRUE;
}
#endif /* WIN32 */

