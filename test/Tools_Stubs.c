/* Minimal stand-ins for the parts of Tools_Core.h that Tools_Stack.c needs
 * and that live in other files: TlsListEnd and TlsIntCounter. Test only. */
#include <Tools_Core.h>

TlsList TlsListEnd = {&TlsListEnd};

static int NullCounterOp(TlsIntCounter *this,int v) { (void)this; (void)v; return 0; }
static struct TlsIntCounter NullCounterStatic = {NullCounterOp,NullCounterOp};
TlsIntCounter TlsIntCounterNull = {&NullCounterStatic};

typedef struct {
    TlsIntCounter TlsIntCounter;
    int v;
} Direct;
static int DirectAdd(TlsIntCounter *this,int val) {
    TlsGetThat(Direct,TlsIntCounter);
    that->v += val;
    return that->v;
}
static int DirectSet(TlsIntCounter *this,int val) {
    TlsGetThat(Direct,TlsIntCounter);
    that->v = val;
    return val;
}
TlsIntCounter *TlsIntCounterDirect(int val) {
    static struct TlsIntCounter Static = {DirectAdd,DirectSet};
    Direct *d;
    rPush(d);
    d->TlsIntCounter.Static = &Static;
    d->v = val;
    return &d->TlsIntCounter;
}
