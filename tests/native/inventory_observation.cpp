#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include "inventory_revision.hpp"
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
namespace {
void check(bool yes){if(!yes)throw std::runtime_error("SDK inventory observation assertion failed");}
void contract(){
 bool permission=true,enabled=true,revoke_factory=false,revoke_validator=false,native_change=false,reentrant=false;
 std::shared_ptr<inventory::Revision> revision=std::make_shared<inventory::Revision>();
 vcf_api api{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);vcf_handle consumer=0,token=0;
 int active=0,calls=0;struct Lifetime {int& active;explicit Lifetime(int& n):active(n){++active;}~Lifetime(){--active;}};
 Host host;host.consumer_allowed=[&](auto){return enabled;};host.permission=[&](auto,auto){return permission;};
 host.observe_inventory_item=[&](auto,uint32_t slot)->std::function<void()>{
  if(slot==35)throw Error{VCF_NOT_FOUND};
  if(revoke_factory)enabled=false;
  auto life=std::make_shared<Lifetime>(active);auto stamp=revision->capture();bool before=native_change;
  return [&,life,stamp,before]{
   ++calls;if(reentrant)check(api.validate_inventory_observation(consumer,token)==VCF_REENTRANT);
   revision->validate(stamp);require(before==native_change,VCF_STALE);if(revoke_validator)permission=false;
  };
 };
 Engine engine(host);attach_engine(&engine);sdk::Client client(api,"observer"),other(api,"other");consumer=client.owner();
 const auto player=sdk::view("player");vcf_handle output=987;
 auto observe=[&](uint32_t slot=19){return api.observe_inventory_item(consumer,player,slot,&output);};
 permission=false;check(observe()==VCF_DENIED&&output==987&&active==0);permission=true;
 check(observe(36)==VCF_INVALID&&output==987&&active==0);check(observe(35)==VCF_NOT_FOUND&&output==987&&active==0);
 revoke_factory=true;check(observe()==VCF_CLOSED&&output==987&&active==0);revoke_factory=false;enabled=true;
 check(observe()==VCF_OK&&active==1);token=output;
 check(api.validate_inventory_observation(other.owner(),token)==VCF_DENIED);
 check(api.release_inventory_observation(other.owner(),token)==VCF_DENIED&&active==1);
 client.validate_inventory_observation(token);reentrant=true;client.validate_inventory_observation(token);reentrant=false;
 vcf_status worker=VCF_OK;std::thread wrong([&]{worker=api.validate_inventory_observation(consumer,token);});wrong.join();check(worker==VCF_WRONG_THREAD);
 client.validate_inventory_observation(token);
 {inventory::Mutation mutation(revision,inventory::Revision::Kind::request);}
 check(api.validate_inventory_observation(consumer,token)==VCF_STALE);auto stopped_calls=calls;
 check(api.validate_inventory_observation(consumer,token)==VCF_STALE&&calls==stopped_calls);
 client.release_inventory_observation(token);check(active==0&&api.validate_inventory_observation(consumer,token)==VCF_NOT_FOUND);
 token=client.observe_inventory_item("player",19);native_change=true;
 check(api.validate_inventory_observation(consumer,token)==VCF_STALE);native_change=false;
 check(api.validate_inventory_observation(consumer,token)==VCF_STALE);client.release_inventory_observation(token);
 token=client.observe_inventory_item("player",19);revoke_validator=true;
 check(api.validate_inventory_observation(consumer,token)==VCF_DENIED);permission=true;revoke_validator=false;
 check(api.validate_inventory_observation(consumer,token)==VCF_DENIED);client.release_inventory_observation(token);
 token=client.observe_inventory_item("player",19);engine.player_gone("player");check(active==0&&api.validate_inventory_observation(consumer,token)==VCF_NOT_FOUND);
 token=client.observe_inventory_item("player",19);engine.release_named("observer");check(active==0);
 check(api.validate_inventory_observation(consumer,token)==VCF_NOT_FOUND);check(client.dispose()==VCF_OK);
 check(other.dispose()==VCF_OK);engine.shutdown();attach_engine(nullptr);
}
void limits(){
 int live=0;struct Count{int& n;explicit Count(int& v):n(v){++n;}~Count(){--n;}};
 Host host;host.consumer_allowed=[](auto){return true;};host.permission=[](auto,auto){return true;};
 host.observe_inventory_item=[&](auto,auto)->std::function<void()>{auto owned=std::make_shared<Count>(live);return [owned]{};};
 Engine engine(host);attach_engine(&engine);vcf_api api{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);
 std::vector<std::unique_ptr<sdk::Client>> clients;
 for(int i=0;i<4;++i){
  clients.push_back(std::make_unique<sdk::Client>(api,"observer"+std::to_string(i)));
  for(int j=0;j<64;++j)clients.back()->observe_inventory_item("player",19);
  vcf_handle output=99;check(api.observe_inventory_item(clients.back()->owner(),sdk::view("player"),19,&output)==VCF_CAPACITY&&output==99);
 }
 check(live==256);sdk::Client fifth(api,"fifth");vcf_handle output=99;
 check(api.observe_inventory_item(fifth.owner(),sdk::view("player"),19,&output)==VCF_CAPACITY&&output==99&&live==256);
 check(clients.front()->dispose()==VCF_OK&&live==192);fifth.observe_inventory_item("player",19);check(live==193);
 engine.shutdown();check(live==0);attach_engine(nullptr);
 Host absent;absent.consumer_allowed=[](auto){return true;};absent.permission=[](auto,auto){return true;};
 Engine unavailable(absent);attach_engine(&unavailable);sdk::Client no_adapter(api,"missing");output=99;
 check(api.observe_inventory_item(no_adapter.owner(),sdk::view("player"),19,&output)==VCF_UNAVAILABLE&&output==99);
 check(no_adapter.dispose()==VCF_OK);unavailable.shutdown();attach_engine(nullptr);
}
}
int main(){try{contract();limits();std::cout<<"SDK observations: ownership, limits, ABA, permissions, reentrancy, lifetime and unavailable platforms passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
