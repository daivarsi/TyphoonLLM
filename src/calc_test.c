#include "local_calc.h"
#include <stdio.h>
#include <string.h>
int main(void){const char*cases[][2]={{"What is 17 + 28?","45"},{"calc: 6 * (4 + 3)","42"},{"Calculate -4 + 2","-2"},{"compute 10 / 4","2.5"},{"calc 1 - -3","4"},{"calc .5 * 8","4"},{"What is a whale?",NULL},{"What is 17 + 28 about?",NULL},{"calc 1/0",NULL},{"calc 1e99+1",NULL},{"calc nan+1",NULL},{"What is 1776?",NULL},{"calc (2+3",NULL},{"calc 2+3) garbage",NULL},{"calc 1+2; exit",NULL}};int bad=0;for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){char out[64];int ok=local_calculate(cases[i][0],out,sizeof(out));if(ok!=(cases[i][1]!=NULL)||(ok&&strcmp(out,cases[i][1]))){printf("FAIL %u\n",i);bad++;}}printf("calculator cases=15 failures=%d\n",bad);return bad?1:0;}
