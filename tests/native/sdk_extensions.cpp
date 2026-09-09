#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace oni::vcf;
namespace {
void check(bool v){if(!v)throw std::runtime_error("SDK extension assertion failed");}
vcf_status VCF_CALL action(void*,const vcf_event*){return VCF_OK;}
template<class F> void refused(vcf_status code,F f){try{f();throw std::runtime_error("expected refusal");}catch(const Error& e){check(e.status==code);}}
struct GuardState {Engine* engine;int calls=0;bool allow=true;vcf_handle revoke=0;};
vcf_status VCF_CALL guard(void* p,const vcf_guard_event* e){
 auto& s=*static_cast<GuardState*>(p);++s.calls;
 check(e->size==sizeof(*e)&&e->version==VCF_ABI_VERSION);
 check(e->request.mode==VCF_NATIVE_CONTEXT&&std::string_view(e->request.canonical_id.data,e->request.canonical_id.length)=="craft");
 if(s.revoke)s.engine->release_named("protector");
 return s.allow?VCF_OK:VCF_DENIED;
}
void exercise(){
 Host host;host.consumer_allowed=[](auto){return true;};host.permission=[](auto,auto){return true;};
 int opens=0;host.open=[&](auto&){++opens;return VCF_PENDING;};host.close=[](auto&){return VCF_PENDING;};
 Engine engine(host);attach_engine(&engine);vcf_api api{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);
 sdk::Client one(api,"one"),two(api,"two"),protector(api,"protector");
 one.action("private",action);one.action("export",action,nullptr,"explicit.permission",true);
 check(one.actions().size()==2&&two.actions().size()==1);
 auto detached=two.actions()[0];check(detached.name=="one:export"&&detached.permission=="explicit.permission");
 uint32_t count=0;uint64_t revision=0;check(api.action_count(two.owner(),&count,&revision)==VCF_OK&&count==1);
 check(api.unregister_action(one.owner(),sdk::view("export"))==VCF_OK);
 auto output=sdk::descriptor<vcf_action_info>();char buffer[256]{};uint32_t needed=0;
 check(api.action_info(two.owner(),0,revision,&output,buffer,sizeof(buffer),&needed)==VCF_STALE);
 check(detached.name=="one:export"&&two.actions().empty());
 GuardState state{&engine};auto id=protector.guard("area",guard,&state,"workbench");
 refused(VCF_DENIED,[&]{engine.remove_guard(one.owner(),id);});
 auto s=one.prepare("player","craft",VCF_NATIVE_CONTEXT);one.open(s);engine.tick();
 check(opens==1&&state.calls==1);state.allow=false;engine.opened(one.owner(),s,VCF_OK);
 check(state.calls==2&&one.info(s).state==VCF_CLOSING);engine.tick();engine.retired(one.owner(),s,VCF_CLOSED);
 check(one.info(s).result==VCF_DENIED);
 state.allow=true;state.revoke=1;protector.guard("second",guard,&state,"craft");
 s=one.prepare("player","craft",VCF_NATIVE_CONTEXT);one.open(s);engine.tick();
 check(state.calls==3); // The second pointer was revoked by the first callback.
 engine.tick();check(protector.dispose()==VCF_OK);
 // Consumer disposal after an automatic plugin-disable revocation has no more
 // callable pointers in the provider. Both remaining registered owners revoke.
 check(one.dispose()==VCF_OK&&two.dispose()==VCF_OK);
 engine.shutdown();attach_engine(nullptr);
}
struct SourceGuardState {int calls=0;bool allow=true;};
vcf_status VCF_CALL source_guard(void* context,const vcf_guard_event* event){
 auto& state=*static_cast<SourceGuardState*>(context);++state.calls;
 auto text=[](vcf_string value){return std::string_view(value.data,value.length);};
 const auto& request=event->request;
 check(request.mode==VCF_REAL_SOURCE&&text(request.canonical_id)=="furnace");
 check(text(request.dimension)=="overworld"&&text(request.source_permission)=="machine.owner");
 check(request.x==-17&&request.y==81&&request.z==5&&request.entity_id.length==0&&request.held_id.length==0);
 return state.allow?VCF_OK:VCF_DENIED;
}
void copied_source_guards(){
 Host host;host.consumer_allowed=[](auto){return true;};host.permission=[](auto,auto){return true;};
 int opens=0;host.open=[&](const auto&){++opens;return VCF_PENDING;};host.close=[](auto&){return VCF_PENDING;};
 Engine engine(host);attach_engine(&engine);vcf_api api{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);
 sdk::Client caller(api,"machine"),protector(api,"source_protection");SourceGuardState guard_state;
 protector.guard("source",source_guard,&guard_state,"furnace");
 std::string dimension="overworld",permission="machine.owner",kind="furnace";
 auto request=sdk::descriptor<vcf_session_desc>();request.player=sdk::view("player");request.canonical_id=sdk::view(kind);
 request.mode=VCF_REAL_SOURCE;request.dimension=sdk::view(dimension);request.source_permission=sdk::view(permission);
 request.x=-17;request.y=81;request.z=5;vcf_handle ticket=0;
 check(api.prepare(caller.owner(),&request,&ticket)==VCF_OK);
 // The caller may release or reuse all borrowed descriptor storage as soon
 // as prepare returns. Every asynchronous guard must still see the source.
 dimension.assign(200,'x');permission.clear();kind="smoker";request.x=999;
 caller.open(ticket);engine.tick();check(opens==1&&guard_state.calls==1);
 engine.opened(caller.owner(),ticket,VCF_OK);check(guard_state.calls==2&&caller.info(ticket).state==VCF_ACTIVE);
 guard_state.allow=false;refused(VCF_DENIED,[&]{engine.authorize(caller.owner(),ticket,VCF_GUARD_ACTIVE);});
 check(guard_state.calls==3);caller.close(ticket);engine.tick();engine.retired(caller.owner(),ticket,VCF_CLOSED);
 check(api.forget(caller.owner(),ticket)==VCF_OK);check(caller.dispose()==VCF_OK&&protector.dispose()==VCF_OK);
 engine.shutdown();attach_engine(nullptr);
}
struct PairedGuardState {Engine* engine;vcf_handle owner=0,ticket=0;int primary=0,partner=0;bool deny=false,revoke=false;};
vcf_status VCF_CALL paired_guard(void* context,const vcf_guard_event* event){
 auto& state=*static_cast<PairedGuardState*>(context);
 const auto& request=event->request;
 check(event->ticket==state.ticket&&event->requesting_consumer==state.owner);
 check(request.mode==VCF_REAL_SOURCE&&request.y==91&&request.z==7);
 check(std::string_view(request.dimension.data,request.dimension.length)=="overworld");
 check(std::string_view(request.canonical_id.data,request.canonical_id.length)=="doublechest");
 // A protection callback can inspect the original ticket while authorizing
 // its partner. Authorizing an additional source must not rewrite it.
 check(state.engine->session(state.owner,state.ticket).x==1);
 if(request.x==1){++state.primary;return VCF_OK;}
 check(request.x==2);++state.partner;
 if(state.revoke)state.engine->release_named("paired_machine");
 return state.deny?VCF_DENIED:VCF_OK;
}
void paired_source_guards(){
 Host host;host.consumer_allowed=[](auto){return true;};bool permission=true;
 host.permission=[&](auto,auto){return permission;};host.open=[](auto&){return VCF_PENDING;};host.close=[](auto&){return VCF_PENDING;};
 Engine engine(host);const auto owner=engine.consumer("paired_machine"),protector=engine.consumer("paired_protection");
 Session source;source.player="player";source.kind="doublechest";source.dimension="overworld";
 source.permission="paired.owner";source.x=1;source.y=91;source.z=7;
 const auto ticket=engine.prepare(owner,source);PairedGuardState state{&engine,owner,ticket};
 engine.add_guard(protector,"both_halves","doublechest",paired_guard,&state);
 engine.open(owner,ticket);engine.tick();check(state.primary==1&&state.partner==0);
 engine.authorize_block(owner,ticket,VCF_GUARD_READY,2,91,7);engine.opened(owner,ticket,VCF_OK);
 check(state.primary==2&&state.partner==1&&engine.session(owner,ticket).state==VCF_ACTIVE);
 state.deny=true;
 refused(VCF_DENIED,[&]{engine.authorize_block(owner,ticket,VCF_GUARD_ACTIVE,2,91,7);});
 check(state.partner==2&&engine.session(owner,ticket).x==1);
 permission=false;
 refused(VCF_DENIED,[&]{engine.authorize_block(owner,ticket,VCF_GUARD_ACTIVE,2,91,7);});
 check(state.partner==2);permission=true;
 refused(VCF_INVALID,[&]{engine.authorize_block(owner,ticket,0,2,91,7);});
 refused(VCF_INVALID,[&]{engine.authorize_block(owner,ticket,VCF_GUARD_ACTIVE,30000001,91,7);});
 for(auto mode:{VCF_NATIVE_CONTEXT,VCF_TRANSIENT}){
  source.mode=mode;auto wrong=engine.prepare(owner,source);
  refused(VCF_INVALID,[&]{engine.authorize_block(owner,wrong,VCF_GUARD_ACTIVE,2,91,7);});
 }
 source.mode=VCF_REAL_SOURCE;source.held_id="held";auto held=engine.prepare(owner,source);
 refused(VCF_INVALID,[&]{engine.authorize_block(owner,held,VCF_GUARD_ACTIVE,2,91,7);});
 source.held_id.clear();source.kind="enderchest";auto personal=engine.prepare(owner,source);
 refused(VCF_INVALID,[&]{engine.authorize_block(owner,personal,VCF_GUARD_ACTIVE,2,91,7);});
 state.deny=false;state.revoke=true;
 refused(VCF_CLOSED,[&]{engine.authorize_block(owner,ticket,VCF_GUARD_ACTIVE,2,91,7);});
 check(state.partner==3);engine.tick();engine.release(protector);engine.shutdown();
}
vcf_status VCF_CALL older_guard(void* calls,const vcf_guard_event* event){
 check(event->version==VCF_ABI_VERSION_1_1&&event->request.version==VCF_ABI_VERSION_1_1);
 ++*static_cast<int*>(calls);return VCF_OK;
}
void held_inspection_api(){
 Host host;bool allowed=true,enabled=true;int reads=0;
 host.consumer_allowed=[&](auto){return enabled;};host.permission=[&](auto,auto permission){return permission!="remoteworkstations.open.shulker"||allowed;};
 host.inspect_held=[&](auto player){
  check(player=="player");++reads;vcf_held_info result{};
  std::strcpy(result.canonical_id,"shulker");std::strcpy(result.identifier,"minecraft:purple_shulker_box");
  result.slot=6;result.amount=1;result.metadata_bytes=123;result.native_open_available=1;return result;
 };
 host.open=[](auto&){return VCF_PENDING;};Engine engine(host);attach_engine(&engine);
 vcf_api api{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);sdk::Client caller(api,"held_reader");
 auto info=caller.inspect_held("player");check(info.slot==6&&info.amount==1&&info.metadata_bytes==123&&!info.native_open_available&&reads==1);
 info=sdk::descriptor<vcf_held_info>();info.slot=77;auto before=info;allowed=false;
 check(api.inspect_held(caller.owner(),sdk::view("player"),&info)==VCF_DENIED&&std::memcmp(&before,&info,sizeof(info))==0);allowed=true;
 enabled=false;check(api.inspect_held(caller.owner(),sdk::view("player"),&info)==VCF_CLOSED);enabled=true;
 vcf_status status=VCF_OK;std::thread thread([&]{auto out=sdk::descriptor<vcf_held_info>();status=api.inspect_held(caller.owner(),sdk::view("player"),&out);});thread.join();check(status==VCF_WRONG_THREAD);
 auto owner=caller.owner();check(caller.dispose()==VCF_OK);check(api.inspect_held(owner,sdk::view("player"),&info)==VCF_NOT_FOUND);
 // A binary using the old 1.1 table still sees 1.1 protection descriptors.
 vcf_api old{};check(oni_vcf_get_api(VCF_ABI_VERSION_1_1,VCF_API_1_1_SIZE,&old)==VCF_OK);
 auto consumer=sdk::descriptor<vcf_consumer_desc>();consumer.version=VCF_ABI_VERSION_1_1;consumer.name=sdk::view("old_reader");
 vcf_handle old_owner=0;check(old.register_consumer(&consumer,&old_owner)==VCF_OK);
 int calls=0;auto guard=sdk::descriptor<vcf_guard_desc>();guard.version=VCF_ABI_VERSION_1_1;guard.name=sdk::view("old_guard");guard.callback=older_guard;guard.context=&calls;
 vcf_handle registration=0;check(old.register_guard(old_owner,&guard,&registration)==VCF_OK);
 Session source;source.player="player";source.kind="chest";auto ticket=engine.prepare(old_owner,source);engine.authorize(old_owner,ticket,VCF_GUARD_DISPATCH);check(calls==1);
 check(old.unregister_consumer(old_owner)==VCF_OK);engine.shutdown();attach_engine(nullptr);
}
}
int main(){try{exercise();copied_source_guards();paired_source_guards();held_inspection_api();std::cout<<"Detached actions, copied and paired sources, held inspection, compatible guards and callback revocation passed\n";}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
