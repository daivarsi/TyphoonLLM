/* Bounded whole-request arithmetic; no model-output parsing or history access. */
#include "local_calc.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { const char *p; int ok, depth, operations; } Parser;
static void ws(Parser *s) { while (isspace((unsigned char)*s->p)) s->p++; }
static int starts(const char *s, const char *p) {
 while (*p) { if (!*s || tolower((unsigned char)*s++) != *p++) return 0; } return 1;
}
static int word_op(Parser *s, const char *word) {
 const char *p=s->p; int spaced=isspace((unsigned char)*p); ws(s); size_t n=strlen(word);
 if (spaced && starts(s->p,word) && isspace((unsigned char)s->p[n])) { s->p+=n; return 1; }
 s->p=p; return 0;
}
/* Exact reduced rationals. Bounds make all checked intermediates portable
 * int64_t on both host and Vita. Out-of-bound requests fall through. */
#include <stdint.h>
typedef struct { int64_t n,d; } Value;
#define LIMIT INT64_C(1000000000000)
static int64_t magnitude(int64_t x) { return x<0?-x:x; }
static int64_t gcd64(int64_t a,int64_t b) { while(b){int64_t t=a%b;a=b;b=t;}return a; }
static Value value(Parser*s,int64_t n,int64_t d) {
 Value v={0,1}; if(!d){s->ok=0;return v;}
 if(d<0){n=-n;d=-d;} int64_t g=gcd64(magnitude(n),d);n/=g;d/=g;
 if(magnitude(n)>LIMIT || d>LIMIT){s->ok=0;return v;}v.n=n;v.d=d;return v;
}
static int64_t multiply(Parser*s,int64_t a,int64_t b) {
 if(b && magnitude(a)>LIMIT/magnitude(b)){s->ok=0;return 0;} return a*b;
}
static Value combine(Parser*s,Value a,Value b,int op) {
 if(op=='+' || op=='-') {
  int64_t g=gcd64(a.d,b.d),ad=a.d/g,bd=b.d/g;
  int64_t left=multiply(s,a.n,bd),right=multiply(s,b.n,ad);
  int64_t n=left+(op=='+'?1:-1)*right;
  int64_t d=multiply(s,ad,b.d); return value(s,n,d);
 }
 if(op=='/') { if(!b.n){s->ok=0;return (Value){0,1};}int64_t t=b.n;b.n=b.d;b.d=t; }
 if(b.d<0){b.n=-b.n;b.d=-b.d;}
 int64_t g=gcd64(magnitude(a.n),b.d),h=gcd64(magnitude(b.n),a.d);
 int64_t n=multiply(s,a.n/g,b.n/h),d=multiply(s,a.d/h,b.d/g);
 return value(s,n,d);
}
static Value expr(Parser *s);
static Value factor(Parser *s) {
 ws(s); if(++s->depth>16){s->ok=0;return (Value){0,1};}
 int sign=1;Value x={0,1};
 if(*s->p=='+' || *s->p=='-'){if(*s->p=='-')sign=-1;s->p++;ws(s);}
 if(*s->p=='('){s->p++;x=expr(s);ws(s);if(*s->p!=')')s->ok=0;else s->p++;}
 else {
  int digits=0;int64_t n=0,d=1;
  while(isdigit((unsigned char)*s->p)){
   int digit=*s->p++-'0';digits++;
   if(n>(LIMIT-digit)/10)s->ok=0;else n=n*10+digit;
  }
  if(*s->p=='.' && isdigit((unsigned char)s->p[1])){
   s->p++;
   while(isdigit((unsigned char)*s->p)){
    int digit=*s->p++-'0';digits++;
    if(n>(LIMIT-digit)/10 || d>LIMIT/10)s->ok=0;
    else {n=n*10+digit;d*=10;}
   }
  }
  if(!digits || digits>15)s->ok=0;
  x=value(s,n,d);
 }
 s->depth--;x.n*=sign;return x;
}
static int operation(Parser *s,int prod) {
 const char*p=s->p;ws(s);char c=*s->p;
 if(c && strchr(prod?"*/":"+-",c)){s->p++;s->operations++;return c;}
 s->p=p;
 if(word_op(s,prod?"times":"plus") || (prod && word_op(s,"multiplied by"))){s->operations++;return prod?'*':'+';}
 if(word_op(s,prod?"divided by":"minus")){s->operations++;return prod?'/':'-';}
 s->p=p;return 0;
}
static Value product(Parser*s){Value x=factor(s);int op;while(s->ok && (op=operation(s,1)))x=combine(s,x,factor(s),op);return x;}
static Value expr(Parser*s){Value x=product(s);int op;while(s->ok && (op=operation(s,0)))x=combine(s,x,product(s),op);return x;}
/* Integer long division, exact ties-to-even rounding. Default emits up to 15
 * fractional digits; fixed contracts emit exactly the requested 0..6 digits. */
