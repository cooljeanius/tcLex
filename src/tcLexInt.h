/* 
 * tcLexInt.h --
 *
 *  This is the header file for the module that implements lexer for Tcl.
 *  This file defines the internal parts of the tcLex module. They are not
 *  supposed to be used outside of tcLex.
 *
 * Copyright (c) 1998 Frédéric BONNET <frederic.bonnet@ciril.fr>
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _TCLEXINT_H_
#define _TCLEXINT_H_


/*
 *--------------------------------------------------------------
 *
 * Global variables
 *
 *--------------------------------------------------------------
 */

/* Initial condition, ie condition named "0" or "initial" */
/* We use a pre-initialized object to save space & time   */
static Tcl_Obj *InitialCond;

/* Table of currently active lexers, one per interpreter */
static Tcl_HashTable CurrentLexersTable;


/*
 *--------------------------------------------------------------
 *
 * Functions
 *
 *--------------------------------------------------------------
 */

/*
 * Tcl8.1 compatibily procs: Unicode object type.
 * This object uses a Unicode string in a Tcl_DString as their
 * internal representation
 */

#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 1)
int	     Tcl_GetCharLength _ANSI_ARGS_((Tcl_Obj *objPtr));
Tcl_UniChar* Tcl_GetUnicode    _ANSI_ARGS_((Tcl_Obj *objPtr));
#endif

/*
 * Lexer info functions
 */
void         LexerSetCurrent _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer));
TcLex_Lexer* LexerGetCurrent _ANSI_ARGS_((Tcl_Interp *interp));

/*
 * Conditions management functions
 */
static int       ConditionIndex       _ANSI_ARGS_((TcLex_Lexer *lexer, char *string));
static Tcl_Obj * ConditionFromIndex   _ANSI_ARGS_((TcLex_Lexer *lexer, int index));
static int       ConditionIsExclusive _ANSI_ARGS_((TcLex_Lexer *lexer, int index));

/*
 * State management functions
 */
static int    StateNew       _ANSI_ARGS_((TcLex_Lexer *lexer, Tcl_Obj *string));
static void   StateSetString _ANSI_ARGS_((TcLex_Lexer *lexer, int n, Tcl_Obj *string));
static Char * StateGetString _ANSI_ARGS_((TcLex_Lexer *lexer, int n, int *length));
static void   StateDelete    _ANSI_ARGS_((TcLex_Lexer *lexer, int n));

/*
 * Rule functions
 */
int  RuleTry         _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer, int iRule, int bCheckOverrun, int *pMinLength));
int  LexerMatchVars  _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer, int bIndices, int matchc, Tcl_Obj *CONST matchv[]));
void RuleEvalDefault _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer));

#endif /* _TCLEXINT_H_ */
