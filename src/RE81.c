/*
 * This is a slightly modified portion of Tcl's regexp.c and rege_dfa.c
 * files. Please read these files for further information (legal issues,
 * comments...). The reasons why we do that instead of calling Tcl regexp
 * functions directly are:
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

#include "tcLexInt.h"

/**************************************
 * Modified regexec.c from Tcl source *
 **************************************/


/*
 * re_*exec and friends - match REs
 */

#include <regguts.h>



/******************
 * Redefine stuff *
 ******************/

/* Redefine rm_detail_t struct hook */
typedef struct {
	int endReached;	/* used for reporting if we reach the end of */
	/* the input string */
} my_rm_detail_t;
#undef rm_detail_t
#define rm_detail_t my_rm_detail_t

/* Undefine exec */
#undef exec

/***********************
 * End of redefinition *
 ***********************/



/* internal variables, bundled for easy passing around */
struct vars {
	regex_t *re;
	struct guts *g;
	int eflags;		/* copies of arguments */
	size_t nmatch;
	regmatch_t *pmatch;
	chr *start;		/* start of string */
	chr *stop;		/* just past end of string */
	int err;		/* error code if any (0 none) */
	regoff_t *mem;		/* memory vector for backtracking */
	rm_detail_t details;	/* hook for future ellaboration */
};
#define	VISERR(vv)	((vv)->err != 0)	/* have we seen an error yet? */
#define	ISERR()	VISERR(v)
#define	VERR(vv,e)	(((vv)->err) ? (vv)->err : ((vv)->err = (e)))
#define	ERR(e)	VERR(v, e)		/* record an error */
#define	NOERR()	{if (ISERR()) return;}	/* if error seen, return */
#define	OFF(p)	((p) - v->start)
#define	LOFF(p)	((long)OFF(p))



/* lazy-DFA representation */
struct arcp {			/* "pointer" to an outarc */
	struct sset *ss;
	color co;
};

struct sset {			/* state set */
	unsigned *states;	/* pointer to bitvector */
	unsigned hash;		/* hash of bitvector */
#		define	HASH(bv, nw)	(((nw) == 1) ? *(bv) : hash(bv, nw))
#	define	HIT(h,bv,ss,nw)	((ss)->hash == (h) && ((nw) == 1 || \
		memcmp(VS(bv), VS((ss)->states), (nw)*sizeof(unsigned)) == 0))
	int flags;
#		define	STARTER		01	/* the initial state set */
#		define	POSTSTATE	02	/* includes the goal state */
#		define	LOCKED		04	/* locked in cache */
#		define	NOPROGRESS	010	/* zero-progress state set */
	struct arcp ins;	/* chain of inarcs pointing here */
	chr *lastseen;		/* last entered on arrival here */
	struct sset **outs;	/* outarc vector indexed by color */
	struct arcp *inchain;	/* chain-pointer vector for outarcs */
};

struct dfa {
	int nssets;		/* size of cache */
	int nssused;		/* how many entries occupied yet */
	int nstates;		/* number of states */
	int ncolors;		/* length of outarc and inchain vectors */
	int wordsper;		/* length of state-set bitvectors */
	struct sset *ssets;	/* state-set cache */
	unsigned *statesarea;	/* bitvector storage */
	unsigned *work;		/* pointer to work area within statesarea */
	struct sset **outsarea;	/* outarc-vector storage */
	struct arcp *incarea;	/* inchain storage */
	struct cnfa *cnfa;
	struct colormap *cm;
	chr *lastpost;		/* location of last cache-flushed success */
	chr *lastnopr;		/* location of last cache-flushed NOPROGRESS */
	struct sset *search;	/* replacement-search-pointer memory */
	int cptsmalloced;	/* were the areas individually malloced? */
	char *mallocarea;	/* self, or master malloced area, or NULL */
};

#define	WORK	1		/* number of work bitvectors needed */

/* setup for non-malloc allocation for small cases */
#define	FEWSTATES	20	/* must be less than UBITS */
#define	FEWCOLORS	15
struct smalldfa {
	struct dfa dfa;
	struct sset ssets[FEWSTATES*2];
	unsigned statesarea[FEWSTATES*2 + WORK];
	struct sset *outsarea[FEWSTATES*2 * FEWCOLORS];
	struct arcp incarea[FEWSTATES*2 * FEWCOLORS];
};




/*
 * forward declarations
 */
/* =====^!^===== begin forwards =====^!^===== */
/* automatically gathered by fwd; do not hand-edit */
/* === regexec.c === */
int exec _ANSI_ARGS_((regex_t *, CONST chr *, size_t, rm_detail_t *, size_t, regmatch_t [], int));
static int find _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *));
static int cfind _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *));
static VOID zapsubs _ANSI_ARGS_((regmatch_t *, size_t));
static VOID zapmem _ANSI_ARGS_((struct vars *, struct subre *));
static VOID subset _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int dissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int condissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int altdissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int cdissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int ccondissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int crevdissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int cbrdissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
static int caltdissect _ANSI_ARGS_((struct vars *, struct subre *, chr *, chr *));
/* === rege_dfa.c === */
static chr *longest _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *));
static chr *shortest _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *, chr *, chr **));
static struct dfa *newdfa _ANSI_ARGS_((struct vars *, struct cnfa *, struct colormap *, struct smalldfa *));
static VOID freedfa _ANSI_ARGS_((struct dfa *));
static unsigned hash _ANSI_ARGS_((unsigned *, int));
static struct sset *initialize _ANSI_ARGS_((struct vars *, struct dfa *, chr *));
static struct sset *miss _ANSI_ARGS_((struct vars *, struct dfa *, struct sset *, pcolor, chr *, chr *));
static int lacon _ANSI_ARGS_((struct vars *, struct cnfa *, chr *, pcolor));
static struct sset *getvacant _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *));
static struct sset *pickss _ANSI_ARGS_((struct vars *, struct dfa *, chr *, chr *));
/* automatically gathered by fwd; do not hand-edit */
/* =====^!^===== end forwards =====^!^===== */

#define	REG_BEGINNING	0100	/* only match at the beginning of string */
#define	REG_REPORTEOS	0200	/* report when you reach end of string */


/*
 - exec - match regular expression
 ^ int exec(regex_t *, CONST chr *, size_t, rm_detail_t *,
 ^					size_t, regmatch_t [], int);
 */
