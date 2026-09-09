#include "inventory_watch.hpp"
#include "../runtime.hpp"
#include <endstone/player.h>
#include <openssl/evp.h>
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <typeinfo>
#include <atomic>

namespace oni::vcf::platform::linux_native {
namespace {
using Hash=std::array<uint8_t,32>;
Hash hash(std::span<const uint8_t> bytes){
 Hash value{};unsigned size=0;
 require(EVP_Digest(bytes.data(),bytes.size(),value.data(),&size,EVP_sha256(),nullptr)==1&&size==32,VCF_INTERNAL);return value;
}
struct Memory {
 struct Region{uintptr_t first,last;bool executable;};std::vector<Region> regions;
 Memory(){
  std::ifstream input("/proc/self/maps");require(bool(input),VCF_UNAVAILABLE);
  for(std::string line;std::getline(input,line);){unsigned long a,b;char mode[5]{};
   if(std::sscanf(line.c_str(),"%lx-%lx %4s",&a,&b,mode)==3&&mode[0]=='r'){
    require(regions.size()<16384,VCF_CAPACITY);regions.push_back({a,b,mode[2]=='x'});
   }
  }
 }
 void check(uintptr_t p,size_t n,bool executable=false)const{
  if(p&&n<=UINTPTR_MAX-p)for(const auto&r:regions)if(p>=r.first&&p+n<=r.last&&(!executable||r.executable))return;
  throw Error{VCF_UNAVAILABLE};
 }
 template<class T>T get(uintptr_t p,size_t offset=0)const{
  require(offset<=UINTPTR_MAX-p,VCF_UNAVAILABLE);check(p+offset,sizeof(T));T v;std::memcpy(&v,reinterpret_cast<void*>(p+offset),sizeof(v));return v;
 }
 void function(uintptr_t p,size_t n,std::string_view expected)const{
  check(p,n,true);auto value=hash({reinterpret_cast<const uint8_t*>(p),n});
  constexpr char digits[]="0123456789abcdef";std::array<char,64> actual{};
  for(size_t i=0;i<32;++i){actual[i*2]=digits[value[i]>>4];actual[i*2+1]=digits[value[i]&15];}
  require(std::string_view(actual.data(),actual.size())==expected,VCF_UNAVAILABLE);
 }
};
using Changed=void(*)(void*,int);
using Set=void(*)(void*,int,const void*,bool);
using Process=void(*)(void*);
thread_local uint32_t native_depth=0;
struct Entry {uintptr_t container,manager;std::weak_ptr<inventory::Revision> revision;};
struct State {
 uintptr_t bds=0,runtime=0;
 std::thread::id thread=std::this_thread::get_id();
 std::mutex mutex;std::map<std::string,Entry> entries;
 Changed changed{};Set set{};Process process{};
 std::array<uintptr_t,3> targets{};std::array<Hash,3> patches{};
 bool installed=false,active=false;
 std::atomic<bool> off_thread{false};
 void primary()const{require(std::this_thread::get_id()==thread,VCF_WRONG_THREAD);}
 std::shared_ptr<inventory::Revision> find(uintptr_t pointer,bool manager=false)noexcept {
  std::lock_guard lock(mutex);
  for(const auto&[id,e]:entries)if((manager?e.manager:e.container)==pointer)return e.revision.lock();
  return {};
 }
 void verify()const{
  primary();require(installed&&active,VCF_UNAVAILABLE);
  require(!off_thread.load(),VCF_QUARANTINED);require(!native_depth,VCF_REENTRANT);Memory m;
  for(size_t i=0;i<targets.size();++i){m.check(targets[i],32,true);require(hash({reinterpret_cast<const uint8_t*>(targets[i]),32})==patches[i],VCF_UNAVAILABLE);}
 }
 std::pair<uintptr_t,uintptr_t> identify(endstone::Player& player)const{
  require(player.isValid()&&!player.isDead(),VCF_CLOSED);
  auto& inv=player.getInventory();require(inv.getSize()==36&&std::string_view(typeid(inv).name())=="N8endstone4core23EndstonePlayerInventoryE",VCF_UNAVAILABLE);
  Memory m;auto wrapper=reinterpret_cast<uintptr_t>(&inv);require(wrapper>=8,VCF_UNAVAILABLE);
  auto vt=m.get<uintptr_t>(wrapper);require(vt>=16&&m.get<intptr_t>(vt-16)==-16,VCF_UNAVAILABLE);
  auto container=m.get<uintptr_t>(wrapper-8),native_player=m.get<uintptr_t>(wrapper,8);m.check(container,352);
  require(m.get<uintptr_t>(container)==bds+0xe64d2e0&&m.get<uintptr_t>(container,344)==native_player,VCF_UNAVAILABLE);
  // Independently compiled pinned Player layout; do not include its unrelated
  // global RakNet initializers in the loadable provider. See research evidence.
  m.check(native_player,2568);auto manager=m.get<uintptr_t>(native_player,2560);m.check(manager,152);
  require(m.get<uintptr_t>(manager)==bds+0xe6c65f8,VCF_UNAVAILABLE);return {container,manager};
 }
};
// Endstone's bundled funchook owns the trampolines. Both this state and the
// provider code live for the process; disabling only retires watched objects.
State* state=nullptr;
struct NativeMutation {
 bool active;
 explicit NativeMutation(bool value):active(value){
  if(!active)return;
  if(std::this_thread::get_id()!=state->thread)state->off_thread.store(true);
  ++native_depth;
 }
 ~NativeMutation(){if(active)--native_depth;}
};
bool inventory_container(const void* p){uintptr_t vt=0;std::memcpy(&vt,p,8);return vt==state->bds+0xe64d2e0;}
void changed(void* container,int slot){
 NativeMutation native(inventory_container(container));
 inventory::Mutation scope(state->find(reinterpret_cast<uintptr_t>(container)),inventory::Revision::Kind::slot,slot);
 state->changed(container,slot);
}
void set(void* container,int slot,const void* item,bool balance){
 NativeMutation native(inventory_container(container));
 inventory::Mutation scope(state->find(reinterpret_cast<uintptr_t>(container)),inventory::Revision::Kind::slot,slot);
 state->set(container,slot,item,balance);
}
void process(void* manager){
 auto watch=state->find(reinterpret_cast<uintptr_t>(manager),true);
 uintptr_t vt=0;std::memcpy(&vt,manager,8);uint64_t count=0;
 if(vt==state->bds+0xe6c65f8)std::memcpy(&count,static_cast<const uint8_t*>(manager)+144,8);
 else if(watch){watch->poison();watch.reset();}
 if(!count)watch.reset();
 // Cover requests that began before a watch existed. A first read from a
 // reentrant native callback must not acquire an apparently idle lease.
 NativeMutation native(count!=0);
 // Nonempty native request batches invalidate held edits even when a component
 // mutates in place and BDS sends no setter/change notification (bundle extract).
 inventory::Mutation scope(std::move(watch),inventory::Revision::Kind::request);
 state->process(manager);
}
}
void initialize_inventory_watches(){
 if(state){state->primary();require(state->installed,VCF_UNAVAILABLE);state->active=true;state->verify();return;}
 require(inspect_runtime().accepted&&pin_provider(),VCF_UNAVAILABLE);
 state=new State;
 dl_iterate_phdr([](dl_phdr_info*i,size_t,void*p){if(!i->dlpi_name||!*i->dlpi_name){*static_cast<uintptr_t*>(p)=i->dlpi_addr;return 1;}return 0;},&state->bds);
 Dl_info runtime{};auto* entry=dlsym(RTLD_DEFAULT,"endstone_get_server");require(entry&&dladdr(entry,&runtime),VCF_UNAVAILABLE);
 state->runtime=reinterpret_cast<uintptr_t>(runtime.dli_fbase);require(state->bds&&state->runtime,VCF_UNAVAILABLE);
 Memory memory;
 auto native=[&](uintptr_t rva,size_t size,std::string_view digest){auto p=state->bds+rva;memory.function(p,size,digest);return p;};
 auto loader=[&](uintptr_t rva,size_t size,std::string_view digest){auto p=state->runtime+rva;memory.function(p,size,digest);return p;};
 state->targets={
  native(0xade2e00,66,"005182631bf9abfb28760293e9d6518101808396a229074ebaf0df1d0f5262e6"),
  native(0xb3cb4e0,1823,"5741cfc4ffbd1e89cb8d52af7c229e9ce775ad841cf8d1e674b467a09663df2a"),
  native(0xbcd29f0,8934,"5c8e7279a00e10993d28469058fe2b3b62b3f4a1a6214dba6d9e44e5462c0962")};
 native(0xbcd2700,273,"ee3df4349e01d7b3dd76f6f4cfc10cdb0a00566bff5d28ba65cbf32739700659");
 native(0xbcd4ce0,319,"1bc0e782c43ea4a3117f734d14897be38682eb9047a53a4fcf5d13014f9421a8");
 native(0xbcd5960,279,"557cb5a98ade7fabd52309442d04a9e8c5d7a303cb0ac7a3fa56249102d604d9");
 loader(0x1c6500,327,"f786bbfc01ec48e2032ff996514a47e745e8a0533334de1316053c5cd478dd6a");
 auto create=reinterpret_cast<void*(*)()>(loader(8529696,126,"8fa58e8ed6b13f8659e449c72b5b2da27aad07c468516abc9ad56f7943b76f7e"));
 auto prepare=reinterpret_cast<int(*)(void*,void**,void*)>(loader(8530464,1098,"fb0f63162e992162332a8161e56e93dc55b648093b960c032b8b607855fad980"));
 auto install=reinterpret_cast<int(*)(void*,int)>(loader(8531568,262,"e04b0d75b8ff08b5b04b31082b1d9a9ae35eaa0fa0e005b949585b38c4b0508d"));
 auto handle=create();require(handle,VCF_UNAVAILABLE);
 std::array<void*,3> trampolines{};for(size_t i=0;i<3;++i)trampolines[i]=reinterpret_cast<void*>(state->targets[i]);
 const std::array<void*,3> detours{reinterpret_cast<void*>(&changed),reinterpret_cast<void*>(&set),reinterpret_cast<void*>(&process)};
 for(size_t i=0;i<3;++i)require(prepare(handle,&trampolines[i],detours[i])==0,VCF_UNAVAILABLE);
 state->changed=reinterpret_cast<Changed>(trampolines[0]);state->set=reinterpret_cast<Set>(trampolines[1]);state->process=reinterpret_cast<Process>(trampolines[2]);
 require(install(handle,0)==0,VCF_UNAVAILABLE);
 for(size_t i=0;i<3;++i)state->patches[i]=hash({reinterpret_cast<const uint8_t*>(state->targets[i]),32});
 state->installed=true;state->active=true;
}
InventoryWatch watch_inventory(endstone::Player& player){
 require(state,VCF_UNAVAILABLE);state->verify();auto [container,manager]=state->identify(player);
 std::lock_guard lock(state->mutex);const auto id=player.getUniqueId().str();
 for(auto it=state->entries.begin();it!=state->entries.end();)if(it->second.revision.expired())it=state->entries.erase(it);else ++it;
 auto it=state->entries.find(id);std::shared_ptr<inventory::Revision> revision;
 if(it!=state->entries.end()){
  if(it->second.container!=container||it->second.manager!=manager){if(auto old=it->second.revision.lock())old->retire();state->entries.erase(it);throw Error{VCF_STALE};}
  revision=it->second.revision.lock();
 }
 if(!revision){
  require(state->entries.size()<256,VCF_CAPACITY);
  // Two identities may never attach separate clocks to the same native owner.
  for(const auto&[key,e]:state->entries)require(e.container!=container&&e.manager!=manager,VCF_CONFLICT);
  revision=std::make_shared<inventory::Revision>(state->thread);state->entries.emplace(id,Entry{container,manager,revision});
 }
 InventoryWatch result;result.revision_=std::move(revision);result.container_=container;result.manager_=manager;return result;
}
inventory::Revision::Stamp InventoryWatch::capture()const{require(revision_!=nullptr,VCF_CLOSED);state->verify();return revision_->capture();}
void InventoryWatch::validate(endstone::Player& player,const inventory::Revision::Stamp& expected)const{
 require(revision_!=nullptr,VCF_CLOSED);state->verify();revision_->validate(expected);
 require(state->identify(player)==std::pair{container_,manager_},VCF_STALE);revision_->validate(expected);
}
void retire_inventory_watches(std::string_view player){
 if(!state)return;state->primary();std::lock_guard lock(state->mutex);auto it=state->entries.find(std::string(player));
 if(it!=state->entries.end()){if(auto revision=it->second.revision.lock())revision->retire();state->entries.erase(it);}
}
void shutdown_inventory_watches(){
 if(!state)return;state->primary();std::lock_guard lock(state->mutex);
 for(const auto&[id,e]:state->entries)if(auto revision=e.revision.lock())revision->retire();state->entries.clear();state->active=false;
}
}
