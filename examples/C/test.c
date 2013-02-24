/* 
 * tcLex.h --
 *
 *  This is the header file for the module that implements lexer for Tcl.
 *  This file defines the public interface of the tcLex module.
 *
 * Copyright (c) 1998 Frédéric BONNET <frederic.bonnet@ciril.fr>
 *
 * See the file "license.txt" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#ifndef _TCLEX_H_
#define _TCLEX_H_


#include <tcl.h>

/*
 * if the BUILD_tcLex macro is defined, the assumption is that we are
 * building the dynamic library.
 */

#ifdef BUILD_tcLex
#  undef TCL_STORAGE_CLASS
#  define TCL_STORAGE_CLASS DLLEXPORT
#endif

/*
 *--------------------------------------------------------------
 *
 * Various stuff
 *
 *--------------------------------------------------------------
 */

#define EXPORT(a,b) a b

#ifdef WIN32
#   define WIN32_LEAN_AND_MEAN
#   include <windows.h>
#   undef WIN32_LEAN_AND_MEAN

#   ifndef STATIC_BUILD
#       if defined(__BORLANDC__)
#           undef EXPORT
#           define EXPORT(a,b) a _export b
#       endif
#   endif
#endif	/* WIN32 */


/*
 *--------------------------------------------------------------
 *
 * Structures
 *
 *--------------------------------------------------------------
 */

/* 
 * Lexer rules
 *
 * Each lexer is made by a collection of rules. Each rule is
 * described by the TcLex_Rule structure.
 */
typedef struct TcLex_Rule {
  /*
   * Each rule is active only within one or several conditions.
   * Here we store the indices of the valid conditions in the
   * array <conditionsIndices> of length <conditionsIndicesLength>,
   * w.r.t the lexer's TcLex_Lexer <inclusiveConditions> and
   * <exclusiveConditions> arrays
   */
  int *conditionsIndices;
  int conditionsIndicesLength;

  Tcl_Obj    *reObj;     /* Object containing the regexp */
  Tcl_RegExp re;         /* Regular expression of the rule */
  Tcl_Obj    *matchVars; /* List of match var names */
  int        nbRanges;   /* Max number of ranges in the regexp, approximately */
                         /* equal to the number of parentheses pairs */
  Tcl_Obj    *script;    /* Script eval'd when the rule is matched */

} TcLex_Rule;


/* 
 * Lexer buffers
 *
 * Buffers are used to handle input: Tcl strings, files, channels...
 */
#if (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION == 0)
typedef char Char;
#else
typedef Tcl_UniChar Char;
#endif

typedef struct TcLex_Buffer {
  Tcl_DString chars;

  int index;            /* Current byte position within <chars> */
  int nextIndex;

  int bMatchedEmpty;    /* Set when we match an empty string 
                         * (such as an empty line). To avoid
                         * endless loops we can't match two
                         * consecutive empty strings */
} TcLex_Buffer;

/* 
 * Lexer states
 *
 * When a lexer is active, its state is described using the
 * TcLex_State structure. It is used when the same lexer is
 * called recursively or in parallel (eg. incrementally)
 */
typedef struct TcLex_State {
  /*
   * The lexer's condition can change during its execution. We
   * need to store these conditions in a stack
   */
  int *conditionsStack;       /* Array used to store conditions,
                               * current condition is the last
                               * element */
  int conditionsStackLength;  /* Length of the above stack */

  char *bFailed;              /* Rule failure indicators. We need to
                               * store it for incremental processing
                               * to work properly */

  Tcl_HashTable *varTablePtr; /* Used to store the lexer's local
                               * variables for continuation,
                               * eg. for incremental evaluation */

  TcLex_Buffer inputBuffer;   /* Buffer from which to take input */
  TcLex_Buffer *currentBuffer;

  int bRejected;        /* Used when the rule is rejected, using 
                         * the "reject" subcommand */
  
  int nbRanges;         /* Number of matches ranges/subranges */
  int *startIndices;    /* When a rule matches, used to store */
  int *endIndices;      /* the matching strings' indices */

  Tcl_Obj *pendingResult;     /* Used when the lexer returns a
                               * result before reaching the end of
                               * the string */
} TcLex_State;


/* 
 * Lexer
 */

#define LEXER_FLAG_LINES    1
#define LEXER_FLAG_NOCASE   2
#define LEXER_FLAG_LONGEST  4