int
exec(re, string, len, details, nmatch, pmatch, flags)
regex_t *re;
CONST chr *string;
size_t len;
rm_detail_t *details;		/* hook for future elaboration */
size_t nmatch;
regmatch_t pmatch[];
int flags;
{
	struct vars var;
	register struct vars *v = &var;
	int st;
	size_t n;
	int complications;
#	define	LOCALMAT	20
	regmatch_t mat[LOCALMAT];
#	define	LOCALMEM	40
	regoff_t mem[LOCALMEM];

	/* sanity checks */
	if (re == NULL || string == NULL || re->re_magic != REMAGIC)
		return REG_INVARG;
	if (re->re_csize != sizeof(chr))
		return REG_MIXED;

	/* setup */
	v->re = re;
	v->g = (struct guts *)re->re_guts;
	if (v->g->unmatchable)
		return REG_NOMATCH;
	complications = (v->g->info&REG_UBACKREF) ? 1 : 0;
	if (v->g->usedshorter)
		complications = 1;
	v->eflags = flags;
	if (v->g->cflags&REG_NOSUB)
		nmatch = 0;		/* override client */
	v->nmatch = nmatch;
	if (complications && v->nmatch < v->g->nsub + 1) {
		/* need work area bigger than what user gave us */
		if (v->g->nsub + 1 <= LOCALMAT)
			v->pmatch = mat;
		else
			v->pmatch = (regmatch_t *)MALLOC((v->g->nsub + 1) *
							sizeof(regmatch_t));
		if (v->pmatch == NULL)
			return REG_ESPACE;
		v->nmatch = v->g->nsub + 1;
	} else
		v->pmatch = pmatch;
	v->start = (chr *)string;
	v->stop = (chr *)string + len;
	v->err = 0;

	/* Addition */
	/* get details */
	if (details) {
	    v->details = *details;
	}
	v->details.endReached = 0;
	/* End addition */

	if (complications) {
		assert(v->g->ntree >= 0);
		n = (size_t)v->g->ntree;
		if (n <= LOCALMEM)
			v->mem = mem;
		else
			v->mem = (regoff_t *)MALLOC(n*sizeof(regoff_t));
		if (v->mem == NULL) {
			if (v->pmatch != pmatch && v->pmatch != mat)
				FREE(v->pmatch);
			return REG_ESPACE;
		}
	} else
		v->mem = NULL;

	/* do it */
	assert(v->g->tree != NULL);
	if (complications)
		st = cfind(v, &v->g->tree->cnfa, &v->g->cmap);
	else
		st = find(v, &v->g->tree->cnfa, &v->g->cmap);

	/* copy (portion of) match vector over if necessary */
	if (st == REG_OKAY && v->pmatch != pmatch && nmatch > 0) {
		zapsubs(pmatch, nmatch);
		n = (nmatch < v->nmatch) ? nmatch : v->nmatch;
		memcpy(VS(pmatch), VS(v->pmatch), n*sizeof(regmatch_t));
	}

	/* Addition */
	/* report details */
	if (details)
	    *details = v->details;
	/* End addition */

	/* clean up */
	if (v->pmatch != pmatch && v->pmatch != mat)
		FREE(v->pmatch);
	if (v->mem != NULL && v->mem != mem)
		FREE(v->mem);

	return st;
}

/*
 - find - find a match for the main NFA (no-complications case)
 ^ static int find(struct vars *, struct cnfa *, struct colormap *);
 */
static int
find(v, cnfa, cm)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
{
	struct smalldfa da;
	struct dfa *d = newdfa(v, cnfa, cm, &da);
	struct smalldfa sa;
	struct dfa *s = newdfa(v, &v->g->search, cm, &sa);
	chr *begin;
	chr *end;
	chr *open;		/* open and close of range of possible starts */
	chr *close;

	if (d == NULL)
		return v->err;
	if (s == NULL) {
		freedfa(d);
		return v->err;
	}

	close = v->start;
	do {
		MDEBUG(("\nsearch at %ld\n", LOFF(close)));
		/* Addition */
		if (v->eflags&REG_BEGINNING) {
			/* We only match at the beginning of the string */
			open = v->start;
		} else {
		/* End addition */
			close = shortest(v, s, close, close, v->stop, &open);
			if (close == NULL)
				break;			/* NOTE BREAK */
			if (v->nmatch == 0) {
				/* don't need exact location */
				freedfa(d);
				freedfa(s);
				return REG_OKAY;
			}
		/* Addition */
		}
		/* End addition */
		MDEBUG(("between %ld and %ld\n", LOFF(open), LOFF(close)));
		for (begin = open; begin <= close; begin++) {
			MDEBUG(("\nfind trying at %ld\n", LOFF(begin)));
			end = longest(v, d, begin, v->stop);
			if (end != NULL) {
				assert(v->nmatch > 0);
 				v->pmatch[0].rm_so = OFF(begin);
 				v->pmatch[0].rm_eo = OFF(end);
				freedfa(d);
				freedfa(s);
				if (v->nmatch > 1) {
					zapsubs(v->pmatch, v->nmatch);
					return dissect(v, v->g->tree, begin,
									end);
				}
				if (ISERR())
					return v->err;
				return REG_OKAY;
			}
		}
	/* Modification */
	/*} while (close < v->stop);*/
	} while (!(v->eflags&REG_BEGINNING) && close < v->stop);
	/* End modification */

	freedfa(d);
	freedfa(s);
	if (ISERR())
		return v->err;
	return REG_NOMATCH;
}

/*
 - cfind - find a match for the main NFA (with complications)
 ^ static int cfind(struct vars *, struct cnfa *, struct colormap *);
 */