static int render(Value v,int integer,int places,char*out,unsigned cap){
 if(integer && v.n%v.d)return 0;
 char buf[96];int64_t n=magnitude(v.n),rem=n%v.d;
 int len=snprintf(buf,sizeof(buf),"%lld",(long long)(n/v.d));
 int count=integer?0:places<0?15:places;
 if(count){buf[len++]='.';for(int i=0;i<count;i++){rem*=10;buf[len++]='0'+(char)(rem/v.d);rem%=v.d;}}
 if(!integer && (rem*2>v.d || (rem*2==v.d && ((buf[len-1]-'0')%2)))){
  int j=len-1;for(;j>=0;j--){if(buf[j]=='.')continue;if(buf[j]!='9'){buf[j]++;break;}buf[j]='0';}
  if(j<0){memmove(buf+1,buf,(size_t)len);buf[0]='1';len++;}
 }
 if(places<0 && count){while(buf[len-1]=='0')len--;if(buf[len-1]=='.')len--;}
 int nonzero=0;for(int i=0;i<len;i++)if(buf[i]>='1'&&buf[i]<='9')nonzero=1;
 int neg=v.n<0 && nonzero;
 if((unsigned)(len+neg)>=cap)return 0;
 if(neg)*out++='-';
 memcpy(out,buf,(size_t)len);out[len]=0;return 1;
}

static int equal(const char *s,const char *t) { return starts(s,t) && strlen(s)==strlen(t); }
int local_calculate(const char *prompt,char *out,unsigned capacity) {
 if (!prompt || strlen(prompt)>256 || !out || capacity<2) return 0;
 const char *p=prompt; while (isspace((unsigned char)*p)) p++;
 const char *prefixes[]={"what is ","calculate ","compute ","calc ","calc: "}; int found=0;
 for (unsigned i=0;i<sizeof(prefixes)/sizeof(*prefixes);i++)
  if (starts(p,prefixes[i])) { p+=strlen(prefixes[i]); found=1; break; }
 if (!found) return 0;
 /* Date-shaped triples are ambiguous under every command prefix. Compact
  * chains conservatively miss; spaced triples miss when a year and plausible
  * month/day fields make a date interpretation possible. */
 {
  /* Inspect a leading triple of optionally parenthesized unsigned fields.
   * Parentheses/whitespace may wrap each field without making a date safe.
   * This scan never extracts operands for calculation; it can only decline. */
  const char *q=p;int fields[3]={0},lengths[3]={0};char sep=0;int triple=1,compact=1;
  for(int i=0;i<3;i++) {
   while(isspace((unsigned char)*q) || *q=='(')q++;
   while(isdigit((unsigned char)*q)) {
    if(lengths[i]<4)fields[i]=fields[i]*10+(*q-'0');
    lengths[i]++;q++;
   }
   if(!lengths[i]){triple=0;break;}
   const char *after=q;
   while(isspace((unsigned char)*q) || *q==')')q++;
   if(q>after && isspace((unsigned char)*after))compact=0;
   if(i<2){
    if(!i)sep=*q;
    if((sep!='-' && sep!='/') || *q!=sep){triple=0;break;}q++;
    if(isspace((unsigned char)*q))compact=0;
   }
  }
  if(triple && compact)return 0;
  if(triple) {
   int year_first=lengths[0]==4 && lengths[1]<=2 && lengths[2]<=2 &&
      fields[0]>0 && fields[1]>=1 && fields[1]<=12 && fields[2]>=1 && fields[2]<=31;
   int year_last=lengths[2]==4 && lengths[0]<=2 && lengths[1]<=2 && fields[2]>0 &&
      fields[0]>=1 && fields[1]>=1 && ((fields[0]<=12 && fields[1]<=31) || (fields[0]<=31 && fields[1]<=12));
   if(year_first || year_last)return 0;
  }
 }

 Parser s={p,1,0,0}; Value result=expr(&s); ws(&s);
 if (!s.ok || !s.operations) return 0;
 if (*s.p=='?' || *s.p=='.') { s.p++; if (*s.p && !isspace((unsigned char)*s.p)) return 0; ws(&s); }
 char contract[180]; size_t n=strlen(s.p); if (n>=sizeof(contract)) return 0;
 memcpy(contract,s.p,n+1); while (n && isspace((unsigned char)contract[n-1])) contract[--n]=0;
 int integer=0, places=-1;
 if (!*contract || equal(contract,"reply with only the number.") || equal(contract,"return only the number.") ||
     equal(contract,"reply with only the decimal, using a period.")) { /* default decimal */ }
 else if (equal(contract,"reply with only the integer.") || equal(contract,"return only the integer.")) integer=1;
 else {
  const char *lead="give the answer to "; size_t k=strlen(lead);
  if (!starts(contract,lead) || contract[k]<'0' || contract[k]>'6' ||
      !equal(contract+k+1," decimal places.")) return 0;
  places=contract[k]-'0';
 }
 return render(result,integer,places,out,capacity);
}
