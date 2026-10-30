/* All state belongs to the persistent generation worker; UI requests occur only
 * while idle. Token history retains the actual emitted IDs, avoiding BPE
 * re-tokenization changes when an assistant answer is appended. */
#include "session.h"
#include "../assistant.h"
static Session s_session;
static int s_session_ready,s_continue_job,s_multi_job,s_policy_job,s_last_multi,s_last_policy=-1;
static int s_soft_job=120,s_hard_job=160,s_reuse_enabled=1;
static int s_ledger[512],s_ledger_n,s_turn_offsets[65],s_turn_count,s_base_n;
static int s_evictions,s_cache_reused,s_cache_rebuilt;
static char s_route_name[24]="neutral",s_source[160],s_path_name[24]="neural";
static uint64_t s_begin_us,s_first_us,s_lookup_us,s_cache_us;
static void*s_prefix_snapshot;static size_t s_prefix_bytes;static int s_prefix_ids[32],s_prefix_n,s_prefix_used;
size_t engine_prefix_bytes(const Transformer*,int);
int engine_prefix_save(Transformer*,int,void*,size_t);
int engine_prefix_restore(Transformer*,int,void*,size_t);
static unsigned utf8_prefix(const char*text){unsigned i=0;while(text[i]){unsigned char c=text[i];if(c<128){i++;continue;}int n=c>=0xc2&&c<=0xdf?2:c>=0xe0&&c<=0xef?3:c>=0xf0&&c<=0xf4?4:0;if(!n)return i;for(int j=1;j<n;j++)if(!text[i+j]||((unsigned char)text[i+j]&0xc0)!=0x80)return i;if((c==0xe0&&(unsigned char)text[i+1]<0xa0)||(c==0xed&&(unsigned char)text[i+1]>=0xa0)||(c==0xf0&&(unsigned char)text[i+1]<0x90)||(c==0xf4&&(unsigned char)text[i+1]>=0x90))return i;i+=n;}return i;}
static void publish(const char*text,void*unused){(void)unused;if(!s_first_us&&*text)s_first_us=sceKernelGetProcessTimeWide();unsigned n=utf8_prefix(text);if(n>=sizeof(s_response))n=sizeof(s_response)-1;lock_io();memcpy(s_response,text,n);s_response[n]=0;unlock_io();}
static int cancelled(void*unused){(void)unused;return s_cancel;}
static int encode_bounded(const char*text,int*ids){int n=0;TIMED(30,P_TOKENIZER,engine_encode(&s_tokenizer,text,ids,&n););return n;}
static void escape_roles(char*q,unsigned capacity){char safe[1100];unsigned n=0;for(unsigned i=0;q[i]&&n+3<sizeof(safe);i++){safe[n++]=q[i];if(q[i]=='<'&&q[i+1]=='|')safe[n++]=' ';}safe[n]=0;snprintf(q,capacity,"%s",safe);}
static void session_ready(void){if(!s_session_ready){session_init(&s_session,&s_transformer,&s_tokenizer);s_session.emit=publish;s_session.cancelled=cancelled;s_session_ready=1;}}
static void phase4_prefix_init(void){
 if(s_prefix_snapshot)return;session_ready();s_prefix_n=encode_bounded("<|startoftext|><|im_start|>user\n",s_prefix_ids);
 if(s_prefix_n<1||s_prefix_n>32)return;s_prefix_bytes=engine_prefix_bytes(&s_transformer,s_prefix_n);s_prefix_snapshot=malloc(s_prefix_bytes);
 if(!s_prefix_snapshot)return;
 if(session_start(&s_session,s_prefix_ids,s_prefix_n,1,1,0)||engine_prefix_save(&s_transformer,s_prefix_n,s_prefix_snapshot,s_prefix_bytes)){
  free(s_prefix_snapshot);s_prefix_snapshot=0;session_reset(&s_session);return;
 }
 dbg_log("PREFIX_READY tokens=%d bytes=%u includes_attention_KV_and_all_convolution_state=1",s_prefix_n,(unsigned)s_prefix_bytes);
}
static void base_ledger(void){char base[512];
 if(s_policy_job==1||s_policy_job==2)snprintf(base,sizeof(base),"<|startoftext|><|im_start|>system\nYou are VitaLM on PS Vita. Answer directly, briefly and accurately. Finish your answer.<|im_end|>\n");
 else snprintf(base,sizeof(base),"<|startoftext|>");
 int ids[1024];s_base_n=encode_bounded(base,ids);memcpy(s_ledger,ids,s_base_n*sizeof(int));s_ledger_n=s_base_n;s_turn_count=0;
}
static int evict_turn(void){if(s_turn_count<1)return 0;int next=s_turn_count>1?s_turn_offsets[1]:s_ledger_n,drop=next-s_base_n;if(drop<=0)return 0;memmove(s_ledger+s_base_n,s_ledger+next,(s_ledger_n-next)*sizeof(int));s_ledger_n-=drop;for(int i=1;i<s_turn_count;i++)s_turn_offsets[i-1]=s_turn_offsets[i]-drop;s_turn_count--;s_evictions++;return 1;}
static void commit_response(void){
 if(!strcmp(s_path_name,"neural")){s_ledger_n=s_session.pos;memcpy(s_ledger,s_session.fed,s_ledger_n*sizeof(int));}
 int close[16],n=encode_bounded("<|im_end|>\n",close);if(s_ledger_n+n<=512){memcpy(s_ledger+s_ledger_n,close,n*sizeof(int));s_ledger_n+=n;}
}
static void phase4_generate(const char*prompt){
 s_hard_job=s_hard_job<1?1:s_hard_job>256?256:s_hard_job;s_soft_job=s_soft_job<1?s_hard_job:s_soft_job>s_hard_job?s_hard_job:s_soft_job;
 session_ready();prof_reset();s_begin_us=sceKernelGetProcessTimeWide();s_first_us=0;s_evictions=0;s_cache_reused=0;s_cache_rebuilt=0;s_lookup_us=0;s_cache_us=0;s_prefix_used=0;
 if(s_continue_job){
  snprintf(s_path_name,sizeof(s_path_name),"neural");int rc=session_continue(&s_session,s_soft_job,s_hard_job);
  if(rc<0&&s_session.reason==END_CONTEXT&&s_turn_count>1){
   /* Evict complete OLD turns only; retain the entire unfinished current turn.
    * Rebuild both state families at position zero after changing the prefix. */
   char old[SESSION_TEXT];snprintf(old,sizeof(old),"%s",s_session.text);s_ledger_n=s_session.pos;memcpy(s_ledger,s_session.fed,s_ledger_n*sizeof(int));
   while(s_turn_count>1&&s_ledger_n+s_hard_job+1>512)evict_turn();
   if(s_ledger_n<511){rc=session_start(&s_session,s_ledger,s_ledger_n,s_soft_job,s_hard_job,0);snprintf(s_session.text,sizeof(s_session.text),"%s",old);}
  }
  if(rc<0){s_session.reason=END_CONTEXT;publish(s_session.text,0);goto finish;}
  publish(s_session.text,0);s_first_us=0;
 }else{
  uint64_t lookup=sceKernelGetProcessTimeWide();Assist a=assistant_route(prompt);s_lookup_us=sceKernelGetProcessTimeWide()-lookup;
  snprintf(s_route_name,sizeof(s_route_name),"%s",a.tone);snprintf(s_source,sizeof(s_source),"%s",a.source?a.source:"");
  uint64_t cache=sceKernelGetProcessTimeWide();
  if(!s_multi_job||!s_last_multi||s_last_policy!=s_policy_job)base_ledger();
  char question[1100],body[4096];snprintf(question,sizeof(question),"%s",prompt?prompt:"");escape_roles(question,sizeof(question));int ids[4600],n;
  const char*style=s_policy_job==4?"Be brief; finish sentences.":s_policy_job==3?assistant_style(&a):s_policy_job?a.style:"";
  int style_before=s_policy_job==3&&strcmp(a.tone,"serious");
  for(;;){snprintf(body,sizeof(body),"<|im_start|>user\n%s%s%s%s%s%s<|im_end|>\n<|im_start|>assistant\n",style_before?style:question,*style?"\n":"",style_before?question:style,s_policy_job==2&&a.hit?"\nLocal reference: ":"",s_policy_job==2&&a.hit?a.text:"",s_policy_job==2&&a.hit?"\nUse only the reference. Add no unsupported facts.":"");n=encode_bounded(body,ids);
   if(s_base_n+n+s_hard_job+2<=512)break;
   if(!question[0]){set_error("Input cannot fit this response budget");return;}char*q=question+1;while((*q&0xc0)==0x80)q++;memmove(question,q,strlen(q)+1);
  }
  int history_limit=512-s_hard_job-2;
  if(s_ledger_n+n>history_limit&&history_limit>256)history_limit=256;
  while(s_turn_count&&(s_ledger_n+n>history_limit||s_turn_count>=64)){dbg_log("PHASE4_EVICT reason=%s old_tokens=%d new_tokens=%d hard=%d",s_ledger_n+n>history_limit?"capacity":"defensive_turn_bound",s_ledger_n,n,s_hard_job);evict_turn();}
  s_turn_offsets[s_turn_count++]=s_ledger_n;memcpy(s_ledger+s_ledger_n,ids,n*sizeof(int));s_ledger_n+=n;s_used_context=s_ledger_n;
  /* Explicit lists/structured answers may need several sentences. Their hard
   * boundary is eligible for bounded automatic continuation. */
  if(strstr(prompt,"list")||strstr(prompt,"List")||strstr(prompt,"steps")||strstr(prompt,"JSON")||strstr(prompt,"code"))s_soft_job=s_hard_job;
  s_cache_us=sceKernelGetProcessTimeWide()-cache;
  snprintf(s_path_name,sizeof(s_path_name),"neural");
  char exact[1024];uint64_t direct_at=sceKernelGetProcessTimeWide();int direct=s_policy_job==3?assistant_direct(prompt,&a,exact,sizeof(exact)):0;s_lookup_us+=sceKernelGetProcessTimeWide()-direct_at;
  if(direct==5&&s_multi_job)direct=0; /* Let supplied conversation facts be used. */
  if(direct){
   snprintf(s_path_name,sizeof(s_path_name),"%s",direct==1?"local_tool":direct==2?"local_reference":"epistemic_rule");
   int answer_ids[2048],an=encode_bounded(exact,answer_ids);
   if(s_ledger_n+an+2<=512){memcpy(s_ledger+s_ledger_n,answer_ids,an*sizeof(int));s_ledger_n+=an;}
   s_session.reason=END_LOCAL;s_session.count=0;s_session.total_count=0;s_session.has_pending=0;snprintf(s_session.text,sizeof(s_session.text),"%s",exact);publish(exact,0);commit_response();s_last_multi=s_multi_job;s_last_policy=s_policy_job;goto finish;
  }
  int reuse=s_multi_job&&s_reuse_enabled;
  if(s_reuse_enabled&&s_prefix_snapshot&&s_ledger_n>s_prefix_n&&!memcmp(s_ledger,s_prefix_ids,s_prefix_n*sizeof(int))){
   int append=reuse&&s_session.pos>0&&s_session.pos<s_ledger_n&&!memcmp(s_session.fed,s_ledger,s_session.pos*sizeof(int));
   if(!append){uint64_t copy=sceKernelGetProcessTimeWide();TIMED(30,P_COPY,engine_prefix_restore(&s_transformer,s_prefix_n,s_prefix_snapshot,s_prefix_bytes););memcpy(s_session.fed,s_prefix_ids,s_prefix_n*sizeof(int));s_session.pos=s_prefix_n;s_prefix_used=1;s_cache_us+=sceKernelGetProcessTimeWide()-copy;reuse=1;}
  }
  int rc=session_start(&s_session,s_ledger,s_ledger_n,s_soft_job,s_hard_job,reuse);
  if(rc==-4){s_turn_count=0;s_ledger_n=0;s_last_multi=0;publish("Stopped before an answer was generated.",0);goto finish;}
  if(rc<0){set_error("Session prefill failed");return;}
 }
 s_cache_reused=s_session.reused;s_cache_rebuilt=s_session.rebuilt;
 while(!s_cancel&&s_session.reason==END_NONE){if(session_step(&s_session)<0)break;sceKernelDelayThread(1000);}
 if(s_cancel)session_stop(&s_session);
 publish(s_session.text,0);commit_response();s_last_multi=s_multi_job;s_last_policy=s_policy_job;
finish:;
#ifdef __vita__
 dbg_log("PHASE4_HARDWARE cpu_MHz=%d bus_MHz=%d battery=%d battery_temp=%d prefix_tokens=%d prefix_bytes=%u prefix_used=%d",scePowerGetArmClockFrequency(),scePowerGetBusClockFrequency(),scePowerGetBatteryLifePercent(),scePowerGetBatteryTemp(),s_prefix_n,(unsigned)s_prefix_bytes,s_prefix_used);
#endif
 uint64_t end=sceKernelGetProcessTimeWide();s_ttft=s_first_us?(s_first_us-s_begin_us)/1e6:0;
 s_decode=s_session.count>1&&end>s_first_us?(s_session.count-1)*1e6/(end-s_first_us):0;
 dbg_log("PHASE4_RESULT stop=%s count=%d total_count=%d soft=%d hard=%d pos=%d prompt=%d reused=%d rebuilt=%d evicted=%d turns=%d mode=%d policy=%d ttft_us=%llu total_us=%llu decode_pieces_s=%.6f heap=%u lookup_us=%llu cache_us=%llu route=%s source=%s path=%s",session_reason(s_session.reason),s_session.count,s_session.total_count,s_soft_job,s_hard_job,s_session.pos,s_used_context,s_cache_reused,s_cache_rebuilt,s_evictions,s_turn_count,s_multi_job,s_policy_job,(unsigned long long)(s_first_us?s_first_us-s_begin_us:0),(unsigned long long)(end-s_begin_us),s_decode,(unsigned)mallinfo().uordblks,(unsigned long long)s_lookup_us,(unsigned long long)s_cache_us,s_route_name,s_source,s_path_name);
 dbg_log("PHASE4_OUTPUT %s",s_response);for(int i=0;i<s_session.count;i++)dbg_log("PHASE4_ID index=%d id=%d",i,s_session.generated[i]);prof_dump();
 /* Actual-input trace is private-bubble diagnostics, never production I/O. */
 if(app_test_path()){
  if(!strcmp(s_path_name,"neural"))for(int i=0;i<s_session.pos;i++)dbg_log("PHASE11_FED index=%d id=%d",i,s_session.fed[i]);
  for(int i=0;i<s_ledger_n;i++)dbg_log("PHASE11_LEDGER index=%d id=%d",i,s_ledger[i]);
 }
}