static int
cfind(v, cnfa, cm)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
{
	struct smalldfa da;
	struct dfa *d = newdfa(v, cnfa, cm, &da);
	struct smalldfa sa;
	struct dfa *s = newdfa(v, &v->g->search, cm, &sa);
	chr *begin;
	chr *end;
	chr *open;		/* open and close of range of possible starts */
	chr *close;
	chr *estart;
	chr *estop;
	int er;
	int shorter = v->g->tree->flags&SHORTER;

	if (d == NULL)
		return v->err;
	if (s == NULL) {
		freedfa(d);
		return v->err;
	}

	close = v->start;
	do {
		MDEBUG(("\ncsearch at %ld\n", LOFF(close)));
		/* Addition */
		if (v->eflags&REG_BEGINNING) {
			/* We only match at the beginning of the string */
			open = v->start;
		} else {
		/* End addition */
			close = shortest(v, s, close, close, v->stop, &open);
			if (close == NULL)
				break;			/* NOTE BREAK */
		/* Addition */
		}
		/* End addition */
		MDEBUG(("cbetween %ld and %ld\n", LOFF(open), LOFF(close)));
		for (begin = open; begin <= close; begin++) {
			MDEBUG(("\ncfind trying at %ld\n", LOFF(begin)));
			estart = begin;
			estop = v->stop;
			for (;;) {
				if (shorter)
					end = shortest(v, d, begin, estart,
							estop, (chr **)NULL);
				else
					end = longest(v, d, begin, estop);
				if (end == NULL)
					break;		/* NOTE BREAK OUT */
				MDEBUG(("tentative end %ld\n", LOFF(end)));
				zapsubs(v->pmatch, v->nmatch);
				zapmem(v, v->g->tree);
				er = cdissect(v, v->g->tree, begin, end);
				switch (er) {
				case REG_OKAY:
					if (v->nmatch > 0) {
						v->pmatch[0].rm_so = OFF(begin);
						v->pmatch[0].rm_eo = OFF(end);
					}
					freedfa(d);
					freedfa(s);
					if (ISERR())
						return v->err;
					return REG_OKAY;
					break;
				case REG_NOMATCH:
					/* go around and try again */
					if ((shorter) ? end == estop :
								end == begin) {
						/* no point in trying again */
						freedfa(s);
						freedfa(d);
						return REG_NOMATCH;
					}
					if (shorter)
						estart = end + 1;
					else
						estop = end - 1;
					break;
				default:
					freedfa(d);
					freedfa(s);
					return er;
					break;
				}
			}
		}
	/* Modification */
	/*} while (close < v->stop);*/
	} while (!(v->eflags&REG_BEGINNING) && close < v->stop);
	/* End modification */

	freedfa(d);
	freedfa(s);
	if (ISERR())
		return v->err;
	return REG_NOMATCH;
}

/*
 - zapsubs - initialize the subexpression matches to "no match"
 ^ static VOID zapsubs(regmatch_t *, size_t);
 */
static VOID
zapsubs(p, n)
regmatch_t *p;
size_t n;
{
	size_t i;

	for (i = n-1; i > 0; i--) {
		p[i].rm_so = -1;
		p[i].rm_eo = -1;
	}
}

/*
 - zapmem - initialize the retry memory of a subtree to zeros
 ^ static VOID zapmem(struct vars *, struct subre *);
 */
static VOID
zapmem(v, t)
struct vars *v;
struct subre *t;
{
	if (t == NULL)
		return;

	assert(v->mem != NULL);
	v->mem[t->retry] = 0;
	if (t->op == '(') {
		assert(t->subno > 0);
		v->pmatch[t->subno].rm_so = -1;
		v->pmatch[t->subno].rm_eo = -1;
	}

	if (t->left != NULL)
		zapmem(v, t->left);
	if (t->right != NULL)
		zapmem(v, t->right);
}

/*
 - subset - set any subexpression relevant to a successful subre
 ^ static VOID subset(struct vars *, struct subre *, chr *, chr *);
 */
static VOID
subset(v, sub, begin, end)
struct vars *v;
struct subre *sub;
chr *begin;
chr *end;
{
	int n = sub->subno;

	assert(n > 0);
	if ((size_t)n >= v->nmatch)
		return;

	MDEBUG(("setting %d\n", n));
	v->pmatch[n].rm_so = OFF(begin);
	v->pmatch[n].rm_eo = OFF(end);
}

/*
 - dissect - determine subexpression matches (uncomplicated case)
 ^ static int dissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
dissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	assert(t != NULL);
	MDEBUG(("dissect %ld-%ld\n", LOFF(begin), LOFF(end)));

	switch (t->op) {
	case '=':		/* terminal node */
		assert(t->left == NULL && t->right == NULL);
		return REG_OKAY;	/* no action, parent did the work */
		break;
	case '|':		/* alternation */
		assert(t->left != NULL);
		return altdissect(v, t, begin, end);
		break;
	case 'b':		/* back ref -- shouldn't be calling us! */
		return REG_ASSERT;
		break;
	case '.':		/* concatenation */
		assert(t->left != NULL && t->right != NULL);
		return condissect(v, t, begin, end);
		break;
	case '(':		/* capturing */
		assert(t->left != NULL && t->right == NULL);
		assert(t->subno > 0);
		subset(v, t, begin, end);
		return dissect(v, t->left, begin, end);
		break;
	default:
		return REG_ASSERT;
		break;
	}
}

/*
 - condissect - determine concatenation subexpression matches (uncomplicated)
 ^ static int condissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
condissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct smalldfa da;
	struct dfa *d;
	struct smalldfa d2a;
	struct dfa *d2;
	chr *mid;
	int i;

	assert(t->op == '.');
	assert(t->left != NULL && t->left->cnfa.nstates > 0);
	assert(t->right != NULL && t->right->cnfa.nstates > 0);

	d = newdfa(v, &t->left->cnfa, &v->g->cmap, &da);
	if (ISERR())
		return v->err;
	d2 = newdfa(v, &t->right->cnfa, &v->g->cmap, &d2a);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}

	/* pick a tentative midpoint */
	mid = longest(v, d, begin, end);
	if (mid == NULL) {
		freedfa(d);
		freedfa(d2);
		return REG_ASSERT;
	}
	MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));

	/* iterate until satisfaction or failure */
	while (longest(v, d2, mid, end) != end) {
		/* that midpoint didn't work, find a new one */
		if (mid == begin) {
			/* all possibilities exhausted! */
			MDEBUG(("no midpoint!\n"));
			freedfa(d);
			freedfa(d2);
			return REG_ASSERT;
		}
		mid = longest(v, d, begin, mid-1);
		if (mid == NULL) {
			/* failed to find a new one! */
			MDEBUG(("failed midpoint!\n"));
			freedfa(d);
			freedfa(d2);
			return REG_ASSERT;
		}
		MDEBUG(("new midpoint %ld\n", LOFF(mid)));
	}

	/* satisfaction */
	MDEBUG(("successful\n"));
	freedfa(d);
	freedfa(d2);
	i = dissect(v, t->left, begin, mid);
	if (i != REG_OKAY)
		return i;
	return dissect(v, t->right, mid, end);
}

