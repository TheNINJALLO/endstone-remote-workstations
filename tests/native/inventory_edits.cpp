#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
namespace {
void check(bool v){if(!v)throw std::runtime_error("inventory edit assertion failed");}
std::vector<uint8_t> item(uint8_t count){return {10,0,0,1,5,0,'C','o','u','n','t',count,0};}
void api(){
 bool allowed=true,enabled=true,stale=false,revoke=false,mutate=false;int calls=0;
 auto before=item(1),after=item(2);const auto expected=before;
 Host host;host.consumer_allowed=[&](auto){return enabled;};host.permission=[&](auto,auto permission){return permission!="remoteworkstations.inventory.write"||allowed;};
 host.observe_inventory_item=[&](auto,auto){return [&]{require(!stale,VCF_STALE);};};
 host.apply_inventory_edit=[&](auto,std::span<const InventoryEdit> changes,auto authorize,auto validate){
  ++calls;validate();authorize();check(changes.size()==1&&changes[0].slot==19&&changes[0].expected==expected);
  if(mutate){before[11]=9;check(changes[0].expected==expected);}
  if(revoke)enabled=false;authorize();
 };
 Engine engine(host);attach_engine(&engine);vcf_api table{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(table),&table)==VCF_OK);
 sdk::Client client(table,"edit_consumer"),foreign(table,"foreign_consumer");
 auto change=sdk::descriptor<vcf_inventory_edit>();change.slot=19;change.expected={before.data(),static_cast<uint32_t>(before.size())};change.replacement={after.data(),static_cast<uint32_t>(after.size())};
 auto observe=[&]{return client.observe_inventory_item("player",19);};
 auto apply=[&](vcf_handle token){return table.apply_inventory_edit(client.owner(),token,&change,1);};
 auto token=observe();check(table.apply_inventory_edit(foreign.owner(),token,&change,1)==VCF_DENIED&&calls==0);client.validate_inventory_observation(token);
 change.slot=36;check(apply(token)==VCF_INVALID&&calls==0);change.slot=19;
 change.version=VCF_ABI_VERSION_1_4;check(apply(token)==VCF_VERSION);change.version=VCF_ABI_VERSION;
 vcf_status threaded=VCF_OK;std::thread other([&]{threaded=apply(token);});other.join();check(threaded==VCF_WRONG_THREAD&&calls==0);
 allowed=false;check(apply(token)==VCF_DENIED&&calls==0);check(table.validate_inventory_observation(client.owner(),token)==VCF_NOT_FOUND);allowed=true;
 token=observe();stale=true;check(apply(token)==VCF_STALE&&calls==0);stale=false;
 token=observe();mutate=true;check(apply(token)==VCF_OK&&calls==1);mutate=false;before=expected;change.expected.data=before.data();check(apply(token)==VCF_NOT_FOUND);
 token=observe();revoke=true;check(apply(token)==VCF_CLOSED&&calls==2);revoke=false;enabled=true;
 token=observe();std::array<vcf_inventory_edit,2> duplicate{change,change};check(table.apply_inventory_edit(client.owner(),token,duplicate.data(),2)==VCF_INVALID);client.release_inventory_observation(token);
 check(client.dispose()==VCF_OK&&foreign.dispose()==VCF_OK);engine.shutdown();attach_engine(nullptr);
}
void provider_lifetime(){
 std::shared_ptr<Engine> engine;std::weak_ptr<Engine> weak;bool survived=false;
 Host host;host.consumer_allowed=[](auto){return true;};host.permission=[](auto,auto){return true;};host.observe_inventory_item=[](auto,auto){return []{};};
 host.apply_inventory_edit=[&](auto,auto,auto authorize,auto){
  engine->shutdown();attach_engine(nullptr);engine.reset();survived=!weak.expired();authorize();
 };
 engine=std::make_shared<Engine>(host);weak=engine;attach_engine(engine.get());vcf_api table{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(table),&table)==VCF_OK);
 sdk::Client client(table,"lifetime_consumer");auto token=client.observe_inventory_item("player",19);auto before=item(1),after=item(2);
 auto edit=sdk::descriptor<vcf_inventory_edit>();edit.slot=19;edit.expected={before.data(),static_cast<uint32_t>(before.size())};edit.replacement={after.data(),static_cast<uint32_t>(after.size())};
 check(table.apply_inventory_edit(client.owner(),token,&edit,1)==VCF_CLOSED&&survived&&weak.expired());
}
}
int main(){try{api();provider_lifetime();std::cout<<"Inventory edit C ABI bounds, owner isolation, permission, copied input, one-shot admission and provider lifetime passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
