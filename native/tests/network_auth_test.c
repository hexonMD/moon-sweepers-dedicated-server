#include <assert.h>
#include "../server_network.c"
int MoonPassword_Read(char *out,size_t capacity){(void)out;(void)capacity;return -1;}
static int password_notices;
static int anonymous_logons,token_logons;
static void __cdecl mock_anonymous_logon(void *self){assert(self==(void *)1);anonymous_logons++;}
static void __cdecl mock_token_logon(void *self,const char *token){assert(self==(void *)1);assert(!strcmp(token,"test-only-gslt-value"));token_logons++;}
void MoonPassword_NotifyRejected(void){password_notices++;}

static int auth_calls,end_calls,send_calls,auth_return,license_return,receive_calls,release_calls,cancel_calls,last_control_type;
static int __cdecl mock_begin(void *self,const void *ticket,int bytes,uint64_t id){(void)self;(void)ticket;(void)id;assert(bytes==8);auth_calls++;return auth_return;}
static void __cdecl mock_end(void *self,uint64_t id){(void)self;(void)id;end_calls++;}
static int __cdecl mock_send(void *self,const NetIdentity *id,const void *data,uint32_t bytes,int flags,int channel){(void)self;(void)id;(void)flags;assert(channel==3);assert(bytes>=sizeof(ControlHeader));assert(((const ControlHeader *)data)->magic==CONTROL_MAGIC&&((const ControlHeader *)data)->version==CONTROL_VERSION);last_control_type=((const ControlHeader *)data)->type;send_calls++;return 1;}
static bool __cdecl mock_close(void *self,const NetIdentity *id){(void)self;(void)id;return true;}
static int __cdecl mock_license(void *self,uint64_t id,uint32_t app){(void)self;(void)id;assert(app==5335510);return license_return;}
static void __cdecl mock_cancel(void *self,uint32_t ticket){(void)self;assert(ticket==123);cancel_calls++;}
static int __cdecl mock_receive(void *self,int channel,NetMessage **out,int capacity){(void)self;assert(channel==3&&capacity==16);static ControlHeader h={CONTROL_MAGIC,CONTROL_VERSION,PING,0};static NetMessage m={0};m.data=&h;m.size=sizeof(h);m.peer=identity(1);for(int i=0;i<capacity;i++)out[i]=&m;receive_calls++;return capacity;}
static void __cdecl mock_release(NetMessage *message){assert(message);release_calls++;}
static const uint64_t user_a=(1ULL<<56)|(1ULL<<52)|(1ULL<<32)|42;
static const uint64_t user_b=(1ULL<<56)|(1ULL<<52)|(1ULL<<32)|43;
static const uint64_t host_id=(1ULL<<56)|(4ULL<<52)|123;
static void reset(void){
 memset(peers,0,sizeof(peers));memset(&game,0,sizeof(game));configured=true;token_configured=false;token_valid=true;anonymous_logons=token_logons=0;SecureZeroMemory(server_login_token,sizeof(server_login_token));mode=MODE_SERVER;server_id=self_id=host_id;
 backend_ready=true;world_ready=false;client_connected=false;auth_calls=end_calls=send_calls=auth_return=license_return=0;
 shutdown_requested=client_stopped=server_started=false;client_auth_started=client_last_heard=0;user_api=NULL;client_ticket=0;receive_calls=release_calls=cancel_calls=0;
 allowlist_set=allowlist_valid=true;allow_count=1;allowlist[0]=user_a;
 memset(server_password,0,sizeof(server_password));memcpy(server_password,"test-only-password",18);server_password_length=18;server_password_valid=true;
 server_api=messages_api=(void *)1;sdk.begin_auth=mock_begin;sdk.end_auth=mock_end;sdk.send=mock_send;sdk.close=mock_close;sdk.has_license=mock_license;
}
static AuthPayload request(void){AuthPayload a={0};a.bytes=8;a.password_length=(uint16_t)server_password_length;memcpy(a.password,server_password,server_password_length);memcpy(a.name,"Test player",12);memset(a.ticket,0x42,8);return a;}
static ControlHeader request_header(void){ControlHeader h={CONTROL_MAGIC,CONTROL_VERSION,AUTH_REQUEST,(uint32_t)offsetof(AuthPayload,ticket)+8};return h;}
static void validate(uint64_t id,int32_t result){uint8_t data[20]={0};memcpy(data,&id,8);memcpy(data+8,&result,4);CallbackMessage callback={0,143,data,20};handle_server_callback(&callback);}
static void reconfigure(void){token_valid=true;token_configured=false;SecureZeroMemory(server_login_token,sizeof(server_login_token));configured=false;allow_count=0;allowlist_valid=true;server_password_valid=false;server_password_length=0;SecureZeroMemory(server_password,sizeof(server_password));configure();}
int main(void){
 reset();sdk.logon_anonymous=mock_anonymous_logon;sdk.logon_token=mock_token_logon;
 log_on_server();assert(anonymous_logons==1&&token_logons==0&&!strcmp(MoonNet_LoginMode(),"anonymous"));
 token_configured=true;memcpy(server_login_token,"test-only-gslt-value",20);log_on_server();assert(anonymous_logons==1&&token_logons==1&&!strcmp(MoonNet_LoginMode(),"gslt"));
 {int32_t result=5;CallbackMessage failed={0,102,&result,4};handle_server_callback(&failed);assert(token_configured&&!backend_ready&&anonymous_logons==1&&token_logons==1);}
 assert(server_identity((1ULL<<56)|(3ULL<<52)|123));assert(server_identity(host_id));assert(!server_identity(user_a));assert(!server_identity((1ULL<<56)|(3ULL<<52)));assert(!server_identity((3ULL<<52)|123));
 reset();AuthPayload a=request();ControlHeader h=request_header();
 consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==1);assert(peer(user_a,false));assert(!member(user_a));
 {uint8_t short_data[19]={0};memcpy(short_data,&user_a,8);CallbackMessage short_callback={0,143,short_data,19};handle_server_callback(&short_callback);assert(!member(user_a));}
 validate(user_a,0);assert(member(user_a));assert(MoonNet_AuthenticatedPeers()==1);assert(send_calls==1);
 validate(user_a,2);assert(!member(user_a));assert(!peer(user_a,false));assert(end_calls==1);
 reset();consume_control(user_b,&h,(const uint8_t *)&a);assert(auth_calls==0&&!peer(user_b,false));
 reset();a.bytes=MAX_TICKET+1;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();a=request();h.bytes--;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();h=request_header();h.bytes=3;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();h=request_header();auth_return=1;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==1&&!member(user_a)&&!peer(user_a,false));
 reset();allowlist_valid=false;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();allow_count=0;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();a=request();a.password[0]^=1;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0&&!peer(user_a,false)&&last_control_type==PASSWORD_REJECTED);
 reset();a=request();a.password_length--;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0&&last_control_type==PASSWORD_REJECTED);
 reset();a=request();server_password_valid=false;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==0);
 reset();a=request();allowlist_set=false;consume_control(user_b,&h,(const uint8_t *)&a);assert(auth_calls==1&&!member(user_b));validate(user_b,0);assert(member(user_b));
 reset();consume_control(user_a,&h,(const uint8_t *)&a);license_return=1;validate(user_a,0);assert(!member(user_a)&&!peer(user_a,false));
 reset();mode=MODE_CLIENT;self_id=user_a;client_connected=false;
 RosterPayload r={0};r.server=host_id;r.count=2;r.entries[0].id=host_id;r.entries[1].id=user_a;
 memcpy(r.entries[0].name,"Server",7);memcpy(r.entries[1].name,"Player",7);
 ControlHeader roster={CONTROL_MAGIC,CONTROL_VERSION,ROSTER,12+2*sizeof(RosterEntry)};
 consume_control(host_id+1,&roster,(const uint8_t *)&r);assert(!client_connected);
 r.entries[1].id=user_b;consume_control(host_id,&roster,(const uint8_t *)&r);assert(!client_connected);
 r.entries[1].id=user_a;consume_control(host_id,&roster,(const uint8_t *)&r);assert(client_connected&&member(host_id)&&member(user_a)&&!member(user_b));
 reset();a=request();h=request_header();consume_control(user_a,&h,(const uint8_t *)&a);validate(user_a,0);assert(member(user_a));
 {CallbackMessage disconnected={0,103,NULL,0};handle_server_callback(&disconnected);assert(!backend_ready&&!world_ready&&!server_id&&!MoonNet_AuthenticatedPeers()&&!member(user_a));int before=auth_calls;consume_control(user_a,&h,(const uint8_t *)&a);assert(auth_calls==before);}
 reset();sdk.receive=mock_receive;sdk.release=mock_release;receive_control();assert(receive_calls==4&&release_calls==64);
 reset();mode=MODE_CLIENT;self_id=user_a;client_connected=backend_ready=true;user_api=(void *)1;client_ticket=123;sdk.cancel_ticket=mock_cancel;client_last_heard=1000;
 assert(!client_timed_out(45999)&&client_connected);assert(client_timed_out(46001));assert(MoonNet_IsStopped()&&!client_connected&&!backend_ready&&!client_ticket&&cancel_calls==1);
 reset();mode=MODE_CLIENT;client_auth_started=1000;assert(!client_timed_out(60999));assert(client_timed_out(61001));assert(MoonNet_IsStopped());
 reset();mode=MODE_CLIENT;password_notices=0;ControlHeader wrong={CONTROL_MAGIC,CONTROL_VERSION,PASSWORD_REJECTED,0};consume_control(host_id+1,&wrong,NULL);assert(!MoonNet_IsStopped()&&!password_notices);consume_control(host_id,&wrong,NULL);assert(MoonNet_IsStopped()&&password_notices==1&&!strcmp(MoonNet_Phase(),"client-wrong-password"));consume_control(host_id,&wrong,NULL);assert(password_notices==1);
 {size_t n;char value[130];memset(value,'A',128);value[128]=0;assert(password_size(value,&n)&&n==128);value[8]=0;assert(password_size(value,&n)&&n==8);value[7]=0;assert(!password_size(value,&n));memset(value,'A',129);value[129]=0;assert(!password_size(value,&n));value[8]=0;value[2]='\n';assert(!password_size(value,&n));}
 SetEnvironmentVariableA("MOON_STEAM_TOKEN",NULL);
 char ids[128];snprintf(ids,sizeof(ids),"%llu,bad",(unsigned long long)user_a);
 SetEnvironmentVariableA("MOON_CONNECT",NULL);SetEnvironmentVariableA("MOON_DEDICATED","1");SetEnvironmentVariableA("MOON_SERVER_PASSWORD","test-only-password");SetEnvironmentVariableA("MOON_ALLOWED_STEAM_IDS",ids);reconfigure();assert(allowlist_set&&!allowlist_valid&&!allowed(user_a));
 SetEnvironmentVariableA("MOON_ALLOWED_STEAM_IDS",NULL);reconfigure();assert(server_password_valid&&!allowlist_set&&allowed(user_b));
 SetEnvironmentVariableA("MOON_ALLOWED_STEAM_IDS","");reconfigure();assert(server_password_valid&&!allowlist_set&&allowed(user_b));
 SetEnvironmentVariableA("MOON_ALLOWED_STEAM_IDS"," ");reconfigure();assert(!allowlist_valid&&!allowed(user_b));
 SetEnvironmentVariableA("MOON_ALLOWED_STEAM_IDS",NULL);SetEnvironmentVariableA("MOON_SERVER_PASSWORD",NULL);reconfigure();assert(!server_password_valid&&!allowed(user_b));assert(!MoonNet_Install(NULL,NULL));
 SetEnvironmentVariableA("MOON_STEAM_TOKEN","test-only-gslt-value");reconfigure();assert(token_configured&&token_valid&&!strcmp(MoonNet_LoginMode(),"gslt"));
 SetEnvironmentVariableA("MOON_STEAM_TOKEN","");reconfigure();assert(!token_configured&&token_valid&&!strcmp(MoonNet_LoginMode(),"anonymous"));
 SetEnvironmentVariableA("MOON_STEAM_TOKEN","bad token");reconfigure();assert(token_configured&&!token_valid&&!MoonNet_Install(NULL,NULL));
 {char token[MAX_LOGIN_TOKEN+2];memset(token,'A',sizeof(token)-1);token[sizeof(token)-1]=0;SetEnvironmentVariableA("MOON_STEAM_TOKEN",token);reconfigure();assert(token_configured&&!token_valid);token[MAX_LOGIN_TOKEN]=0;SetEnvironmentVariableA("MOON_STEAM_TOKEN",token);reconfigure();assert(token_configured&&token_valid);}
 SetEnvironmentVariableA("MOON_STEAM_TOKEN",NULL);
 printf("PASS: optional GSLT login, no fallback after rejection, both server account types, token validation; protocol-v2 password gate and wrong-password feedback; optional/malformed allowlists; Steam auth/license; packed20-byte callback; disconnect cleanup; bounded receive; client timeouts.\n");
 return 0;
}