/*
 - altdissect - determine alternative subexpression matches (uncomplicated)
 ^ static int altdissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
altdissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct smalldfa da;
	struct dfa *d;
	int i;

	assert(t != NULL);
	assert(t->op == '|');

	for (i = 0; t != NULL; t = t->right, i++) {
		MDEBUG(("trying %dth\n", i));
		assert(t->left != NULL && t->left->cnfa.nstates > 0);
		d = newdfa(v, &t->left->cnfa, &v->g->cmap, &da);
		if (ISERR())
			return v->err;
		if (longest(v, d, begin, end) == end) {
			MDEBUG(("success\n"));
			freedfa(d);
			return dissect(v, t->left, begin, end);
		}
		freedfa(d);
	}
	return REG_ASSERT;	/* none of them matched?!? */
}

/*
 - cdissect - determine subexpression matches (with complications)
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static int cdissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
cdissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	int er;

	assert(t != NULL);
	MDEBUG(("cdissect %ld-%ld\n", LOFF(begin), LOFF(end)));

	switch (t->op) {
	case '=':		/* terminal node */
		assert(t->left == NULL && t->right == NULL);
		return REG_OKAY;	/* no action, parent did the work */
		break;
	case '|':		/* alternation */
		assert(t->left != NULL);
		return caltdissect(v, t, begin, end);
		break;
	case 'b':		/* back ref -- shouldn't be calling us! */
		assert(t->left == NULL && t->right == NULL);
		return cbrdissect(v, t, begin, end);
		break;
	case '.':		/* concatenation */
		assert(t->left != NULL && t->right != NULL);
		return ccondissect(v, t, begin, end);
		break;
	case '(':		/* capturing */
		assert(t->left != NULL && t->right == NULL);
		assert(t->subno > 0);
		er = cdissect(v, t->left, begin, end);
		if (er == REG_OKAY)
			subset(v, t, begin, end);
		return er;
		break;
	default:
		return REG_ASSERT;
		break;
	}
}

/*
 - ccondissect - concatenation subexpression matches (with complications)
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static int ccondissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
ccondissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct smalldfa da;
	struct dfa *d;
	struct smalldfa d2a;
	struct dfa *d2;
	chr *mid;
	int er;

	assert(t->op == '.');
	assert(t->left != NULL && t->left->cnfa.nstates > 0);
	assert(t->right != NULL && t->right->cnfa.nstates > 0);

	if (t->left->flags&SHORTER)		/* reverse scan */
		return crevdissect(v, t, begin, end);

	d = newdfa(v, &t->left->cnfa, &v->g->cmap, &da);
	if (ISERR())
		return v->err;
	d2 = newdfa(v, &t->right->cnfa, &v->g->cmap, &d2a);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}
	MDEBUG(("cconcat %d\n", t->retry));

	/* pick a tentative midpoint */
	if (v->mem[t->retry] == 0) {
		mid = longest(v, d, begin, end);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));
		v->mem[t->retry] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[t->retry] - 1);
		MDEBUG(("working midpoint %ld\n", LOFF(mid)));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		er = cdissect(v, t->left, begin, mid);
		if (er == REG_OKAY && longest(v, d2, mid, end) == end &&
				(er = cdissect(v, t->right, mid, end)) == 
								REG_OKAY)
			break;			/* NOTE BREAK OUT */
		if (er != REG_OKAY && er != REG_NOMATCH) {
			freedfa(d);
			freedfa(d2);
			return er;
		}

		/* that midpoint didn't work, find a new one */
		if (mid == begin) {
			/* all possibilities exhausted */
			MDEBUG(("%d no midpoint\n", t->retry));
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		mid = longest(v, d, begin, mid-1);
		if (mid == NULL) {
			/* failed to find a new one */
			MDEBUG(("%d failed midpoint\n", t->retry));
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		MDEBUG(("%d: new midpoint %ld\n", t->retry, LOFF(mid)));
		v->mem[t->retry] = (mid - begin) + 1;
		zapmem(v, t->left);
		zapmem(v, t->right);
	}

	/* satisfaction */
	MDEBUG(("successful\n"));
	freedfa(d);
	freedfa(d2);
	return REG_OKAY;
}

/*
 - crevdissect - determine shortest-first subexpression matches
 * The retry memory stores the offset of the trial midpoint from begin, 
 * plus 1 so that 0 uniquely means "clean slate".
 ^ static int crevdissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
crevdissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct smalldfa da;
	struct dfa *d;
	struct smalldfa d2a;
	struct dfa *d2;
	chr *mid;
	int er;

	assert(t->op == '.');
	assert(t->left != NULL && t->left->cnfa.nstates > 0);
	assert(t->right != NULL && t->right->cnfa.nstates > 0);
	assert(t->left->flags&SHORTER);

	/* concatenation -- need to split the substring between parts */
	d = newdfa(v, &t->left->cnfa, &v->g->cmap, &da);
	if (ISERR())
		return v->err;
	d2 = newdfa(v, &t->right->cnfa, &v->g->cmap, &d2a);
	if (ISERR()) {
		freedfa(d);
		return v->err;
	}
	MDEBUG(("crev %d\n", t->retry));

	/* pick a tentative midpoint */
	if (v->mem[t->retry] == 0) {
		mid = shortest(v, d, begin, begin, end, (chr **)NULL);
		if (mid == NULL) {
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		MDEBUG(("tentative midpoint %ld\n", LOFF(mid)));
		v->mem[t->retry] = (mid - begin) + 1;
	} else {
		mid = begin + (v->mem[t->retry] - 1);
		MDEBUG(("working midpoint %ld\n", LOFF(mid)));
	}

	/* iterate until satisfaction or failure */
	for (;;) {
		/* try this midpoint on for size */
		er = cdissect(v, t->left, begin, mid);
		if (er == REG_OKAY && longest(v, d2, mid, end) == end &&
				(er = cdissect(v, t->right, mid, end)) == 
								REG_OKAY)
			break;			/* NOTE BREAK OUT */
		if (er != REG_OKAY && er != REG_NOMATCH) {
			freedfa(d);
			freedfa(d2);
			return er;
		}

		/* that midpoint didn't work, find a new one */
		if (mid == end) {
			/* all possibilities exhausted */
			MDEBUG(("%d no midpoint\n", t->retry));
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		mid = shortest(v, d, begin, mid+1, end, (chr **)NULL);
		if (mid == NULL) {
			/* failed to find a new one */
			MDEBUG(("%d failed midpoint\n", t->retry));
			freedfa(d);
			freedfa(d2);
			return REG_NOMATCH;
		}
		MDEBUG(("%d: new midpoint %ld\n", t->retry, LOFF(mid)));
		v->mem[t->retry] = (mid - begin) + 1;
		zapmem(v, t->left);
		zapmem(v, t->right);
	}

	/* satisfaction */
	MDEBUG(("successful\n"));
	freedfa(d);
	freedfa(d2);
	return REG_OKAY;
}

/*
 - cbrdissect - determine backref subexpression matches
 ^ static int cbrdissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
cbrdissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	int i;
	int n = t->subno;
	size_t len;
	chr *paren;
	chr *p;
	chr *stop;
	int min = t->min;
	int max = t->max;

	assert(t != NULL);
	assert(t->op == 'b');
	assert(n >= 0);
	assert((size_t)n < v->nmatch);

	MDEBUG(("cbackref n%d %d{%d-%d}\n", t->retry, n, min, max));

	if (v->pmatch[n].rm_so == -1)
		return REG_NOMATCH;
	paren = v->start + v->pmatch[n].rm_so;
	len = v->pmatch[n].rm_eo - v->pmatch[n].rm_so;

	/* no room to maneuver -- retries are pointless */
	if (v->mem[t->retry])
		return REG_NOMATCH;
	v->mem[t->retry] = 1;

	/* special-case zero-length string */
	if (len == 0) {
		if (begin == end)
			return REG_OKAY;
		return REG_NOMATCH;
	}

	/* and too-short string */
	assert(end >= begin);
	if ((size_t)(end - begin) < len)
		return REG_NOMATCH;
	stop = end - len;

	/* count occurrences */
	i = 0;
	for (p = begin; p <= stop && (i < max || max == INFINITY); p += len) {
		if ((*v->g->compare)(paren, p, len) != 0)
				break;
		i++;
	}
	MDEBUG(("cbackref found %d\n", i));

	/* and sort it out */
	if (p != end)			/* didn't consume all of it */
		return REG_NOMATCH;
	if (min <= i && (i <= max || max == INFINITY))
		return REG_OKAY;
	return REG_NOMATCH;		/* out of range */
}

