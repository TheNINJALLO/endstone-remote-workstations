#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include <iostream>
#include <stdexcept>
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
}
int main(){try{exercise();std::cout<<"Detached actions, stale revisions, scoped guards and callback revocation passed\n";}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
