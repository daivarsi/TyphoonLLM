/* One executable for test and production. Unknown identities fail closed. */
#include <psp2/appmgr.h>
#include <string.h>
#include "app_paths.h"
static int role;
int app_paths_init(void){
 char title[32]={0};
 if(sceAppMgrAppParamGetString(0,SCE_APPMGR_APP_PARAM_TITLE_ID,title,sizeof(title))<0)return -1;
 if(!strcmp(title,"VLMTEST13"))role=1;
 else if(!strcmp(title,"VLMCHAT01"))role=2;
 else return -1;
 return 0;
}
const char*app_data_dir(void){return role==2?"ux0:data/VitaLMChat":"ux0:data/TyphoonTest13";}
const char*app_log_path(void){return role==2?"ux0:data/VitaLMChat/chat.log":"ux0:data/TyphoonTest13/chat.log";}
/* Production never reads or renames an automatic test trigger. */
const char*app_test_path(void){return role==1?"ux0:data/TyphoonTest13/phase4-tests.txt":0;}
const char*app_test_done(void){return role==1?"ux0:data/TyphoonTest13/phase4-tests.done":0;}