/*
 - caltdissect - determine alternative subexpression matches (w. complications)
 ^ static int caltdissect(struct vars *, struct subre *, chr *, chr *);
 */
static int			/* regexec return code */
caltdissect(v, t, begin, end)
struct vars *v;
struct subre *t;
chr *begin;			/* beginning of relevant substring */
chr *end;			/* end of same */
{
	struct smalldfa da;
	struct dfa *d;
	int er;
#	define	UNTRIED	0	/* not yet tried at all */
#	define	TRYING	1	/* top matched, trying submatches */
#	define	TRIED	2	/* top didn't match or submatches exhausted */

	if (t == NULL)
		return REG_NOMATCH;
	assert(t->op == '|');
	if (v->mem[t->retry] == TRIED)
		return caltdissect(v, t->right, begin, end);

	MDEBUG(("calt n%d\n", t->retry));
	assert(t->left != NULL);

	if (v->mem[t->retry] == UNTRIED) {
		d = newdfa(v, &t->left->cnfa, &v->g->cmap, &da);
		if (ISERR())
			return v->err;
		if (longest(v, d, begin, end) != end) {
			freedfa(d);
			v->mem[t->retry] = TRIED;
			return caltdissect(v, t->right, begin, end);
		}
		freedfa(d);
		MDEBUG(("calt matched\n"));
		v->mem[t->retry] = TRYING;
	}

	er = cdissect(v, t->left, begin, end);
	if (er != REG_NOMATCH)
		return er;

	v->mem[t->retry] = TRIED;
	return caltdissect(v, t->right, begin, end);
}



/*#include "rege_dfa.c"*/

/***************************************
 * Modified rege_dfa.c from Tcl source *
 ***************************************/

/*
 * DFA routines
 * This file is #included by regexec.c.
 */

/*
 - longest - longest-preferred matching engine
 ^ static chr *longest(struct vars *, struct dfa *, chr *, chr *);
 */
static chr *			/* endpoint, or NULL */
longest(v, d, start, stop)
struct vars *v;			/* used only for debug and exec flags */
struct dfa *d;
chr *start;			/* where the match should start */
chr *stop;			/* match must end at or before here */
{
	chr *cp;
	chr *realstop = (stop == v->stop) ? stop : stop + 1;
	color co;
	struct sset *css;
	struct sset *ss;
	chr *post;
	int i;
	struct colormap *cm = d->cm;

	/* initialize */
	css = initialize(v, d, start);
	cp = start;

	/* startup */
	FDEBUG(("+++ startup +++\n"));
	if (cp == v->start) {
		co = d->cnfa->bos[(v->eflags&REG_NOTBOL) ? 0 : 1];
		FDEBUG(("color %ld\n", (long)co));
	} else {
		co = GETCOLOR(cm, *(cp - 1));
		FDEBUG(("char %c, color %ld\n", (char)*(cp-1), (long)co));
	}
	css = miss(v, d, css, co, cp, start);
	if (css == NULL)
		return NULL;
	css->lastseen = cp;

	/* main loop */
	if (v->eflags&REG_FTRACE)
		while (cp < realstop) {
			FDEBUG(("+++ at c%d +++\n", css - d->ssets));
			co = GETCOLOR(cm, *cp);
			FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1, start);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
		}
	else
		while (cp < realstop) {
			co = GETCOLOR(cm, *cp);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1, start);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
		}

	/* shutdown */
	FDEBUG(("+++ shutdown at c%d +++\n", css - d->ssets));
	if (cp == v->stop && stop == v->stop) {
		/* Addition */
		v->details.endReached = 1;
		/* End addition */
		co = d->cnfa->eos[(v->eflags&REG_NOTEOL) ? 0 : 1];
		FDEBUG(("color %ld\n", (long)co));
		ss = miss(v, d, css, co, cp, start);
		/* special case:  match ended at eol? */
		if (ss != NULL && (ss->flags&POSTSTATE))
			return cp;
		else if (ss != NULL)
			ss->lastseen = cp;	/* to be tidy */
	}

	/* find last match, if any */
	post = d->lastpost;
	for (ss = d->ssets, i = 0; i < d->nssused; ss++, i++)
		if ((ss->flags&POSTSTATE) && post != ss->lastseen &&
					(post == NULL || post < ss->lastseen))
			post = ss->lastseen;
	if (post != NULL)		/* found one */
		return post - 1;

	return NULL;
}

/*
 - shortest - shortest-preferred matching engine
 ^ static chr *shortest(struct vars *, struct dfa *, chr *, chr *, chr *,
 ^ 	chr **);
 */
