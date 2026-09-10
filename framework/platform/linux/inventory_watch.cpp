#include "inventory_watch.hpp"
#include "inventory_writer.hpp"
#include "native_items.hpp"
#include "native_nbt.hpp"
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
using Save=std::unique_ptr<::ListTag>(*)(const void*,const uint8_t&);
using SaveItem=void(*)(uintptr_t*,const void*,const uint8_t*);
thread_local uint32_t native_depth=0;
struct SaveProjection {
 uint8_t context=0;
 std::array<uintptr_t,36> sources{};
 std::array<std::unique_ptr<NativeItem>,36> items;
};
struct SaveBarrier {
 uintptr_t container=0;
 std::array<std::unique_ptr<::ListTag>,5> before;
 std::atomic<bool> interference{false};
 // Accessed only on the primary thread; foreign threads never consume a permit.
 int armed=-1,inside=-1;
};
struct Entry {uintptr_t container,manager;std::weak_ptr<inventory::Revision> revision;};
struct State {
 uintptr_t bds=0,runtime=0;
 std::thread::id thread=std::this_thread::get_id();
 mutable std::mutex mutex;std::map<std::string,Entry> entries;
 Changed changed{};Set set{};Process process{};Save save{};SaveItem save_item{};
 std::array<uintptr_t,5> targets{};std::array<Hash,5> patches{};
 std::shared_ptr<SaveBarrier> barrier;
 std::shared_ptr<SaveProjection> projection;
 bool installed=false,active=false;
 std::atomic<bool> off_thread{false};
 void primary()const{require(std::this_thread::get_id()==thread,VCF_WRONG_THREAD);}
 std::shared_ptr<SaveBarrier> saving(uintptr_t container)const{
  std::lock_guard lock(mutex);return barrier&&barrier->container==container?barrier:nullptr;
 }
 std::shared_ptr<inventory::Revision> find(uintptr_t pointer,bool manager=false)noexcept {
  std::lock_guard lock(mutex);
  for(const auto&[id,e]:entries)if((manager?e.manager:e.container)==pointer)return e.revision.lock();
  return {};
 }
 void verify(bool writing=false)const{
  primary();require(installed&&active,VCF_UNAVAILABLE);
  require(!off_thread.load(),VCF_QUARANTINED);require(!native_depth,VCF_REENTRANT);
  {std::lock_guard lock(mutex);require(!projection&&(writing||!barrier),VCF_REENTRANT);}Memory m;
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
void admit_list(const ::ListTag* tag){
 Memory m;const auto p=reinterpret_cast<uintptr_t>(tag);m.check(p,40);
 require(m.get<uintptr_t>(p)==state->bds+0xe7af5e0,VCF_UNAVAILABLE);
 const auto vt=state->bds+0xe7af5e0;
 require(m.get<uintptr_t>(vt,8)==state->bds+0xcc8f710&&m.get<uintptr_t>(vt,24)==state->bds+0xcc873c0
  &&m.get<uintptr_t>(vt,80)==state->bds+0xcc89680,VCF_UNAVAILABLE);
 const auto first=m.get<uintptr_t>(p,8),last=m.get<uintptr_t>(p,16),limit=m.get<uintptr_t>(p,24);
 require(first<=last&&last<=limit&&(last-first)%8==0&&(last-first)/8<=36,VCF_CAPACITY);
 if(first!=last)m.check(first,last-first);
}
std::vector<uint8_t> list_bytes(const ::ListTag& tag){
 admit_list(&tag);NbtOutput out;out.writeByte(10);out.writeString("");out.writeByte(9);out.writeString("Inventory");tag.write(out);out.writeByte(0);return out.finish();
}
std::unique_ptr<::ListTag> copy_list(const ::ListTag& tag){
 admit_list(&tag);auto copy=tag.copy();admit_list(reinterpret_cast<const ::ListTag*>(copy.get()));
 return std::unique_ptr<::ListTag>(static_cast<::ListTag*>(copy.release()));
}
std::unique_ptr<::ListTag> saved(const void* container,const uint8_t& context){
 const auto barrier=state->saving(reinterpret_cast<uintptr_t>(container));
 if(!barrier)return state->save(container,context);
 // Preserve the caller's actual SaveContext. Each admitted context has its own
 // immutable native baseline; Network serialization may omit bundle contents.
 require(context<barrier->before.size(),VCF_UNAVAILABLE);
 return copy_list(*barrier->before[context]);
}
void saved_item(uintptr_t* result,const void* source,const uint8_t* context){
 // Inventory::saveToTag still creates the native list and slot tags. Only its
 // item serializer receives a detached object. Native non-Disk contexts may
 // consume component state, so they must never prepare views from live items.
 std::shared_ptr<SaveProjection> projection;
 if(std::this_thread::get_id()==state->thread){std::lock_guard lock(state->mutex);projection=state->projection;}
 if(projection){
  for(size_t i=0;i<projection->sources.size();++i)if(projection->sources[i]==reinterpret_cast<uintptr_t>(source)){
   require(context&&*context==projection->context,VCF_UNAVAILABLE);
   state->save_item(result,reinterpret_cast<const void*>(projection->items[i]->address()),context);return;
  }
 }
 state->save_item(result,source,context);
}
void changed(void* container,int slot){
 auto barrier=state->saving(reinterpret_cast<uintptr_t>(container));
 if(barrier&&(std::this_thread::get_id()!=state->thread||barrier->inside<0||barrier->inside!=slot))barrier->interference.store(true);
 NativeMutation native(inventory_container(container));
 inventory::Mutation scope(state->find(reinterpret_cast<uintptr_t>(container)),inventory::Revision::Kind::slot,slot);
 state->changed(container,slot);
}
void set(void* container,int slot,const void* item,bool balance){
 auto barrier=state->saving(reinterpret_cast<uintptr_t>(container));
 if(barrier){
  if(std::this_thread::get_id()!=state->thread||barrier->armed!=slot||barrier->inside!=-1){barrier->interference.store(true);return;}
  barrier->armed=-1;barrier->inside=slot;
 }
 struct Finish{std::shared_ptr<SaveBarrier> value;~Finish(){if(value)value->inside=-1;}} finish{barrier};
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
 if(count){
  std::lock_guard lock(state->mutex);
  if(state->barrier){for(const auto&[id,e]:state->entries)if(e.manager==reinterpret_cast<uintptr_t>(manager)&&e.container==state->barrier->container){
   state->barrier->interference.store(true);return; // retain queued work for the next normal tick
  }}
 }
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
  native(0xbcd29f0,8934,"5c8e7279a00e10993d28469058fe2b3b62b3f4a1a6214dba6d9e44e5462c0962"),
  native(0xbcfdea0,653,"4b7d37869be48dc71f48e6e4862e91ff818395e3edec553762202e441d4f66c0"),
  native(0xbbb7810,2367,"c144aa9fa63f229e691912937cac78bd9cb505ecf0cec94c8e60d6efa925d781")};
 native(0xcc89680,14,"6dee5deae26ad1c42a05b33a5860ed300a48981823482724c481a9cf0290bcae");
 native(0xcc89690,694,"f96b61c77bb087c1c2b58171daa1d121af9a7fa4003b84e2aea5f4b22ca046c9");
 native(0xcc8f710,120,"7e95cc9feec617b52e2db9802f7b7afba578d7fff2a4f83bf0d8cbc6bb774ef9");
 native(0xcc873c0,7,"c0aaa941b4b78a3377f57e1761480054a12af422e6930a97444acbea8c2661b7");
 native(0xcc873d0,184,"5358309eb5b0a885e8633af1f8352cf2fb17d4babbf457f706652ae644a7515b");
 native(0xbcd2700,273,"ee3df4349e01d7b3dd76f6f4cfc10cdb0a00566bff5d28ba65cbf32739700659");
 native(0xbcd4ce0,319,"1bc0e782c43ea4a3117f734d14897be38682eb9047a53a4fcf5d13014f9421a8");
 native(0xbcd5960,279,"557cb5a98ade7fabd52309442d04a9e8c5d7a303cb0ac7a3fa56249102d604d9");
 loader(0x1c6500,327,"f786bbfc01ec48e2032ff996514a47e745e8a0533334de1316053c5cd478dd6a");
 auto create=reinterpret_cast<void*(*)()>(loader(8529696,126,"8fa58e8ed6b13f8659e449c72b5b2da27aad07c468516abc9ad56f7943b76f7e"));
 auto prepare=reinterpret_cast<int(*)(void*,void**,void*)>(loader(8530464,1098,"fb0f63162e992162332a8161e56e93dc55b648093b960c032b8b607855fad980"));
 auto install=reinterpret_cast<int(*)(void*,int)>(loader(8531568,262,"e04b0d75b8ff08b5b04b31082b1d9a9ae35eaa0fa0e005b949585b38c4b0508d"));
 auto handle=create();require(handle,VCF_UNAVAILABLE);
 std::array<void*,5> trampolines{};for(size_t i=0;i<5;++i)trampolines[i]=reinterpret_cast<void*>(state->targets[i]);
 const std::array<void*,5> detours{reinterpret_cast<void*>(&changed),reinterpret_cast<void*>(&set),reinterpret_cast<void*>(&process),reinterpret_cast<void*>(&saved),reinterpret_cast<void*>(&saved_item)};
 for(size_t i=0;i<5;++i)require(prepare(handle,&trampolines[i],detours[i])==0,VCF_UNAVAILABLE);
 state->changed=reinterpret_cast<Changed>(trampolines[0]);state->set=reinterpret_cast<Set>(trampolines[1]);state->process=reinterpret_cast<Process>(trampolines[2]);
 state->save=reinterpret_cast<Save>(trampolines[3]);
 state->save_item=reinterpret_cast<SaveItem>(trampolines[4]);
 require(install(handle,0)==0,VCF_UNAVAILABLE);
 for(size_t i=0;i<5;++i)state->patches[i]=hash({reinterpret_cast<const uint8_t*>(state->targets[i]),32});
 state->installed=true;state->active=true;
}
void verify_inventory_item_save_hook(){
 require(state,VCF_UNAVAILABLE);state->primary();require(state->installed&&state->active,VCF_UNAVAILABLE);
 Memory m;m.check(state->targets[4],32,true);require(hash({reinterpret_cast<const uint8_t*>(state->targets[4]),32})==state->patches[4],VCF_UNAVAILABLE);
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
void InventoryWatch::validate_writer(endstone::Player& player,uint64_t request_revision)const{
 require(revision_!=nullptr,VCF_CLOSED);state->verify(true);
 require(revision_->capture().request_revision==request_revision,VCF_STALE);
 require(state->identify(player)==std::pair{container_,manager_},VCF_STALE);
}
struct InventoryWriter::Impl {
 Resolve resolve;std::function<void()> initial_admission;NativeItems items;InventoryWatch watch;inventory::Revision::Stamp stamp;
 inventory::SavedInventory original;
 std::string identity;uintptr_t container=0;
 std::array<std::unique_ptr<NativeItem>,36> old_items,new_items;
 std::shared_ptr<SaveBarrier> barrier;bool used=false;
 const char* barrier_operation="opening save barrier";
 const char* write_operation="writing inventory slot";
 explicit Impl(Resolve resolver,std::function<void()> admission):resolve(std::move(resolver)),initial_admission(std::move(admission)){
  require(bool(resolve));auto* p=resolve();require(p,VCF_CLOSED);identity=p->getUniqueId().str();
  watch=watch_inventory(*p);stamp=watch.capture();items.verify();container=items.container(*p);
  original=read();watch.validate(player(),stamp);if(initial_admission)initial_admission();watch.validate(player(),stamp);
 }
 endstone::Player& player(){
  auto* p=resolve();require(p&&p->isValid()&&!p->isDead()&&p->getUniqueId().str()==identity,VCF_CLOSED);
  require(items.container(*p)==container,VCF_STALE);return *p;
 }
 inventory::SavedInventory read(){
  auto& p=player();(void)p;inventory::SavedInventory result;size_t total=0;
  for(uint32_t i=0;i<36;++i){result[i]=items.slot(container,i);total+=result[i].size();require(total<=128*1024,VCF_CAPACITY);}
  player();return result;
 }
 void guard(const std::function<void()>& authorize){
  auto check=[&]{if(barrier){watch.validate_writer(player(),stamp.request_revision);require(!barrier->interference.load(),VCF_CONFLICT);}
   else{watch.validate(player(),stamp);if(initial_admission)initial_admission();watch.validate(player(),stamp);}};
  check();authorize();check();
 }
 void begin(){
  watch.validate(player(),stamp);auto next=std::make_shared<SaveBarrier>();next->container=container;
  const uint8_t disk=0;auto disk_baseline=state->save(reinterpret_cast<void*>(container),disk);admit_list(disk_baseline.get());
  for(uint8_t context=0;context<next->before.size();++context){
   constexpr const char* operations[]={"capturing Disk save view","capturing Network save view","capturing Clone save view","capturing Move save view","capturing BlockActorToItemUserData save view"};
   barrier_operation=operations[context];
   auto projection=std::make_shared<SaveProjection>();projection->context=context;
   for(uint32_t slot=0;slot<36;++slot){projection->sources[slot]=items.slot_address(container,slot);projection->items[slot]=items.reconstruct(original[slot]);}
   watch.validate(player(),stamp);require(read()==original,VCF_STALE);
   {std::lock_guard lock(state->mutex);require(!state->projection,VCF_REENTRANT);state->projection=projection;}
   struct EndProjection{~EndProjection(){std::lock_guard lock(state->mutex);state->projection.reset();}} end_projection;
   auto tag=state->save(reinterpret_cast<void*>(container),context);
   {std::lock_guard lock(state->mutex);state->projection.reset();}
   admit_list(tag.get());
   auto clone=copy_list(*tag);require(list_bytes(*clone)==list_bytes(*tag),VCF_UNAVAILABLE);
   if(context==0)require(list_bytes(*tag)==list_bytes(*disk_baseline),VCF_UNAVAILABLE);
   next->before[context]=std::move(tag);
  }
  barrier_operation="validating mutation stamp after native saves";watch.validate(player(),stamp);
  barrier_operation="validating inventory bytes after native saves";require(read()==original,VCF_STALE);
  barrier_operation="validating SDK observation after native saves";if(initial_admission)initial_admission();watch.validate(player(),stamp);
  std::lock_guard lock(state->mutex);require(!state->barrier,VCF_REENTRANT);
  state->barrier=next;barrier=std::move(next);
 }
 void end(){
  state->primary();{std::lock_guard lock(state->mutex);require(barrier&&state->barrier==barrier,VCF_CONFLICT);state->barrier.reset();}
  barrier.reset();
 }
 void write(uint32_t slot,bool rollback){
  write_operation="writing inventory slot";
  watch.validate_writer(player(),stamp.request_revision);
  require(barrier&&slot<36&&barrier->inside==-1&&barrier->armed==-1,VCF_REENTRANT);
  const auto& item=rollback?old_items[slot]:new_items[slot];require(item!=nullptr);
  barrier->armed=static_cast<int>(slot);
  try{reinterpret_cast<Set>(state->targets[1])(reinterpret_cast<void*>(container),static_cast<int>(slot),reinterpret_cast<const void*>(item->address()),true);}
  catch(...){barrier->armed=-1;throw;}
  require(barrier->armed==-1,VCF_UNAVAILABLE);
  // Exercise the installed save entry while this batch is partially applied.
  // Every admitted context must still return its complete baseline. A broken
  // guard cannot be treated as a successful inventory publication.
  write_operation="validating guarded native save views";state->verify(true);
  for(uint8_t context=0;context<barrier->before.size();++context){
   auto view=reinterpret_cast<Save>(state->targets[3])(reinterpret_cast<void*>(container),context);
   require(list_bytes(*view)==list_bytes(*barrier->before[context]),VCF_CONFLICT);
  }
 }
};
InventoryWriter::InventoryWriter(Resolve resolve,std::function<void()> initial):impl_(std::make_unique<Impl>(std::move(resolve),std::move(initial))){}
InventoryWriter::~InventoryWriter()=default;
const inventory::SavedInventory& InventoryWriter::before()const{return impl_->original;}
void InventoryWriter::validate()const{require(!impl_->used,VCF_CLOSED);impl_->watch.validate(impl_->player(),impl_->stamp);require(impl_->read()==impl_->original,VCF_STALE);impl_->watch.validate(impl_->player(),impl_->stamp);}
inventory::CommitResult InventoryWriter::commit(const inventory::SavedInventory& after,std::function<void()> authorize,std::function<void(Boundary)> record){
 require(!impl_->used,VCF_CLOSED);impl_->used=true;require(bool(authorize)&&bool(record));auto& self=*impl_;
 inventory::CommitHost host;
 host.guard=[&]{self.guard(authorize);};host.read=[&]{return self.read();};
 host.prepare=[&]{size_t total=0;for(size_t i=0;i<36;++i){
  total+=self.original[i].size()+after[i].size();require(total<=256*1024,VCF_CAPACITY);
  if(self.original[i]!=after[i]){self.old_items[i]=self.items.reconstruct(self.original[i]);self.new_items[i]=self.items.reconstruct(after[i]);}
 }};
 host.begin=[&]{self.begin();};host.end=[&]{self.end();};host.write=[&](uint32_t slot,bool rollback){self.write(slot,rollback);};host.record=std::move(record);
 auto result=inventory::commit_inventory(self.original,after,host);
 if(std::string_view(result.operation)=="opening save barrier")result.operation=self.barrier_operation;
 if(std::string_view(result.operation)=="writing inventory slot")result.operation=self.write_operation;
 return result;
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
