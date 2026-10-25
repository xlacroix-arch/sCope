/*  Fondement Michtam
 *  Copyright (C) 2026 Xavier Lacroix
 *  GPL v2 or later, see the other Tools_ files.
 */

#include <Tools_Expr.h>

typedef struct {
    TlsTok *t;
    TlsExprEnv *env;
    char const *err;
    int c;                              /* the token that is being looked at */
    int skip;                           /* > 0: what is read is not evaluated */
} St;

static void Adv(St *s) { s->c = s->err ? TokNil : x_(s->t,Next); }
static long long Fail(St *s,char const *msg) {
    if (!s->err) s->err = msg;
    s->c = TokNil;
    return 0;
}

static long long Cond(St *s);

static long long Unary(St *s) {
    long long v;
    switch (s->c) {
    case '+': Adv(s); return Unary(s);
    case '-': Adv(s); return (long long)(0-(unsigned long long)Unary(s));
    case '!': Adv(s); return !Unary(s);
    case '~': Adv(s); return ~Unary(s);
    case '(':
        Adv(s);
        v = Cond(s);
        if (s->c!=')') return Fail(s,"')' expected");
        Adv(s);
        return v;
    case TokInt: case TokChar:
        v = (long long)s->t->Val;
        Adv(s);
        return v;
    case TokLabel:
        v = 0;
        if (!s->skip && (!s->env || !x_(s->env,Label,(int)s->t->Val,&v))) return Fail(s,"name without a value");
        Adv(s);
        return v;
    case TokFloat: return Fail(s,"number with a fraction in an expression");
    case TokNil: case TokNl: return Fail(s,"expression expected");
    }
    return Fail(s,"bad token in an expression");
}

static int Prec(int c) {
    switch (c) {
    case TokOp(OrOr): return 1;
    case TokOp(AndAnd): return 2;
    case '|': return 3;
    case '^': return 4;
    case '&': return 5;
    case TokOp(Eq): case TokOp(Ne): return 6;
    case '<': case '>': case TokOp(Le): case TokOp(Ge): return 7;
    case TokOp(Shl): case TokOp(Shr): return 8;
    case '+': case '-': return 9;
    case '*': case '/': case '%': return 10;
    }
    return 0;
}

static long long Apply(St *s,int op,long long a,long long b) {
    unsigned long long ua = (unsigned long long)a,ub = (unsigned long long)b;
    switch (op) {
    case '|': return a|b;
    case '^': return a^b;
    case '&': return a&b;
    case TokOp(Eq): return a==b;
    case TokOp(Ne): return a!=b;
    case '<': return a<b;
    case '>': return a>b;
    case TokOp(Le): return a<=b;
    case TokOp(Ge): return a>=b;
    case TokOp(Shl): return b<0 || b>=64 ? 0 : (long long)(ua<<b);
    case TokOp(Shr): return b<0 || b>=64 ? (a<0 ? -1 : 0) : a>>b;
    case '+': return (long long)(ua+ub);
    case '-': return (long long)(ua-ub);
    case '*': return (long long)(ua*ub);
    default:                                            /* / % */
        if (!b) { if (!s->skip) Fail(s,"division by 0"); return 0; }
        if (b==-1) return op=='/' ? (long long)(0-ua) : 0;
        return op=='/' ? a/b : a%b;
    }
}

static long long Bin(St *s,int min) {
    long long l = Unary(s);
    int p;
    while ((p = Prec(s->c))>=min && p) {
        int op = s->c;
        long long r;
        Adv(s);
        if (op==TokOp(OrOr) || op==TokOp(AndAnd)) {
            int cut = op==TokOp(OrOr) ? l!=0 : l==0;    /* the right side is not evaluated */
            s->skip += cut;
            r = Bin(s,p+1);
            s->skip -= cut;
            l = op==TokOp(OrOr) ? (l || r) : (l && r);
        } else {
            r = Bin(s,p+1);
            l = Apply(s,op,l,r);
        }
    }
    return l;
}

static long long Cond(St *s) {
    long long v = Bin(s,1),a,b;
    if (s->c!='?') return v;
    Adv(s);
    s->skip += !v; a = Cond(s); s->skip -= !v;
    if (s->c!=':') return Fail(s,"':' expected");
    Adv(s);
    s->skip += v!=0; b = Cond(s); s->skip -= v!=0;
    return v ? a : b;
}

char const *TlsExprEval(TlsTok *t,TlsExprEnv *env,long long *v) {
    St s;
    long long r;
    s.t = t; s.env = env; s.err = 0; s.skip = 0;
    Adv(&s);
    r = Cond(&s);
    if (!s.err && s.c!=TokNil && s.c!=TokNl) Fail(&s,"bad token in an expression");
    if (s.err) return s.err;
    *v = r;
    return 0;
}