static chr *			/* endpoint, or NULL */
shortest(v, d, start, min, max, coldp)
struct vars *v;			/* used only for debug and exec flags */
struct dfa *d;
chr *start;			/* where the match should start */
chr *min;			/* match must end at or after here */
chr *max;			/* match must end at or before here */
chr **coldp;			/* store coldstart pointer here, if nonNULL */
{
	chr *cp;
	chr *realmin = (min == v->stop) ? min : min + 1;
	chr *realmax = (max == v->stop) ? max : max + 1;
	color co;
	struct sset *css;
	struct sset *ss;
	struct colormap *cm = d->cm;
	chr *nopr;
	int i;

	/* initialize */
	css = initialize(v, d, start);
	cp = start;

	/* startup */
	FDEBUG(("--- startup ---\n"));
	if (cp == v->start) {
		co = d->cnfa->bos[(v->eflags&REG_NOTBOL) ? 0 : 1];
		FDEBUG(("color %ld\n", (long)co));
	} else {
		co = GETCOLOR(cm, *(cp - 1));
		FDEBUG(("char %c, color %ld\n", (char)*(cp-1), (long)co));
	}
	css = miss(v, d, css, co, cp, start);
	if (css == NULL)
		return NULL;
	css->lastseen = cp;
	ss = css;

	/* main loop */
	if (v->eflags&REG_FTRACE)
		while (cp < realmax) {
			FDEBUG(("--- at c%d ---\n", css - d->ssets));
			co = GETCOLOR(cm, *cp);
			FDEBUG(("char %c, color %ld\n", (char)*cp, (long)co));
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1, start);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
			if ((ss->flags&POSTSTATE) && cp >= realmin)
				break;		/* NOTE BREAK OUT */
		}
	else
		while (cp < realmax) {
			co = GETCOLOR(cm, *cp);
			ss = css->outs[co];
			if (ss == NULL) {
				ss = miss(v, d, css, co, cp+1, start);
				if (ss == NULL)
					break;	/* NOTE BREAK OUT */
			}
			cp++;
			ss->lastseen = cp;
			css = ss;
			if ((ss->flags&POSTSTATE) && cp >= realmin)
				break;		/* NOTE BREAK OUT */
		}

	if (ss == NULL)
		return NULL;
	else if (ss->flags&POSTSTATE) {
		assert(cp >= realmin);
		cp--;
	} else if (cp == v->stop && max == v->stop) {
		/* Addition */
		v->details.endReached = 1;
		/* End addition */
		co = d->cnfa->eos[(v->eflags&REG_NOTEOL) ? 0 : 1];
		FDEBUG(("color %ld\n", (long)co));
		ss = miss(v, d, css, co, cp, start);
		/* match might have ended at eol */
	}

	if (ss == NULL || !(ss->flags&POSTSTATE))
		return NULL;

	/* find last no-progress state set, if any */
	nopr = d->lastnopr;
	for (ss = d->ssets, i = 0; i < d->nssused; ss++, i++)
		if ((ss->flags&NOPROGRESS) && nopr != ss->lastseen &&
					(nopr == NULL || nopr < ss->lastseen))
			nopr = ss->lastseen;
	assert(nopr != NULL);
	if (coldp != NULL)
		*coldp = (nopr == v->start) ? nopr : nopr-1;
	return cp;
}

/*
 - newdfa - set up a fresh DFA
 ^ static struct dfa *newdfa(struct vars *, struct cnfa *,
 ^ 	struct colormap *, struct smalldfa *);
 */
static struct dfa *
newdfa(v, cnfa, cm, small)
struct vars *v;
struct cnfa *cnfa;
struct colormap *cm;
struct smalldfa *small;		/* preallocated space, may be NULL */
{
	struct dfa *d;
	size_t nss = cnfa->nstates * 2;
	int wordsper = (cnfa->nstates + UBITS - 1) / UBITS;
	struct smalldfa *smallwas = small;

	assert(cnfa != NULL && cnfa->nstates != 0);

	if (nss <= FEWSTATES && cnfa->ncolors <= FEWCOLORS) {
		assert(wordsper == 1);
		if (small == NULL) {
			small = (struct smalldfa *)MALLOC(
						sizeof(struct smalldfa));
			if (small == NULL) {
				ERR(REG_ESPACE);
				return NULL;
			}
		}
		d = &small->dfa;
		d->ssets = small->ssets;
		d->statesarea = small->statesarea;
		d->work = &d->statesarea[nss];
		d->outsarea = small->outsarea;
		d->incarea = small->incarea;
		d->cptsmalloced = 0;
		d->mallocarea = (smallwas == NULL) ? (char *)small : NULL;
	} else {
		d = (struct dfa *)MALLOC(sizeof(struct dfa));
		if (d == NULL) {
			ERR(REG_ESPACE);
			return NULL;
		}
		d->ssets = (struct sset *)MALLOC(nss * sizeof(struct sset));
		d->statesarea = (unsigned *)MALLOC((nss+WORK) * wordsper *
							sizeof(unsigned));
		d->work = &d->statesarea[nss * wordsper];
		d->outsarea = (struct sset **)MALLOC(nss * cnfa->ncolors *
							sizeof(struct sset *));
		d->incarea = (struct arcp *)MALLOC(nss * cnfa->ncolors *
							sizeof(struct arcp));
		d->cptsmalloced = 1;
		d->mallocarea = (char *)d;
		if (d->ssets == NULL || d->statesarea == NULL ||
				d->outsarea == NULL || d->incarea == NULL) {
			freedfa(d);
			ERR(REG_ESPACE);
			return NULL;
		}
	}

	d->nssets = (v->eflags&REG_SMALL) ? 7 : nss;
	d->nssused = 0;
	d->nstates = cnfa->nstates;
	d->ncolors = cnfa->ncolors;
	d->wordsper = wordsper;
	d->cnfa = cnfa;
	d->cm = cm;
	d->lastpost = NULL;
	d->lastnopr = NULL;
	d->search = d->ssets;

	/* initialization of sset fields is done as needed */

	return d;
}

/*
 - freedfa - free a DFA
 ^ static VOID freedfa(struct dfa *);
 */
static VOID
freedfa(d)
struct dfa *d;
{
	if (d->cptsmalloced) {
		if (d->ssets != NULL)
			FREE(d->ssets);
		if (d->statesarea != NULL)
			FREE(d->statesarea);
		if (d->outsarea != NULL)
			FREE(d->outsarea);
		if (d->incarea != NULL)
			FREE(d->incarea);
	}

	if (d->mallocarea != NULL)
		FREE(d->mallocarea);
}

