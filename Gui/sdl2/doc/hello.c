#include <stdio.h>
int ILg2(unsigned int v) {
    int r,p,p1;
    unsigned int v0;
    v=v|(v>>1); v=v|(v>>2); v=v|(v>>4); v=v|(v>>8); v=v|(v>>16);
    v0=v>>16; p=v0&1; r=p<<4; v= v^v0;
    v0=v>>8;  p= p^(v0&1); r|=p<<3; v=v^v0;
    v0=v>>4;  p= p^(v0&1); r|=p<<2; v=v^v0;
    v0=v>>2;  p= p^(v0&1); r|=p<<1; v=v^v0;
    p= p^((v>>1)&1); r|=p;
    return r;
}
int main() {
    int i; 
    for (i=0;i<256;i++) { printf("(%d,%d) ",i,ILg2(i)); } 
    printf("\n");
}
