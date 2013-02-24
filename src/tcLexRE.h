int  RuleCompileRegexp _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Rule *rule, Tcl_Obj *reObj, int flags));
int  RuleExec          _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer, TcLex_Rule *rule, Tcl_Obj *stringObj, int index, int start, int *pOverrun));
void RuleGetRange      _ANSI_ARGS_((Tcl_Interp *interp, TcLex_Lexer *lexer, TcLex_Rule *rule, Tcl_Obj *stringObj, int index, int rangeIndex, int *start, int *end));
void RuleFree          _ANSI_ARGS_((TcLex_Rule *rule));