/*
 - hash - construct a hash code for a bitvector
 * There are probably better ways, but they're more expensive.
 ^ static unsigned hash(unsigned *, int);
 */
static unsigned
hash(uv, n)
unsigned *uv;
int n;
{
	int i;
	unsigned h;

	h = 0;
	for (i = 0; i < n; i++)
		h ^= uv[i];
	return h;
}

/*
 - initialize - hand-craft a cache entry for startup, otherwise get ready
 ^ static struct sset *initialize(struct vars *, struct dfa *, chr *);
 */
static struct sset *
initialize(v, d, start)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
chr *start;
{
	struct sset *ss;
	int i;

	/* is previous one still there? */
	if (d->nssused > 0 && (d->ssets[0].flags&STARTER))
		ss = &d->ssets[0];
	else {				/* no, must (re)build it */
		ss = getvacant(v, d, start, start);
		for (i = 0; i < d->wordsper; i++)
			ss->states[i] = 0;
		BSET(ss->states, d->cnfa->pre);
		ss->hash = HASH(ss->states, d->wordsper);
		assert(d->cnfa->pre != d->cnfa->post);
		ss->flags = STARTER|LOCKED|NOPROGRESS;
		/* lastseen dealt with below */
	}

	for (i = 0; i < d->nssused; i++)
		d->ssets[i].lastseen = NULL;
	ss->lastseen = start;		/* maybe untrue, but harmless */
	d->lastpost = NULL;
	d->lastnopr = NULL;
	return ss;
}

/*
 - miss - handle a cache miss
 ^ static struct sset *miss(struct vars *, struct dfa *, struct sset *,
 ^ 	pcolor, chr *, chr *);
 */
static struct sset *		/* NULL if goes to empty set */
miss(v, d, css, co, cp, start)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
struct sset *css;
pcolor co;
chr *cp;			/* next chr */
chr *start;			/* where the attempt got started */
{
	struct cnfa *cnfa = d->cnfa;
	int i;
	unsigned h;
	struct carc *ca;
	struct sset *p;
	int ispost;
	int noprogress;
	int gotstate;
	int dolacons;
	int didlacons;

	/* for convenience, we can be called even if it might not be a miss */
	if (css->outs[co] != NULL) {
		FDEBUG(("hit\n"));
		return css->outs[co];
	}
	FDEBUG(("miss\n"));

	/* first, what set of states would we end up in? */
	for (i = 0; i < d->wordsper; i++)
		d->work[i] = 0;
	ispost = 0;
	noprogress = 1;
	gotstate = 0;
	for (i = 0; i < d->nstates; i++)
		if (ISBSET(css->states, i))
			for (ca = cnfa->states[i]+1; ca->co != COLORLESS; ca++)
				if (ca->co == co) {
					BSET(d->work, ca->to);
					gotstate = 1;
					if (ca->to == cnfa->post)
						ispost = 1;
					if (!cnfa->states[ca->to]->co)
						noprogress = 0;
					FDEBUG(("%d -> %d\n", i, ca->to));
				}
	dolacons = (gotstate) ? (cnfa->flags&HASLACONS) : 0;
	didlacons = 0;
	while (dolacons) {		/* transitive closure */
		dolacons = 0;
		for (i = 0; i < d->nstates; i++)
			if (ISBSET(d->work, i))
				for (ca = cnfa->states[i]+1; ca->co != COLORLESS;
									ca++)
					if (ca->co > cnfa->ncolors &&
						!ISBSET(d->work, ca->to) &&
							lacon(v, cnfa, cp,
								ca->co)) {
						BSET(d->work, ca->to);
						dolacons = 1;
						didlacons = 1;
						if (ca->to == cnfa->post)
							ispost = 1;
						if (!cnfa->states[ca->to]->co)
							noprogress = 0;
						FDEBUG(("%d :> %d\n",i,ca->to));
					}
	}
	if (!gotstate)
		return NULL;
	h = HASH(d->work, d->wordsper);

	/* next, is that in the cache? */
	for (p = d->ssets, i = d->nssused; i > 0; p++, i--)
		if (HIT(h, d->work, p, d->wordsper)) {
#ifndef xxx
p->hash == h && 
memcmp(VS(d->work), VS(p->states),
					d->wordsper*sizeof(unsigned)) == 0) {
#endif
			FDEBUG(("cached c%d\n", p - d->ssets));
			break;			/* NOTE BREAK OUT */
		}
	if (i == 0) {		/* nope, need a new cache entry */
		p = getvacant(v, d, cp, start);
		assert(p != css);
		for (i = 0; i < d->wordsper; i++)
			p->states[i] = d->work[i];
		p->hash = h;
		p->flags = (ispost) ? POSTSTATE : 0;
		if (noprogress)
			p->flags |= NOPROGRESS;
		/* lastseen to be dealt with by caller */
	}

	if (!didlacons) {		/* lookahead conds. always cache miss */
		css->outs[co] = p;
		css->inchain[co] = p->ins;
		p->ins.ss = css;
		p->ins.co = (color)co;
	}
	return p;
}

/*
 - lacon - lookahead-constraint checker for miss()
 ^ static int lacon(struct vars *, struct cnfa *, chr *, pcolor);
 */
static int			/* predicate:  constraint satisfied? */
lacon(v, pcnfa, cp, co)
struct vars *v;
struct cnfa *pcnfa;		/* parent cnfa */
chr *cp;
pcolor co;			/* "color" of the lookahead constraint */
{
	int n;
	struct subre *sub;
	struct dfa *d;
	struct smalldfa sd;
	chr *end;

	n = co - pcnfa->ncolors;
	assert(n < v->g->nlacons && v->g->lacons != NULL);
	FDEBUG(("=== testing lacon %d\n", n));
	sub = &v->g->lacons[n];
	d = newdfa(v, &sub->cnfa, &v->g->cmap, &sd);
	if (d == NULL) {
		ERR(REG_ESPACE);
		return 0;
	}
	end = longest(v, d, cp, v->stop);
	freedfa(d);
	FDEBUG(("=== lacon %d match %d\n", n, (end != NULL)));
	return (sub->subno) ? (end != NULL) : (end == NULL);
}

/*
 - getvacant - get a vacant state set
 * This routine clears out the inarcs and outarcs, but does not otherwise
 * clear the innards of the state set -- that's up to the caller.
 ^ static struct sset *getvacant(struct vars *, struct dfa *, chr *, chr *);
 */
