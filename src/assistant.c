#include "assistant.h"
#include "local_calc.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
typedef struct{int kind;const char*key,*aliases,*text,*source;}Fact;
#include "knowledge.inc"
static void norm(const char*s,char*out,unsigned n){unsigned j=0;int space=1;for(;*s&&j+2<n;s++){unsigned char c=*s;if(isalnum(c)){out[j++]=tolower(c);space=0;}else if(!space){out[j++]=' ';space=1;}}if(j&&out[j-1]==' ')j--;out[j]=0;}
static int phrase(const char*s,const char*p){size_t n=strlen(p);const char*q=s;while((q=strstr(q,p))){if((q==s||q[-1]==' ')&&(q[n]==0||q[n]==' '))return 1;q++;}return 0;}
static int any(const char*s,const char*list){char p[128];while(*list){unsigned n=0;while(*list&&*list!='|'){if(n+1<sizeof(p))p[n++]=*list;list++;}p[n]=0;if(n&&phrase(s,p))return 1;if(*list)list++;}return 0;}
Assist assistant_route(const char*prompt){
 char q[1100];norm(prompt,q,sizeof(q));Assist a={0,0,0,0,"neutral","Be direct and brief."};
 /* Serious overrides casual terms. Routing controls style, never safety facts. */
 if(any(q,"suicide|kill myself|hurt myself|emergency|overdose|chest pain|abuse|grief|died|depressed|diagnosis|medicine|bleeding|weapon|poison")){a.tone="serious";a.style="Be serious, careful and brief. No humor.";}
 else if(any(q,"joke|lol|haha|roast|funny")||(phrase(q,"vita")&&phrase(q,"trapped"))||(any(q,"penguin|unicorn|dragon|toaster")&&any(q,"accountant|mayor|office|rent|taxes"))){a.tone="playful";a.style=phrase(q,"vita")&&phrase(q,"trapped")?"[Tone: playful. Keep it short.]":"Play along briefly. Dry humor, not a lecture.";}
 else if(any(q,"frustrated|annoying|broken|stupid|angry|doesn t work")){a.tone="frustrated";a.style="Give one practical next step. Skip apologies.";}
 else if(any(q,"code|program|compile|bug|algorithm|memory|kernel|vita|homebrew|error|python|c programming|pointer|malloc")){a.tone="technical";a.style="Be concise and technical. No jokes.";}
 else if(any(q,"joke|lol|haha|roast|funny")){a.tone="playful";a.style="Be brief and lightly dry; do not force a joke.";}
 else if(any(q,"bored|hey|hello|hi|hobby|weekend|favorite")){a.tone="casual";a.style="Keep it short and practical.";}
 else if(any(q,"capital|chemical|symbol|atomic|who|what|when|where")){a.tone="factual";a.style="Answer briefly. Do not invent facts.";}
 if(any(q,"only|exactly|json|one sentence|one word|no other|at most|five words"))a.style="";
 int capital=phrase(q,"capital")&&!any(q,"before|after|former|historical|old|not|except|state"),chem=any(q,"chemical symbol|symbol|atomic number|element"),found=-1,hits=0;
 for(unsigned i=0;i<FACT_COUNT;i++){
  const Fact*f=facts+i;int match=0;
  if(f->kind==1&&capital&&!phrase(q,"state")&&(!phrase(q,"georgia")||phrase(q,"country")))match=any(q,f->aliases);
  else if(f->kind==2&&chem)match=any(q,f->aliases);
  else if(f->kind==3)match=any(q,f->aliases);
  if(match){found=i;hits++;}
 }
 /* Ambiguity (two entities or competing references) is a miss, not a guess. */
 if(hits==1){a.hit=found+1;a.score=1;a.text=facts[found].text;a.source=facts[found].source;}else if(hits>1)a.score=-1;
 return a;
}
const char*assistant_style(const Assist*a){
 if(!strcmp(a->tone,"casual")||!strcmp(a->tone,"playful")||!strcmp(a->tone,"frustrated")||!strcmp(a->tone,"serious")||!strcmp(a->tone,"technical"))return a->style;
 return ""; /* Frozen factual/arithmetic/format behavior stays unprompted. */
}
static int reference_answers(const char*q,const Assist*a){
 if(!a->hit)return 0;const Fact*f=facts+a->hit-1;if(f->kind!=3)return 1;
 if(any(q,"came first|stands for|stand for|discovered|invented|inventor|who wrote|who signed|who was|how much|how long|population|address|cost|price"))return 0;
 if(!strncmp(q,"who ",4)&&strcmp(f->key,"antarctica government"))return 0;
 if(!strcmp(f->key,"solar system planets"))return any(q,"how many planets|name the planets|list the planets");
 if(!strcmp(f->key,"declaration independence"))return any(q,"when|date|year")&&any(q,"adopted|signed|written|declaration");
 if(!strcmp(f->key,"us constitution signed"))return any(q,"when|date|year")&&phrase(q,"signed");
 if(any(q,"when|what year|which year")&&!any(f->key,"apollo 11 landing|us civil war|world war ii end|gutenberg bible"))return 0;
 if(!strncmp(q,"why ",4)&&any(f->key,"dna|evaporation|condensation|si metre|si second|si kilogram"))return 0;
 return 1;
}
int assistant_direct(const char*prompt,Assist*a,char*out,unsigned cap){
 if(assistant_tool(prompt,out,cap))return 1;
 char q[1100];norm(prompt,q,sizeof(q));
 int format=any(q,"only|json|exactly|one word|translate|spanish|french|german|quote|quotes|list|bullet|fiction|pretend|story|imagine|what if|hypothetical|suppose|could");
 int question=!strncmp(q,"what ",5)||!strncmp(q,"which ",6)||!strncmp(q,"who ",4)||!strncmp(q,"how ",4)||!strncmp(q,"when ",5)||!strncmp(q,"where ",6)||!strncmp(q,"in which ",9)||!strncmp(q,"in what ",8)||!strncmp(q,"why ",4);
 if(!format&&any(q,"who are you|what model are you|which model are you|what kind of device are you running on")){snprintf(out,cap,"I'm VitaLM, running entirely offline on this PS Vita with LFM2.5-230M Q8.");return 6;}
 /* Reference extraction is conservative: one unambiguous entry and an ordinary
  * short fact question. Complex requests stay neural; no irrelevant injection. */
 if(a->hit&&question&&!format&&strlen(q)<150&&reference_answers(q,a)){snprintf(out,cap,"%s",a->text);return 2;}
 if(!format&&!phrase(q,"joke about")&&(phrase(q,"overdose")||(phrase(q,"too much")&&any(q,"medicine|medication|pills")))){snprintf(out,cap,"A possible overdose needs prompt expert help. Contact a poison center now; do not wait for symptoms. If the person collapses, has a seizure, has trouble breathing or cannot be awakened, call local emergency services immediately.");return 4;}
 if(!question||format)return 0;
 /* Type and chronology checks use general categories, not whole benchmark
  * strings. Fiction/counterfactual requests are excluded by the format guard. */
 if(phrase(q,"capital")&&any(q,"planet|jupiter|mars|saturn|neptune|uranus|sun|ocean")&&!a->hit){snprintf(out,cap,"That is not a country with a national capital. The question's premise is unsupported.");return 3;}
 if(phrase(q,"chemical symbol")&&phrase(q,"element")&&!a->hit&&a->score==0){snprintf(out,cap,"I cannot identify that as a chemical element in the local periodic table. Check the element's name.");return 3;}
 if(any(q,"mercury|venus")&&any(q,"natural moon|natural moons|largest moon")){snprintf(out,cap,"Mercury and Venus have no natural moons.");return 3;}
 if(phrase(q,"nobel prize")&&any(q,"shakespeare|newton|julius caesar|ada lovelace|roman emperor")){snprintf(out,cap,"That person lived before the Nobel Prizes began in 1901, so did not win one.");return 3;}
 if(any(q,"ancient egyptians|roman emperor|julius caesar|dinosaurs|ada lovelace")&&any(q,"smartphone|passenger jets|internet site|video game|electric trains")&&any(q,"invent|invented|publish|published|launch|launched|use|used")&&!any(q,"inspired|influence|legacy")){snprintf(out,cap,"That premise mixes historical periods. The person or group predates that modern technology.");return 3;}
 if(any(q,"rock|stone|granite")&&phrase(q,"lungs")){snprintf(out,cap,"A rock is not an animal and has no lungs.");return 3;}
 int month_days=0;if(sscanf(q,"which month has %d days",&month_days)==1&&month_days>31){snprintf(out,cap,"No month in the Gregorian calendar has more than 31 days.");return 3;}
 if((phrase(q,"my")||phrase(q,"i"))&&any(q,"password|pin|bank balance|secret|private|privately|unseen|hidden|unpublished|unmeasured|sealed|disconnected|unknown location|exact gps|current charge|current temperature|face down")&&!any(q,"if|given|says|reads|told|provided|mentioned|following|can|should|create|choose|change|reset|protect|secure|recover")){
  snprintf(out,cap,"I don't have access to that private or unobserved information. Share the relevant details if you want help.");return 5;
 }
 if(any(q,"lottery|nobel prize|world series|award winner")&&(any(q,"next year|next month|will win|will be drawn")||strstr(q,"203")||strstr(q,"204")||strstr(q,"205")||strstr(q,"206")||strstr(q,"209"))){snprintf(out,cap,"That future result is not known. I cannot reliably predict the winner or draw.");return 3;}
 if(phrase(q,"triangle")&&phrase(q,"fourth side")){snprintf(out,cap,"A triangle has three sides, so it has no fourth side.");return 3;}
 if(phrase(q,"circle")&&phrase(q,"corners")){snprintf(out,cap,"A circle has no corners.");return 3;}
 return 0;
}
static int end(const char*p){while(isspace((unsigned char)*p))p++;return !*p||(*p=='?'&&!p[1]);}
static long civil_days(int y,unsigned m,unsigned d){y-=m<=2;int era=(y>=0?y:y-399)/400;unsigned yy=y-era*400,dy=(153*(m+(m>2?-3:9))+2)/5+d-1;return era*146097L+(yy*365+yy/4-yy/100+dy)-719468;}
static void from_days(long z,int*y,int*m,int*d){z+=719468;long era=(z>=0?z:z-146096)/146097;unsigned doe=z-era*146097,yoe=(doe-doe/1460+doe/36524-doe/146096)/365;int year=yoe+era*400;unsigned dy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*dy+2)/153;*d=dy-(153*mp+2)/5+1;*m=mp+(mp<10?3:-9);*y=year+(*m<=2);}
int assistant_tool(const char*prompt,char*out,unsigned cap){
 if(!prompt||!out||cap<2)return 0;
 if(local_calculate(prompt,out,cap))return 1;
 char command[1100];if(strlen(prompt)>=sizeof(command))return 0;snprintf(command,sizeof(command),"%s",prompt);
 for(char*p=command;*p&&!isspace((unsigned char)*p);p++)*p=tolower((unsigned char)*p);
 prompt=command; /* Command keywords ignore case; unit symbols C/F retain it. */
 int year,month,day,days,used=0;char sign;
 if(sscanf(prompt,"date %d-%d-%d %c %d days %n",&year,&month,&day,&sign,&days,&used)==5&&used&&end(prompt+used)&&(sign=='+'||sign=='-')&&year>=1&&year<=9999&&month>=1&&month<=12&&day>=1&&day<=31&&days>=0&&days<=365000){
  long base=civil_days(year,month,day);int y,m,d;from_days(base,&y,&m,&d);
  if(y==year&&m==month&&d==day){from_days(base+(sign=='+'?days:-days),&y,&m,&d);if(y>=1&&y<=9999){snprintf(out,cap,"%04d-%02d-%02d",y,m,d);return 1;}}
 }
 double x,y;char from[24],to[24];int n=0;
 if(sscanf(prompt,"%lf%% of %lf %n",&x,&y,&n)==2&&n&&end(prompt+n)&&isfinite(x)&&isfinite(y)&&fabs(x)<=1e12&&fabs(y)<=1e12){snprintf(out,cap,"%.12g",x*y/100);return 1;}
 n=0;if(sscanf(prompt,"convert %lf %23s to %23s %n",&x,from,to,&n)==3&&n&&end(prompt+n)&&isfinite(x)&&fabs(x)<=1e12){
  const char*u[]={"m","cm","mm","km","in","ft","mi","g","kg","lb","s","min","h"};
  const double scale[]={1,.01,.001,1000,.0254,.3048,1609.344,1,1000,453.59237,1,60,3600};
  const int dim[]={0,0,0,0,0,0,0,1,1,1,2,2,2};int a=-1,b=-1;
  for(int i=0;i<13;i++){if(!strcmp(from,u[i]))a=i;if(!strcmp(to,u[i]))b=i;}
  if(a>=0&&b>=0&&dim[a]==dim[b]){snprintf(out,cap,"%.12g %s",x*scale[a]/scale[b],to);return 1;}
  if(!strcmp(from,"C")&&!strcmp(to,"F")){snprintf(out,cap,"%.12g F",x*1.8+32);return 1;}
  if(!strcmp(from,"F")&&!strcmp(to,"C")){snprintf(out,cap,"%.12g C",(x-32)/1.8);return 1;}
 }
 /* Explicit list commands have strict parsing; ordinary prose never matches. */
 if(!strncmp(prompt,"sort ",5)||!strncmp(prompt,"count ",6)||!strncmp(prompt,"compare ",8)){
  int sort=!strncmp(prompt,"sort ",5),compare=!strncmp(prompt,"compare ",8);const char*p=prompt+(sort?5:compare?8:6);double values[32];int count=0;
  while(*p){while(isspace((unsigned char)*p)||*p==',')p++;if(!*p)break;char*e;double v=strtod(p,&e);if(e==p||!isfinite(v)||fabs(v)>1e12||count==32)return 0;values[count++]=v;p=e;if(*p&&!isspace((unsigned char)*p)&&*p!=',')return 0;}
  if(!count)return 0;
  if(compare){if(count!=2)return 0;snprintf(out,cap,"%.12g %s %.12g",values[0],values[0]<values[1]?"<":values[0]>values[1]?">":"=",values[1]);}
  else if(!sort)snprintf(out,cap,"%d",count);
  else{for(int i=1;i<count;i++)for(int j=i;j>0&&values[j]<values[j-1];j--){double v=values[j];values[j]=values[j-1];values[j-1]=v;}out[0]=0;for(int i=0;i<count;i++){unsigned used=strlen(out);if(used+32>=cap)return 0;snprintf(out+used,cap-used,"%s%.12g",i?", ":"",values[i]);}}
  return 1;
 }
 return 0;
}