typedef struct TcLex_Lexer {
  Tcl_Command command; /* Token to the created Tcl command, used
                        * with [lexer current] */

  /*
   * There are 2 types of conditions: inclusive, and exclusive.
   * We store both types in the <conditions> array. Inclusive
   * conditions are stored in the first <nbInclusiveConditions>
   * elements, and the remaining <nbExclusiveConditions> elements
   * are used to store exclusive conditions. We also pre-process
   * the active rules for each condition in the <activeRules> array
   * for optimization; that way we don't have to search for active
   * rules at every match, we just look into the <activeRules> array.
   */
  Tcl_Obj **conditions;
  int **activeRules;
  int nbInclusiveConditions, nbExclusiveConditions;

  int flags;            /* OR-ed values of LEXER_FLAG_????? */

  /*
   * The scripts <preScript> and <postScript> are evaluated
   * resp. before and after the string lexing. <preScript> is
   * mainly used for initialization, and <postScript> for
   * returning a result.
   * The script <midScript> is only evaluated during an incremental
   * lexing, at the end of each intermediary lexing, ie calls to
   * "start" or "continue". A call to "finish" return the result
   * of <postScript>.
   * If none of these scripts return a value, the content of
   * the variable named <resultVariable> is returned, if any
   */
  Tcl_Obj *preScript, *midScript, *postScript;
  Tcl_Obj *args;
  Tcl_Obj *resultVariable;

  TcLex_Rule *rules;    /* Array used to store the lexer's rules */
  int nbRules;          /* Number of rules, ie length of the 
                         * above array */

  /*
   * Since one lexer can have several active instances at a time,
   * we need to store their states, in the <states> array
   */
  TcLex_State **states;
  int nbStates;         /* Length of the above array */
  int curState;         /* Index of the current state in the array */

} TcLex_Lexer;


/*
 *--------------------------------------------------------------
 *
 * Functions
 *
 *--------------------------------------------------------------
 */

/*
 * The main entry point
 */
EXTERN EXPORT(int,Tclex_Init)     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN EXPORT(int,Tclex_SafeInit) _ANSI_ARGS_((Tcl_Interp *interp));


/*
 * Lexer Tcl command
 */
int LexerObjCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                             int objc, Tcl_Obj *CONST objv[]));


/*
 * Lexer management functions
 */
void          LexerDelete _ANSI_ARGS_((TcLex_Lexer *lexer));
TcLex_Lexer * LexerNew    _ANSI_ARGS_((Tcl_Interp *interp,
                                       Tcl_Obj *inclusiveConditions, Tcl_Obj *exclusiveConditions,
                                       Tcl_Obj *resultVariable,
                                       Tcl_Obj *preScript, Tcl_Obj *midScript, Tcl_Obj *postScript,
                                       Tcl_Obj *args, int flags, 
                                       int rulec, Tcl_Obj *CONST *rulev));

void  LexerCmdDeleteProc _ANSI_ARGS_((ClientData clientData));
int   LexerMainObjCmd    _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp, 
                                      int objc, Tcl_Obj *CONST objv[]));
int   LexerCreateObjCmd  _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp, 
                                      int objc, Tcl_Obj *CONST objv[], int start));
int   LexerCurrentObjCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp, 
                                      int objc, Tcl_Obj *CONST objv[]));


/*
 * Conditions management functions
 */
int ConditionCurrent _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer));


/*
 * Lexer evaluation functions
 */

/* LexerEval op's */
#define LEXER_EVAL     0
#define LEXER_START    1
#define LEXER_CONTINUE 2
#define LEXER_FINISH   3

int LexerEval       _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer, 
                                 int bCheckOverrun));
int LexerEvalObjCmd _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer,
                                 int objc, Tcl_Obj *CONST objv[], int op));

/*
 * Lexer state functions
 */
int LexerBeginObjCmd      _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerEndObjCmd        _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerConditionsObjCmd _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerRejectObjCmd     _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerIndexObjCmd      _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerInputObjCmd      _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));
int LexerUnputObjCmd      _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
                                       int objc, Tcl_Obj *CONST objv[]));


/*
 * end of tcLex.h
 * reset TCL_STORAGE_CLASS to DLLIMPORT.
 */

# undef TCL_STORAGE_CLASS
# define TCL_STORAGE_CLASS DLLIMPORT

#endif /* _TCLEX_H_ */