static struct sset *
getvacant(v, d, cp, start)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
chr *cp;
chr *start;
{
	int i;
	struct sset *ss;
	struct sset *p;
	struct arcp ap;
	struct arcp lastap;
	color co;

	ss = pickss(v, d, cp, start);
	assert(!(ss->flags&LOCKED));

	/* clear out its inarcs, including self-referential ones */
	ap = ss->ins;
	while ((p = ap.ss) != NULL) {
		co = ap.co;
		FDEBUG(("zapping c%d's %ld outarc\n", p - d->ssets, (long)co));
		p->outs[co] = NULL;
		ap = p->inchain[co];
		p->inchain[co].ss = NULL;	/* paranoia */
	}
	ss->ins.ss = NULL;

	/* take it off the inarc chains of the ssets reached by its outarcs */
	for (i = 0; i < d->ncolors; i++) {
		p = ss->outs[i];
		assert(p != ss);		/* not self-referential */
		if (p == NULL)
			continue;		/* NOTE CONTINUE */
		FDEBUG(("del outarc %d from c%d's in chn\n", i, p - d->ssets));
		if (p->ins.ss == ss && p->ins.co == i)
			p->ins = ss->inchain[i];
		else {
			assert(p->ins.ss != NULL);
			for (ap = p->ins; ap.ss != NULL &&
						!(ap.ss == ss && ap.co == i);
						ap = ap.ss->inchain[ap.co])
				lastap = ap;
			assert(ap.ss != NULL);
			lastap.ss->inchain[lastap.co] = ss->inchain[i];
		}
		ss->outs[i] = NULL;
		ss->inchain[i].ss = NULL;
	}

	/* if ss was a success state, may need to remember location */
	if ((ss->flags&POSTSTATE) && ss->lastseen != d->lastpost &&
			(d->lastpost == NULL || d->lastpost < ss->lastseen))
		d->lastpost = ss->lastseen;

	/* likewise for a no-progress state */
	if ((ss->flags&NOPROGRESS) && ss->lastseen != d->lastnopr &&
			(d->lastnopr == NULL || d->lastnopr < ss->lastseen))
		d->lastnopr = ss->lastseen;

	return ss;
}

/*
 - pickss - pick the next stateset to be used
 ^ static struct sset *pickss(struct vars *, struct dfa *, chr *, chr *);
 */
static struct sset *
pickss(v, d, cp, start)
struct vars *v;			/* used only for debug flags */
struct dfa *d;
chr *cp;
chr *start;
{
	int i;
	struct sset *ss;
	struct sset *end;
	chr *ancient;

	/* shortcut for cases where cache isn't full */
	if (d->nssused < d->nssets) {
		i = d->nssused;
		d->nssused++;
		ss = &d->ssets[i];
		FDEBUG(("new c%d\n", i));
		/* set up innards */
		ss->states = &d->statesarea[i * d->wordsper];
		ss->flags = 0;
		ss->ins.ss = NULL;
		ss->ins.co = WHITE;		/* give it some value */
		ss->outs = &d->outsarea[i * d->ncolors];
		ss->inchain = &d->incarea[i * d->ncolors];
		for (i = 0; i < d->ncolors; i++) {
			ss->outs[i] = NULL;
			ss->inchain[i].ss = NULL;
		}
		return ss;
	}

	/* look for oldest, or old enough anyway */
	if (cp - start > d->nssets*2/3)		/* oldest 33% are expendable */
		ancient = cp - d->nssets*2/3;
	else
		ancient = start;
	for (ss = d->search, end = &d->ssets[d->nssets]; ss < end; ss++)
		if ((ss->lastseen == NULL || ss->lastseen < ancient) &&
							!(ss->flags&LOCKED)) {
			d->search = ss + 1;
			FDEBUG(("replacing c%d\n", ss - d->ssets));
			return ss;
		}
	for (ss = d->ssets, end = d->search; ss < end; ss++)
		if ((ss->lastseen == NULL || ss->lastseen < ancient) &&
							!(ss->flags&LOCKED)) {
			d->search = ss + 1;
			FDEBUG(("replacing c%d\n", ss - d->ssets));
			return ss;
		}

	/* nobody's old enough?!? -- something's really wrong */
	FDEBUG(("can't find victim to replace!\n"));
	assert(NOTREACHED);
	ERR(REG_ASSERT);
	return d->ssets;
}


static Tcl_RegExp
getRegexp(interp, reObj, flags)
    Tcl_Interp *interp;
    Tcl_Obj    *reObj;
    int        flags;
{
    return Tcl_GetRegExpFromObj(interp, reObj,
		REG_ADVANCED
		| ((flags & LEXER_FLAG_LINES)  ? REG_NEWLINE : 0)
		| ((flags & LEXER_FLAG_NOCASE) ? REG_ICASE   : 0));
}




/* This is a modified version of TclRegExpExecUniChar
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
    int status, flags = 0;
    TclRegexp *regexpPtr = (TclRegexp *)getRegexp(interp, rule->reObj, lexer->flags);
    int numChars = Tcl_GetCharLength(stringObj);
    Tcl_UniChar *string = Tcl_GetUnicode(stringObj);

    /* Addition */
    rm_detail_t details;

    /* Error checking */
    if (regexpPtr == NULL)
	return -1;

    /* Only match at the beginning of the specified string */
    flags |= REG_BEGINNING;

    /* Check for string overrun */
    flags |= REG_REPORTEOS;

    /* Check for beginning of (line|string) */
    flags |= (   (index == start)
	   || (   (regexpPtr->flags & REG_NLANCH)
	       && (string[index-1] == '\n')))
	  ? 0 : REG_NOTBOL;
    /* End addition */

    /*
     * Perform the regexp match.
     */

    /* Modification */
    /*
    status = exec(&regexpPtr->re, string, (size_t) numChars,
	(rm_detail_t*)NULL, regexpPtr->re.re_nsub + 1, regexpPtr->matches,
	((string > start) ? REG_NOTBOL : 0));
    */
    status = exec(&regexpPtr->re, string+index, (size_t) numChars-index,
		&details, regexpPtr->re.re_nsub + 1,
		regexpPtr->matches, flags);
    *pOverrun = details.endReached;
    /* End modification */

    /*
     * Check for errors.
     */

    if (status != REG_OKAY) {
	if (status == REG_NOMATCH) {
	    return 0;
	}
	if (interp != NULL) {
#ifdef USE_TCL_STUBS
	    Tcl_SetResult(interp, "error while matching regular expression",
		TCL_STATIC);
#else
	    TclRegError(interp, "error while matching regular expression: ",
		status);
#endif
	}
	return -1;
    }
    return 1;
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
