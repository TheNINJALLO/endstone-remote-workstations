#include "native_ui.hpp"
#include <oni/vcf/window_leases.hpp>
#include <endstone/server.h>
#include <endstone/player.h>
#include <endstone/block/block_data.h>
#include <endstone/level/dimension.h>
#include <endstone/event/server/packet_receive_event.h>
#include <endstone/event/server/packet_send_event.h>
#include <openssl/evp.h>
#include <dlfcn.h>
#include <link.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <typeinfo>
#include <type_traits>

namespace oni::vcf::platform::linux_native {
namespace {
using Clock=std::chrono::steady_clock;
uint64_t milliseconds(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());}
uint8_t next_window(uint8_t value){return value<99?value+1:1;}
struct Point {int32_t x=0,y=0,z=0;bool operator==(const Point&)const=default;};
static_assert(sizeof(Point)==12&&alignof(Point)==4);
// Independently compiled against the pinned Linux headers: the trivial
// variant<BlockPos, ActorUniqueID> occupies 24 bytes and is passed by value
// on the System V stack. It is not the Windows pointer argument.
struct alignas(8) CraftOwner {Point position;uint32_t padding=0,index=0,tail=0;};
struct alignas(8) CraftContext {uintptr_t player;uint8_t type=1;std::array<uint8_t,7> padding{};Point position;uint32_t padding2=0,index=2,tail=0;};
static_assert(sizeof(CraftOwner)==24&&alignof(CraftOwner)==8&&offsetof(CraftOwner,index)==16&&std::is_trivially_copyable_v<CraftOwner>);
static_assert(sizeof(CraftContext)==40&&alignof(CraftContext)==8&&offsetof(CraftContext,type)==8&&offsetof(CraftContext,position)==16&&offsetof(CraftContext,index)==32);
std::string container_open(uint8_t window,uint8_t type,Point position);
struct Spec {std::string_view id,block;uint8_t type;uintptr_t factory;std::string_view hash;size_t factory_size=1502;};
// Linux ELF RTTI, complete FDE extents and the independently compiled
// PlayerOpenContainerEvent layout establish these System V caller arguments.
constexpr Spec specs[]={
 {"craft","minecraft:crafting_table",1,0x98ed100,"d69475a5f07ecae44aa9126d1b68cb41c0cd39d93a5d9fff750b357bc45b331e"},
 {"anvil","minecraft:anvil",5,0x44ff720,"697424f122bd3a97de419c970e16f6e85e99cc8125656f3723c0af6e3b6c98ad"},
 {"smithing","minecraft:smithing_table",33,0x44ff140,"d835340c6cb1bf828b393071aa1ca7aead64cc51642a94eb7c32b738a45a35b9"},
 {"cartography","minecraft:cartography_table",30,0x4500e20,"7adcc8b5ba6279c4dd57e84d2c7fdc1e4dbbd0f7ff567893b71db608db233853"},
 {"grindstone","minecraft:grindstone",26,0x4504870,"2290a6eb96441a2430a453a892f08991deff670a1a207786226e6fb7a2053886"},
 {"loom","minecraft:loom",24,0x45057d0,"c1a48f261804a4284d852e66d3b0c49bc09c435089acc52c41a4984c147ff21c"},
 {"stonecutter","minecraft:stonecutter_block",29,0x4506900,"50a4d7c092c689e223a84db196a999bd8e98194938b89bb64d9137fd61fda4df"},
 {"enchanting","minecraft:enchanting_table",3,0x4503dc0,"943bc11ba415fe3029ec484b2992911b1c98923590d09968a9c7b072ea0cc093",1345},
 {"inventory2x2","",255,0,{}},{"armor","",255,0,{}},{"offhand","",255,0,{}},{"recipebook","",255,0,{}}
};
const Spec* spec(std::string_view id){for(const auto& row:specs)if(row.id==id)return &row;return nullptr;}
// Independently derived with Linux Clang 20.1.8 / pinned Endstone headers,
// then matched against the live wrapper, native vtable and ELF unwind ranges.
constexpr uintptr_t inventory_open_rva=0x98ecc40,held_getter_rva=0x1d3780;
constexpr std::string_view inventory_open_hash="e00d775afbd065bd6355c5ae7242b785cd17865dc969ae991febe343505b3293";
constexpr std::string_view held_getter_hash="12c2537a6b6524441b83a4ac35c961709bf5fe8e2e3c8b38a0c1bf38ef286888";
struct Memory {
 struct Region{uintptr_t first,last;bool read,execute;};std::vector<Region> regions;
 Memory(){
  std::ifstream input("/proc/self/maps");require(bool(input),VCF_UNAVAILABLE);
  for(std::string line;std::getline(input,line);){
   unsigned long first,last;char permissions[5]{};
   if(std::sscanf(line.c_str(),"%lx-%lx %4s",&first,&last,permissions)==3){
    require(regions.size()<16384,VCF_CAPACITY);regions.push_back({first,last,permissions[0]=='r',permissions[2]=='x'});
   }
  }
 }
 void readable(uintptr_t p,size_t bytes,bool executable=false)const{
  if(p&&bytes<=UINTPTR_MAX-p)for(const auto&r:regions)
   if(r.read&&(!executable||r.execute)&&p>=r.first&&p+bytes<=r.last)return;
  throw Error{VCF_UNAVAILABLE};
 }
 template<class T>T field(uintptr_t p,size_t offset=0)const{
  require(offset<=UINTPTR_MAX-p,VCF_INVALID);p+=offset;readable(p,sizeof(T));T value;std::memcpy(&value,reinterpret_cast<void*>(p),sizeof(T));return value;
 }
 void function(uintptr_t p,uintptr_t base,uintptr_t rva,size_t size,std::string_view expected)const{
  require(p==base+rva,VCF_UNAVAILABLE);readable(p,size,true);
  std::array<unsigned char,32> digest{};unsigned length=0;
  require(EVP_Digest(reinterpret_cast<void*>(p),size,digest.data(),&length,EVP_sha256(),nullptr)==1&&length==digest.size(),VCF_UNAVAILABLE);
  constexpr char hex[]="0123456789abcdef";std::array<char,64> actual{};
  for(size_t i=0;i<digest.size();++i){actual[i*2]=hex[digest[i]>>4];actual[i*2+1]=hex[digest[i]&15];}
  require(std::string_view(actual.data(),actual.size())==expected,VCF_UNAVAILABLE);
 }
};
struct Bridge {
 uintptr_t bedrock=0,runtime=0;
 Bridge(){
  dl_iterate_phdr([](dl_phdr_info* info,size_t,void* state){
   if(!info->dlpi_name||!*info->dlpi_name){*static_cast<uintptr_t*>(state)=info->dlpi_addr;return 1;}return 0;
  },&bedrock);
  auto* entry=dlsym(RTLD_DEFAULT,"endstone_get_server");Dl_info info{};
  require(entry&&dladdr(entry,&info)&&info.dli_fname&&std::filesystem::path(info.dli_fname).filename()=="libendstone_runtime.so",VCF_UNAVAILABLE);
  runtime=reinterpret_cast<uintptr_t>(info.dli_fbase);require(bedrock&&runtime,VCF_UNAVAILABLE);
 }
 struct State{uintptr_t player;bool ready;uint8_t window;uintptr_t manager;};
 State inspect(endstone::Player& p)const{
  require(p.isValid()&&!p.isDead(),VCF_CLOSED);
  auto& inventory=p.getInventory();
  require(std::string_view(typeid(inventory).name())=="N8endstone4core23EndstonePlayerInventoryE",VCF_UNAVAILABLE);
  Memory memory;auto wrapper=reinterpret_cast<uintptr_t>(&inventory);auto table=memory.field<uintptr_t>(wrapper);
  require(table>=16&&memory.field<intptr_t>(table-16)==-16,VCF_UNAVAILABLE);
  memory.function(memory.field<uintptr_t>(table,296),runtime,held_getter_rva,9,held_getter_hash);
  const auto player=memory.field<uintptr_t>(wrapper,8);memory.readable(player,3344);
  const auto native_table=memory.field<uintptr_t>(player);
  memory.function(memory.field<uintptr_t>(native_table,197*8),bedrock,inventory_open_rva,434,inventory_open_hash);
  const auto stack=memory.field<uintptr_t>(player,2560);
  require(stack&&memory.field<uintptr_t>(stack,16)==player,VCF_UNAVAILABLE);
  const auto context=memory.field<uintptr_t>(stack,56);require(context,VCF_UNAVAILABLE);
  const auto depth=memory.field<uint64_t>(context,40);require(depth<=64,VCF_UNAVAILABLE);
  const auto window=memory.field<uint8_t>(player,3232);require(window<=99,VCF_UNAVAILABLE);
  const auto manager=memory.field<uintptr_t>(player,1368);
  return {player,manager==0&&depth<=1,window,manager};
 }
 uint8_t open(endstone::Player& p)const{
  auto state=inspect(p);require(state.ready,VCF_CONFLICT);
  // The verified vanilla function owns its native context and item behavior.
  reinterpret_cast<void(*)(void*)>(bedrock+inventory_open_rva)(reinterpret_cast<void*>(state.player));
  return inspect(p).window;
 }
 uint8_t craft(endstone::Player& p,const Spec& layout,const Point& position,const std::function<bool()>& proceed)const{
  auto state=inspect(p);require(state.ready,VCF_CONFLICT);
  Memory memory;
  memory.function(bedrock+layout.factory,bedrock,layout.factory,322,layout.hash);
  memory.function(bedrock+0x98ecab0,bedrock,0x98ecab0,361,"d76dcd0f69e26638885049865ea4c5fd8c965dbe11eb64d7e59e0cf9ebef5ce5");
  memory.function(bedrock+0xbcd0510,bedrock,0xbcd0510,3477,"109958d21ff8b4b85fd0fa40d3e46a340c6debce82ee4e2b2cf820f3c2d69e6d");
  auto stack=memory.field<uintptr_t>(state.player,2560);
  auto callback=memory.field<uintptr_t>(memory.field<uintptr_t>(stack),6*8);
  memory.function(callback,bedrock,0xbcd25a0,39,"4cfb5f6fc659853e72b02a33617414a751bff8f49b7c787ed71ae98fcafad255");
  memory.readable(memory.field<uintptr_t>(state.player,1384),sizeof(uintptr_t));
  auto window=reinterpret_cast<uint8_t(*)(void*,int8_t,CraftOwner)>(bedrock+layout.factory)(reinterpret_cast<void*>(state.player),1,CraftOwner{position});
  // The creator notifies other plugins. Do not overwrite a context they
  // opened or continue after the requesting consumer withdrew its request.
  auto created=inspect(p);
  require(created.player==state.player&&created.ready&&created.window==window&&window==next_window(state.window),VCF_CONFLICT);
  require(proceed(),VCF_CLOSED);
  Memory current;
  require(current.field<uintptr_t>(state.player,2560)==stack&&current.field<uintptr_t>(current.field<uintptr_t>(stack),6*8)==callback,VCF_CONFLICT);
  CraftContext context{state.player,1,{},position};
  reinterpret_cast<void(*)(void*,const CraftContext*)>(callback)(reinterpret_cast<void*>(stack),&context);
  auto activated=inspect(p);
  // Crafting has no ContainerManagerModel; BDS owns its stack context.
  require(activated.player==state.player&&!activated.ready&&!activated.manager&&activated.window==window,VCF_UNAVAILABLE);
  require(proceed(),VCF_CLOSED);
  p.sendPacket(46,container_open(window,1,position));
  return window;
 }
 uint8_t station(endstone::Player& p,const Spec& layout,const Point& position,const std::function<bool()>& proceed)const{
  if(layout.block.empty())return open(p);
  if(layout.type==1)return craft(p,layout,position,proceed);
  auto state=inspect(p);require(state.ready,VCF_CONFLICT);
  Memory memory;const auto function=bedrock+layout.factory;
  memory.function(function,bedrock,layout.factory,layout.factory_size,layout.hash);
  // Native Linux caller passes Player*, const BlockPos*, and ActorUniqueID
  // by value in rdi/rsi/rdx. BDS allocates and owns the complete model.
  reinterpret_cast<void(*)(void*,const Point*,int64_t)>(function)(reinterpret_cast<void*>(state.player),&position,-1);
  auto result=inspect(p);require(result.manager&&!result.ready,VCF_UNAVAILABLE);return result.window;
 }
};
void var(std::string& out,uint64_t value){do{auto b=static_cast<uint8_t>(value&127);value>>=7;out+=static_cast<char>(b|(value?128:0));}while(value);}
void signed_var(std::string& out,int32_t value){var(out,(uint32_t(value)<<1)^(value<0?UINT32_MAX:0));}
std::string block_update(Point point,uint32_t runtime){std::string out;signed_var(out,point.x);signed_var(out,point.y);signed_var(out,point.z);var(out,runtime);var(out,2);var(out,0);return out;}
std::string container_open(uint8_t window,uint8_t type,Point position){std::string out{static_cast<char>(window),static_cast<char>(type)};signed_var(out,position.x);signed_var(out,position.y);signed_var(out,position.z);var(out,1);return out;}
Point projection(endstone::Player& p){
 const auto location=p.getLocation();
 require(std::isfinite(location.getX())&&std::isfinite(location.getY())&&std::isfinite(location.getZ()));
 require(std::abs(location.getX())<=30000000&&std::abs(location.getZ())<=30000000&&location.getY()>=-64&&location.getY()<=319);
 Point feet{static_cast<int32_t>(std::floor(location.getX())),static_cast<int32_t>(std::floor(location.getY())),static_cast<int32_t>(std::floor(location.getZ()))};
 for(auto offset:std::array<Point,6>{{{1,1,0},{-1,1,0},{0,1,1},{0,1,-1},{0,2,1},{0,2,-1}}}){
  Point q{feet.x+offset.x,feet.y+offset.y,feet.z+offset.z};
  if(q.y>=-63&&q.y<=318&&p.getDimension().getBlockAt(q.x,q.y,q.z)->getType()=="minecraft:air")return q;
 }
 throw Error{VCF_UNAVAILABLE};
}
std::string ping(uint32_t nonce){std::string data(9,'\0');for(size_t i=0;i<8;++i)data[i]=static_cast<char>(uint64_t(nonce)>>(8*i));data[8]=1;return data;}
bool reply(std::string_view data,uint32_t nonce){
 if(data.size()!=9||data[8]!=1)return false;uint64_t value=0;
 for(size_t i=0;i<8;++i)value|=uint64_t(static_cast<uint8_t>(data[i]))<<(8*i);
 return value==uint64_t(nonce)*1000000;
}
struct Reader {
 std::string_view data;size_t offset=0;
 uint8_t byte(){require(offset<data.size());return static_cast<uint8_t>(data[offset++]);}
 uint64_t var(unsigned bits=32){uint64_t value=0;
  for(unsigned shift=0;shift<bits;shift+=7){auto b=byte();if(bits-shift<7)require((b&127)<(uint32_t(1)<<(bits-shift)));
   value|=uint64_t(b&127)<<shift;if(!(b&128)){require(shift==0||(b&127)!=0);return value;}}
  throw Error{VCF_INVALID};
 }
 int32_t signed_var(){auto value=static_cast<uint32_t>(var());return static_cast<int32_t>((value>>1)^(uint32_t(0)-(value&1)));}
 Point point(){return {signed_var(),signed_var(),signed_var()};}
};
}
struct NativeUi::Impl {
 struct View{vcf_handle owner,id;std::string player,kind,dimension,permission;uint32_t nonce;int window=-1;
  bool prepared=false,ack=false,activating=false,observed=false,active=false,closing=false,close_seen=false,superseded=false;
  bool projected=false,projecting=false,restored=false,attempted=false,close_sent=false;
  Point position;
  Clock::time_point queued=Clock::now(),next_check{},closed{};};
 endstone::Server& server;Engine& engine;Bridge bridge;std::map<vcf_handle,View> views;
 WindowLeases close_leases;
 uint32_t nonce=std::random_device{}()&0x7fffffff;vcf_handle cursor=0;
 Impl(endstone::Server&s,Engine&e):server(s),engine(e){}
 endstone::Player* player(std::string_view id){for(auto*p:server.getOnlinePlayers())if(p->getUniqueId().str()==id)return p;return nullptr;}
 bool allowed(endstone::Player&p,const View&v){const auto* row=resolve(v.kind);
  return row&&p.isValid()&&!p.isDead()&&p.getGameVersion()=="1.26.45"&&p.getDeviceOS()=="Windows"&&p.getDimension().getName()==v.dimension
   &&p.hasPermission("remoteworkstations.use")&&p.hasPermission(std::string(row->permission))&&(v.permission.empty()||p.hasPermission(v.permission));
 }
 void retire(View&v,vcf_status result){
  if(v.window>=1&&v.window<=99){auto now=milliseconds();close_leases.retire(v.player,static_cast<uint8_t>(v.window),now+60000,now);}
  try{engine.retired(v.owner,v.id,result);}catch(const Error&) {}
 }
 void restore(endstone::Player& p,View& v){
  if(v.restored)return;
  if(v.projected&&p.getDimension().getName()==v.dimension){
   auto data=p.getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getData();
   v.projecting=true;
   try{p.sendPacket(21,block_update(v.position,data->getRuntimeId()));}catch(...){v.projecting=false;throw;}
   v.projecting=false;
  }
  v.restored=true;
 }
 void close(endstone::Player* p,View& v){
  if(!v.closing){v.closing=true;v.closed=Clock::now();}
  if(p&&p->isValid()&&!p->isDead()&&v.window>=1&&!v.superseded&&!v.close_sent&&!spec(v.kind)->block.empty()){
   auto current=bridge.inspect(*p);
   if(current.window!=v.window)v.superseded=true;
   else if(!current.ready){
    v.close_sent=true;
    try{p->sendPacket(47,std::string{static_cast<char>(v.window),static_cast<char>(247),1});}catch(...){v.close_sent=false;throw;}
   }
  }
  if(p)restore(*p,v);
 }
 bool poll(View&v){
  auto*p=player(v.player);const auto now=Clock::now();
  if(!p||!p->isValid()){retire(v,VCF_CLOSED);return true;}
  if(v.superseded){restore(*p,v);retire(v,VCF_CLOSED);return true;}
  if(!allowed(*p,v)){
   try{engine.session(v.owner,v.id).result=VCF_DENIED;}catch(const Error&){}
   close(p,v);
  }
  if(v.closing){
   close(p,v);
   if(v.superseded){retire(v,VCF_CLOSED);return true;}
   if(spec(v.kind)->block.empty()||v.window<0||p->isDead()||bridge.inspect(*p).ready){retire(v,VCF_CLOSED);return true;}
   if(now-v.closed>std::chrono::seconds(5)){p->kick("Native inventory close was not acknowledged; rejoin to continue.");retire(v,VCF_QUARANTINED);return true;}
   return false;
  }
  if(!v.prepared){
   auto native=bridge.inspect(*p);require(native.ready,VCF_CONFLICT);
   require(!close_leases.reserved(v.player,next_window(native.window),milliseconds()),VCF_CONFLICT);
   if(!spec(v.kind)->block.empty()){
    v.position=projection(*p);auto data=server.createBlockData(std::string(spec(v.kind)->block));
    v.projecting=true;v.projected=true;
    try{p->sendPacket(21,block_update(v.position,data->getRuntimeId()));}catch(...){v.projecting=false;throw;}
    v.projecting=false;
   }else v.restored=true;
   v.prepared=true;p->sendPacket(115,ping(v.nonce));return false;
  }
  if(!v.active){
   require(now-v.queued<std::chrono::seconds(10),VCF_UNAVAILABLE);
   if(!v.ack||now-v.queued<std::chrono::milliseconds(500))return false;
   engine.authorize(v.owner,v.id,VCF_GUARD_DISPATCH);
   auto native=bridge.inspect(*p);require(native.ready,VCF_CONFLICT);
   require(!close_leases.reserved(v.player,next_window(native.window),milliseconds()),VCF_CONFLICT);
   if(!spec(v.kind)->block.empty())require(p->getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getType()=="minecraft:air",VCF_CONFLICT);
   v.activating=true;v.attempted=true;
   uint8_t window;
   try{window=bridge.station(*p,*spec(v.kind),v.position,[&]{
    return !v.closing&&!v.superseded&&!v.close_seen&&allowed(*p,v)
     &&p->getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getType()=="minecraft:air";
   });}catch(...){v.activating=false;throw;}
   v.activating=false;
   // Native sends can synchronously disable the requesting consumer. Its
   // close marks this borrowed view; tick owns the eventual erasure.
   if(v.closing)return false;
   require(v.observed&&v.window==window&&!v.superseded,VCF_UNAVAILABLE);
   v.active=true;engine.opened(v.owner,v.id,VCF_OK);
   if(engine.session(v.owner,v.id).state!=VCF_ACTIVE){close(p,v);return false;}
  }
  if(v.close_seen||now>=v.next_check){
   v.next_check=now+std::chrono::milliseconds(250);
   if(bridge.inspect(*p).ready){restore(*p,v);retire(v,VCF_OK);return true;}
   engine.authorize(v.owner,v.id,VCF_GUARD_ACTIVE);
  }
  if(v.close_seen||now-v.queued>std::chrono::minutes(20))close(p,v);
  return false;
 }
};
NativeUi::NativeUi(endstone::Server&s,Engine&e):impl_(std::make_unique<Impl>(s,e)){}
NativeUi::~NativeUi()=default;
bool NativeUi::supports(std::string_view id){return spec(id)!=nullptr;}
vcf_status NativeUi::open(const Session&s){
 const auto* layout=spec(s.kind);
 if(!layout||s.mode!=(layout->block.empty()?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT)||!s.title.empty()||!s.rules.empty())return VCF_UNAVAILABLE;
 for(const auto& slot:s.inventory.slots)if(!slot.item.empty()||slot.policy!=(VCF_INSERT|VCF_EXTRACT))return VCF_UNAVAILABLE;
 if(!s.dimension.empty()||!s.entity_id.empty()||!s.held_id.empty()||s.x||s.y||s.z)return VCF_INVALID;
 auto*p=impl_->player(s.player);if(!p)return VCF_CLOSED;require(impl_->views.size()<100,VCF_CAPACITY);
 for(const auto&[_,view]:impl_->views)require(view.player!=s.player,VCF_CONFLICT);
 Impl::View v{s.owner,s.id,s.player,s.kind,p->getDimension().getName(),s.permission,++impl_->nonce};
 require(impl_->allowed(*p,v),VCF_DENIED);require(impl_->bridge.inspect(*p).ready,VCF_CONFLICT);
 impl_->views.emplace(s.id,std::move(v));return VCF_PENDING;
}
vcf_status NativeUi::close(const Session&s){auto it=impl_->views.find(s.id);if(it==impl_->views.end())return VCF_OK;
 impl_->close(impl_->player(it->second.player),it->second);
 // This is the player's real inventory. Relinquish the observation lease;
 // do not forge a close, change its cursor or destroy BDS's native manager.
 return spec(s.kind)->block.empty()?VCF_OK:VCF_PENDING;
}
void NativeUi::tick(){
 impl_->close_leases.expire(milliseconds());
 const auto deadline=Clock::now()+std::chrono::milliseconds(2);auto remaining=impl_->views.size();std::vector<vcf_handle> done;
 while(remaining--&&!impl_->views.empty()){
  auto it=impl_->views.upper_bound(impl_->cursor);if(it==impl_->views.end())it=impl_->views.begin();impl_->cursor=it->first;
  try{if(impl_->poll(it->second))done.push_back(it->first);}
  catch(...){
   auto& v=it->second;vcf_status result=VCF_INTERNAL;try{throw;}catch(const Error&e){result=e.status;}catch(...){}
   try{auto& session=impl_->engine.session(v.owner,v.id);if(session.state!=VCF_TERMINAL){session.result=result;impl_->engine.close(v.owner,v.id);}}catch(const Error&){}
   auto*p=impl_->player(v.player);try{impl_->close(p,v);}catch(...){}
   if(p&&v.closing&&Clock::now()-v.closed>std::chrono::seconds(5)){
    try{p->kick("Native inventory state could not be verified; rejoin to continue.");}catch(...){}
    impl_->retire(v,VCF_QUARANTINED);done.push_back(it->first);continue;
   }
   if(v.window<0||spec(v.kind)->block.empty()){
    if(p&&v.attempted&&!spec(v.kind)->block.empty())try{if(!impl_->bridge.inspect(*p).ready)p->kick("Native inventory setup was incomplete; rejoin to continue.");}catch(...){}
    impl_->retire(v,result);done.push_back(it->first);
   }
  }
  if(Clock::now()>=deadline)break;
 }
 for(auto id:done)impl_->views.erase(id);
}
void NativeUi::receive(endstone::PacketReceiveEvent&e){
 if(!e.getPlayer()||e.isCancelled())return;
 auto player=e.getPlayer()->getUniqueId().str();
 if(e.getPacketId()==47&&e.getPayload().size()==3){
  auto incoming=static_cast<uint8_t>(e.getPayload()[0]);
  if(impl_->close_leases.reserved(player,incoming,milliseconds()))try{
   auto current=impl_->bridge.inspect(*e.getPlayer());
   if(impl_->close_leases.blocks_close(player,incoming,!current.ready,current.window,milliseconds())){e.setCancelled(true);return;}
  }catch(...){return;}
 }
 for(auto&[_,v]:impl_->views)if(!v.closing&&v.player==e.getPlayer()->getUniqueId().str()){
  if(e.getPacketId()==115&&v.prepared&&reply(e.getPayload(),v.nonce))v.ack=true;
  if(e.getPacketId()==47&&e.getPayload().size()==3&&static_cast<uint8_t>(e.getPayload()[0])==v.window)v.close_seen=true;
  if(e.getPacketId()==147&&v.window>=1&&!v.superseded&&!spec(v.kind)->block.empty()&&!impl_->allowed(*e.getPlayer(),v))e.setCancelled(true);
  return;
 }
}
void NativeUi::sent(endstone::PacketSendEvent&e){
 if(!e.getPlayer()||e.isCancelled())return;
 auto player=e.getPlayer()->getUniqueId().str();
 if(e.getPacketId()==46&&!e.getPayload().empty())impl_->close_leases.observed_open(player,static_cast<uint8_t>(e.getPayload()[0]));
 for(auto&[_,v]:impl_->views)if(v.player==player){
  try{
   if(e.getPacketId()==46){Reader r{e.getPayload()};auto window=r.byte(),type=r.byte();auto position=r.point();r.var(64);require(r.offset==r.data.size());
    if(v.activating&&!v.observed&&type==spec(v.kind)->type&&window>=1&&window<=99&&(spec(v.kind)->block.empty()||position==v.position)){v.window=window;v.observed=true;}else v.superseded=true;
   }else if(e.getPacketId()==100)v.superseded=true;
   else if(e.getPacketId()==47&&e.getPayload().size()==3&&static_cast<uint8_t>(e.getPayload()[0])==v.window)v.close_seen=true;
   else if(e.getPacketId()==21&&v.projected&&!v.projecting&&v.prepared){
    Reader r{e.getPayload()};if(r.point()==v.position){v.restored=true;v.projected=false;v.close_seen=true;}
   }
  }catch(const Error&){v.superseded=true;}
  return;
 }
}
void NativeUi::shutdown(){for(auto&[_,v]:impl_->views)try{impl_->close(impl_->player(v.player),v);}catch(...){}impl_->views.clear();}
}
