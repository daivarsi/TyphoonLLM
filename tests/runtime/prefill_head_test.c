
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "session.h"
#include "profile.h"
uint64_t prof_us[31][P_COUNT],prof_layer[30],prof_forward_us,prof_calls,prof_batch_calls;
static float scores[32];static int state,forwarded,heads,last_scored,checks,cancel_after;
void dbg_log(const char*fmt,...){(void)fmt;}
int engine_get_context(const Transformer*t){(void)t;return 512;}
void engine_reset_context(Transformer*t){(void)t;state=forwarded=heads=0;last_scored=-1;memset(scores,0,sizeof(scores));}
static void advance(int token,int pos){assert(pos==forwarded);state=(state*3+token+pos)%28;forwarded++;}
static float*project(int pos,int need){if(need){for(int i=0;i<32;i++)scores[i]=-1;scores[state]=1;heads++;last_scored=pos;}return scores;}
float*engine_prefill(Transformer*t,int token,int pos,int need){(void)t;advance(token,pos);return project(pos,need);}
float*engine_prefill_batch4(Transformer*t,const int*x,int pos,int need){(void)t;for(int j=0;j<4;j++)advance(x[j],pos+j);return project(pos+3,need);}
float*engine_forward(Transformer*t,int token,int pos){return engine_prefill(t,token,pos,1);}
char*engine_decode(Tokenizer*t,int token){(void)t;(void)token;return "word ";}
static int cancel(void*u){(void)u;return cancel_after>=0&&forwarded>=cancel_after;}
int main(void){
 Transformer model={0};model.config.vocab_size=32;Tokenizer tok={0};tok.eos_id=29;tok.im_start_id=30;tok.im_end_id=31;
 int ids[40];for(int i=0;i<40;i++)ids[i]=(i%23)+1;
 for(int skip=0;skip<=1;skip++)for(int n=1;n<=25;n++){
  Session s;session_init(&s,&model,&tok);session_set_prefill_head_skip(skip);cancel_after=-1;s.cancelled=cancel;
  assert(session_start(&s,ids,n,4,8,0)==0);assert(forwarded==n&&s.pending==state&&last_scored==n-1);
  assert(heads==(skip?1:n/4+n%4));assert(s.pos==n&&s.has_pending);checks++;
 }
 Session s;session_init(&s,&model,&tok);session_set_prefill_head_skip(1);cancel_after=-1;s.cancelled=cancel;
 assert(session_start(&s,ids,8,4,8,0)==0);heads=0;assert(session_start(&s,ids,13,4,8,1)==0);assert(s.reused==8&&forwarded==13&&heads==1&&last_scored==12&&s.pending==state);checks++;
 cancel_after=4;assert(session_start(&s,ids,20,4,8,0)==-4);assert(s.reason==END_STOP&&forwarded==4&&heads==0);checks++;
 cancel_after=-1;assert(session_start(&s,ids,20,4,8,0)==0);assert(heads==1&&s.pending==state);checks++;
 for(int i=0;i<13;i++)ids[i]=(i%17)+2;assert(session_start(&s,ids,13,4,8,1)==0);assert(s.rebuilt&&s.reused==0&&forwarded==13&&heads==1&&last_scored==12);checks++;
 puts("54 prefill boundary/reuse/cancellation checks passed");return 0;
}
