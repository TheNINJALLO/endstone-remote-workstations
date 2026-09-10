#include "held_storage.hpp"
#include "native_items.hpp"
#include "inventory_watch.hpp"
#include "inventory_writer.hpp"
#include "../../inventory_journal.hpp"
#include "../../saved_item.hpp"
#include "../../held_input.hpp"
#include <oni/vcf/window_leases.hpp>
#include <endstone/server.h>
#include <endstone/player.h>
#include <endstone/inventory/item_type.h>
#include <endstone/level/dimension.h>
#include <endstone/block/block_data.h>
#include <endstone/event/server/packet_receive_event.h>
#include <endstone/event/server/packet_send_event.h>
#include <chrono>
#include <random>
#include <cmath>

namespace oni::vcf::platform::linux_native {
namespace {
using Clock=std::chrono::steady_clock;using Bytes=std::vector<uint8_t>;
namespace saved=inventory::saved;
uint64_t milliseconds(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());}
struct Point{int32_t x,y,z;bool operator==(const Point&)const=default;};
void var(std::string& out,uint64_t n){do{auto b=n&127;n>>=7;out+=static_cast<char>(b|(n?128:0));}while(n);}
void signed_var(std::string& out,int32_t n){var(out,(uint32_t(n)<<1)^(n<0?UINT32_MAX:0));}
void point(std::string& out,Point p){signed_var(out,p.x);signed_var(out,p.y);signed_var(out,p.z);}
std::string block(Point p,uint32_t id){std::string out;point(out,p);var(out,id);var(out,2);var(out,0);return out;}
std::string ping(uint32_t n){std::string out(9,'\0');for(unsigned i=0;i<8;++i)out[i]=static_cast<char>(uint64_t(n)>>(8*i));out[8]=1;return out;}
bool pong(std::string_view s,uint32_t n){if(s.size()!=9||s[8]!=1)return false;uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(static_cast<uint8_t>(s[i]))<<(8*i);return v==uint64_t(n)*1000000;}
std::string uuid(){std::random_device random;std::array<uint8_t,16> bytes;for(auto& b:bytes)b=static_cast<uint8_t>(random());bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;std::string out;constexpr char hex[]="0123456789abcdef";for(size_t i=0;i<16;++i){if(i==4||i==6||i==8||i==10)out+='-';out+=hex[bytes[i]>>4];out+=hex[bytes[i]&15];}return out;}
const storage::NetworkSlot& at(const storage::State& state,storage::Address a){if(a.area==storage::Area::player)return state.player.at(a.slot);if(a.area==storage::Area::storage)return state.storage.at(a.slot);return state.cursor;}
}
struct HeldStorage::Impl:std::enable_shared_from_this<Impl> {
 struct View {
  vcf_handle owner,id;std::string player,permission,dimension,title,identity;uint32_t source=0,nonce=0;uint8_t window=0,stage=0;
  bool ack=false,closing=false,close_requested=false,dismissed=false,close_ack_sent=false,superseded=false,projected=false,sending=false,authorizing=false,restored=false,committing=false;
  vcf_status result=VCF_OK;Point position{};Clock::time_point created=Clock::now(),closed{};
  held::InputQuiet quiet{milliseconds()};
  inventory::SavedInventory image;InventoryWatch watch;inventory::Revision::Stamp stamp;
  std::unique_ptr<storage::Reservation> reservation;std::deque<Bytes> requests;
  std::map<Bytes,wire::Descriptor> templates;size_t template_bytes=0;
 };
 endstone::Server& server;std::shared_ptr<Engine> engine;std::shared_ptr<inventory::InventoryJournal> journal;NativeItems items;
 std::map<vcf_handle,std::shared_ptr<View>> views;WindowLeases retired;bool stopped=false;uint32_t nonce=std::random_device{}()&0x7fffffff;int32_t next_identity=1000000;vcf_handle cursor=0;
 Impl(endstone::Server& s,std::shared_ptr<Engine> e,std::shared_ptr<inventory::InventoryJournal> j):server(s),engine(std::move(e)),journal(std::move(j)){}
 endstone::Player* lookup(std::string_view id){for(auto*p:server.getOnlinePlayers())if(p->getUniqueId().str()==id)return p;return nullptr;}
 saved::Limit limit(){return [this](std::string_view id){auto* item=server.getRegistry<endstone::ItemType>().get(endstone::ItemTypeId(id));return item?static_cast<uint32_t>(item->getMaxStackSize()):0;};}
 endstone::Player& player(const View& v){auto*p=lookup(v.player);require(p&&p->isValid()&&!p->isDead(),VCF_CLOSED);return *p;}
 void allowed(View& v){
  require(!stopped&&!v.closing&&!v.superseded,VCF_CLOSED);auto& p=player(v);
  require(p.getGameVersion()=="1.26.45"&&p.getDeviceOS()=="Windows",VCF_UNAVAILABLE);
  require(p.getDimension().getName()==v.dimension&&p.getInventory().getHeldItemSlot()==static_cast<int>(v.source),VCF_STALE);
  for(const auto& permission:{std::string("remoteworkstations.use"),std::string("remoteworkstations.open.shulker"),std::string("remoteworkstations.inventory.write"),v.permission})require(permission.empty()||p.hasPermission(permission),VCF_DENIED);
  require(native_player_ready(p),VCF_CONFLICT);require(v.committing||!journal->blocked(v.player),VCF_QUARANTINED);
 }
 inventory::SavedInventory read(View& v){auto& p=player(v);const auto container=items.container(p);inventory::SavedInventory out;size_t bytes=0;for(uint32_t i=0;i<36;++i){out[i]=items.slot(container,i);bytes+=out[i].size();require(bytes<=128*1024,VCF_CAPACITY);}return out;}
 void guard(View& v){allowed(v);v.watch.validate(player(v),v.stamp);require(read(v)==v.image,VCF_STALE);v.watch.validate(player(v),v.stamp);}
 void authorize(View& v,uint32_t phase){
  require(!v.authorizing,VCF_REENTRANT);allowed(v);v.authorizing=true;
  try{engine->authorize(v.owner,v.id,phase);allowed(v);}catch(...){v.authorizing=false;throw;}v.authorizing=false;
 }
 void send(View& v,uint32_t id,const std::string& payload){
  require(!v.sending,VCF_REENTRANT);auto& p=player(v);v.sending=true;try{p.sendPacket(id,payload);}catch(...){v.sending=false;throw;}v.sending=false;
 }
 void send(View& v,uint32_t id,const Bytes& payload){send(v,id,std::string(reinterpret_cast<const char*>(payload.data()),payload.size()));}
 void ping(View& v){require(nonce<0x7ffffffe,VCF_CAPACITY);v.nonce=++nonce;v.ack=false;send(v,115,linux_native::ping(v.nonce));}
 wire::Descriptor descriptor(View& v,const storage::NetworkSlot& slot){
  const auto& item=slot.value.item;if(item.empty())return {};auto found=v.templates.find(item.nbt);
  if(found==v.templates.end()){
   auto normalized=item;normalized.count=1;auto value=items.descriptor(saved::encode(normalized),1);
   require(v.templates.size()<256&&item.nbt.size()+value.user_data.size()<=1024*1024-v.template_bytes,VCF_CAPACITY);
   const auto used=item.nbt.size()+value.user_data.size();found=v.templates.emplace(item.nbt,std::move(value)).first;v.template_bytes+=used;
  }
  auto value=found->second;value.count=static_cast<uint16_t>(item.count);value.network_id=slot.identity;return value;
 }
 void content(View& v){
  const auto& state=v.reservation->state();wire::Content inventory;inventory.window=0;inventory.container.role=12;
  for(uint32_t i=0;i<36;++i){if(i==v.source){auto source=state.player[i];source.value.item=saved::decode(v.image[i],limit());inventory.items.push_back(descriptor(v,source));}else inventory.items.push_back(descriptor(v,state.player[i]));}
  wire::Content stored;stored.window=v.window;stored.container.role=30;for(const auto& slot:state.storage)stored.items.push_back(descriptor(v,slot));
  wire::Slot cursor_slot;cursor_slot.window=124;cursor_slot.container=wire::Container{59,{}};cursor_slot.item=descriptor(v,state.cursor);
  // Complete serialization precedes the first packet. No partial view on a
  // metadata/size failure; the close path restores the actual native inventory.
  const auto first=wire::encode(inventory),second=wire::encode(stored),third=wire::encode(cursor_slot);guard(v);
  send(v,49,first);send(v,49,second);send(v,50,third);
 }
 vcf_status write(View& v,const storage::State& before,const storage::State& after){
  bool published=false;
  try{
   guard(v);authorize(v,VCF_GUARD_ACTIVE);auto replacement=saved::project_shulker(v.image,v.source,before,after,limit());
   if(replacement==v.image)return VCF_OK;
   InventoryWriter writer([this,id=v.player]{return lookup(id);},[this,&v]{guard(v);});require(writer.before()==v.image,VCF_STALE);
   const auto transaction=journal->reserve({v.player,v.image,replacement});
   v.committing=true;struct Clear{bool& flag;~Clear(){flag=false;}} clear{v.committing};
   const auto result=writer.commit(replacement,[this,&v]{authorize(v,VCF_GUARD_ACTIVE);},[this,transaction](Boundary b){journal->record(transaction,b);});journal->finish(transaction,result);
   if(result.status!=VCF_OK){server.getLogger().warning("Held storage transaction {} refused while {}: status {}.",transaction,result.operation,result.status);return result.uncertain?VCF_QUARANTINED:VCF_CONFLICT;}
   published=true;v.image=std::move(replacement);v.stamp=v.watch.capture();guard(v);return VCF_OK;
  }catch(const Error& e){return published||e.status==VCF_QUARANTINED?VCF_QUARANTINED:VCF_CONFLICT;}catch(...){return VCF_QUARANTINED;}
 }
 void requests(View& v,const Bytes& payload){
  const auto requests=storage::decode(payload);std::vector<storage::Response> responses;bool committed=false;
  for(const auto& request:requests){
   try{authorize(v,VCF_GUARD_ACTIVE);const auto result=v.reservation->apply(request,v.id,v.reservation->state().revision);committed|=result.committed;
    const auto& state=v.reservation->state();std::map<uint8_t,std::map<uint8_t,storage::ResponseSlot>> groups;
    for(const auto& action:request.actions){for(const auto& ref:{std::optional<storage::Reference>(action.source),action.destination})if(ref){const auto& slot=at(state,storage::Layout("shulker").resolve(*ref));
     storage::ResponseSlot reply;reply.slot=ref->slot;reply.hotbar_slot=ref->slot;reply.count=static_cast<uint8_t>(slot.value.item.count);if(reply.count)reply.network_id=slot.identity;groups[ref->role][ref->slot]=std::move(reply);}}
    storage::Response response;response.request_id=request.id;response.groups.emplace();for(auto&[role,slots]:groups){storage::ResponseGroup group;group.role=role;for(auto&[_,slot]:slots)group.slots.push_back(std::move(slot));response.groups->push_back(std::move(group));}responses.push_back(std::move(response));
   }catch(const Error&){responses.push_back({1,request.id,{}});if(v.reservation->closed())throw;}
  }
  send(v,148,storage::encode_responses(responses));content(v);if(committed)engine->committed(v.owner,v.id,v.reservation->state().revision);
 }
 void restore(View& v){
  if(v.restored)return;auto*p=lookup(v.player);if(!p||!p->isValid()||p->isDead()){v.restored=true;return;}
  if(v.projected&&p->getDimension().getName()==v.dimension){const auto actual=p->getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getData();send(v,21,block(v.position,actual->getRuntimeId()));v.projected=false;}
  if(!v.superseded&&native_player_ready(*p)){
   const auto container=items.container(*p);const auto watch=watch_inventory(*p);const auto stamp=watch.capture();wire::Content current;current.window=0;current.container.role=12;
   for(uint32_t i=0;i<36;++i)current.items.push_back(items.native_descriptor(container,i));const auto bytes=wire::encode(current);watch.validate(*p,stamp);
   wire::Slot clear;clear.window=124;clear.container=wire::Container{59,{}};send(v,50,wire::encode(clear));send(v,49,bytes);
  }
  v.restored=true;
 }
 void close(View& v,vcf_status result=VCF_OK){
  if(v.closing)return;v.closing=true;v.closed=Clock::now();v.result=result;v.requests.clear();if(v.reservation){next_identity=std::max(next_identity,v.reservation->state().next_identity);v.reservation->cancel();}
  auto*p=lookup(v.player);if(p&&p->isValid()&&!p->isDead()&&v.stage>=3&&!v.superseded){send(v,47,std::string{static_cast<char>(v.window),static_cast<char>(247),static_cast<char>(!v.dismissed)});v.close_ack_sent=v.dismissed;}
  restore(v);
 }
 bool poll(View& v){
  if(!v.closing&&(v.close_requested||v.dismissed||v.superseded))close(v,v.result);
  if(v.closing){
   if(!v.restored)restore(v);
   if(v.dismissed&&!v.close_ack_sent&&!v.superseded&&v.stage>=3){auto*p=lookup(v.player);if(p&&p->isValid()&&!p->isDead())send(v,47,std::string{static_cast<char>(v.window),static_cast<char>(247),0});v.close_ack_sent=true;}
   if(!lookup(v.player)||v.superseded||v.dismissed||v.stage<3){retire(v);return true;}
   if(Clock::now()-v.closed>std::chrono::seconds(5)){auto*p=lookup(v.player);if(p)p->kick("Held storage close was not acknowledged; rejoin to restore the current inventory.");v.result=VCF_QUARANTINED;retire(v);return true;}return false;
  }
  guard(v);authorize(v,v.stage<4?VCF_GUARD_READY:VCF_GUARD_ACTIVE);const auto elapsed=Clock::now()-v.created;
  require(elapsed<std::chrono::seconds(v.stage<4?12:300),VCF_CLOSED);
  if(v.stage==0&&v.quiet.ready(milliseconds())){
   auto& p=player(v);const auto at=p.getLocation();require(std::isfinite(at.getX())&&std::isfinite(at.getY())&&std::isfinite(at.getZ()));
   require(std::abs(at.getX())<30000000&&std::abs(at.getZ())<30000000&&at.getY()>=-64&&at.getY()<=317);v.position={static_cast<int32_t>(std::floor(at.getX()))+1,static_cast<int32_t>(std::floor(at.getY()))+1,static_cast<int32_t>(std::floor(at.getZ()))};
   require(p.getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getType()=="minecraft:air",VCF_CONFLICT);
   send(v,21,block(v.position,server.createBlockData("minecraft:undyed_shulker_box")->getRuntimeId()));v.projected=true;v.stage=1;ping(v);
  }else if(v.stage==1&&v.ack){
   // Network NBT compound: id and CustomName strings; scalar lengths are
   // unsigned varints, unlike the complete saved-item compounds above.
   std::string payload;point(payload,v.position);payload+=char(10);var(payload,0);
   auto field=[&](std::string_view key,std::string_view value){payload+=char(8);var(payload,key.size());payload+=key;var(payload,value.size());payload+=value;};
   field("id","ShulkerBox");field("CustomName",v.title);payload+=char(0);send(v,56,payload);v.stage=2;ping(v);
  }else if(v.stage==2&&v.ack){std::string payload{static_cast<char>(v.window),0};point(payload,v.position);var(payload,1);send(v,46,payload);v.stage=3;ping(v);
  }else if(v.stage==3&&v.ack){content(v);v.stage=4;engine->opened(v.owner,v.id,VCF_OK);
  }else if(v.stage==4&&!v.requests.empty()){auto payload=std::move(v.requests.front());v.requests.pop_front();requests(v,payload);}
  return false;
 }
 void retire(View& v){retired.retire(v.player,v.window,milliseconds()+60000,milliseconds());try{engine->retired(v.owner,v.id,v.result);}catch(...){} }
};
HeldStorage::HeldStorage(endstone::Server& s,std::shared_ptr<Engine> e,std::shared_ptr<inventory::InventoryJournal> j):impl_(std::make_shared<Impl>(s,std::move(e),std::move(j))){}
HeldStorage::~HeldStorage()=default;
vcf_status HeldStorage::open(const Session& session){
 auto self=impl_;require(!self->stopped,VCF_CLOSED);require(session.kind=="shulker"&&session.mode==VCF_REAL_SOURCE,VCF_UNAVAILABLE);require(session.dimension.empty()&&!session.x&&!session.y&&!session.z&&session.entity_id.empty()&&session.rules.empty()&&session.title.size()<=128);
 require(self->views.size()<16,VCF_CAPACITY);for(const auto&[_,v]:self->views)require(v->player!=session.player,VCF_CONFLICT);
 auto*p=self->lookup(session.player);require(p,VCF_CLOSED);auto view=std::make_shared<Impl::View>();auto& v=*view;v.owner=session.owner;v.id=session.id;v.player=session.player;v.permission=session.permission;v.dimension=p->getDimension().getName();v.title=session.title.empty()?"Shulker Box":session.title;
 const auto source=p->getInventory().getHeldItemSlot();require(source>=0&&source<9);v.source=static_cast<uint32_t>(source);self->allowed(v);v.watch=watch_inventory(*p);v.stamp=v.watch.capture();v.image=self->read(v);self->guard(v);
 auto initial=saved::read_shulker(v.image,v.source,v.id,self->next_identity,self->limit());v.identity=saved::durable_id(v.image[v.source]);if(!session.held_id.empty())require(v.identity==session.held_id,VCF_STALE);
 v.identity=v.identity.empty()?uuid():v.identity;const auto identified=saved::with_durable_id(v.image[v.source],v.identity);
 // Validate every full native item and display descriptor before assigning an
 // identity or displaying the first block. The write is journalled separately.
 for(const auto& slot:initial.player)if(!slot.value.item.empty())self->descriptor(v,slot);for(const auto& slot:initial.storage)if(!slot.value.item.empty())self->descriptor(v,slot);
 auto full_source=initial.player[v.source];full_source.value.item=saved::decode(identified,self->limit());self->descriptor(v,full_source);
 self->authorize(v,VCF_GUARD_DISPATCH);self->guard(v);
 uint8_t window=0;for(uint8_t candidate=70;candidate<=99;++candidate)if(!self->retired.reserved(v.player,candidate,milliseconds())){window=candidate;break;}require(window,VCF_CAPACITY);v.window=window;
 if(identified!=v.image[v.source]){
  auto next=v.image;next[v.source]=identified;InventoryWriter writer([self,id=v.player]{return self->lookup(id);},[self,view]{self->guard(*view);});
  const auto transaction=self->journal->reserve({v.player,v.image,next});v.committing=true;struct Clear{bool& flag;~Clear(){flag=false;}} clear{v.committing};
  const auto result=writer.commit(next,[self,view]{self->authorize(*view,VCF_GUARD_DISPATCH);},[self,transaction](Boundary b){self->journal->record(transaction,b);});self->journal->finish(transaction,result);require(result.status==VCF_OK,result.status);
  v.image=std::move(next);v.stamp=v.watch.capture();initial=saved::read_shulker(v.image,v.source,v.id,self->next_identity,self->limit());
 }
 std::weak_ptr<Impl> weak=self;std::weak_ptr<Impl::View> lease=view;
 auto lock=[weak,lease]{auto owner=weak.lock();auto view=lease.lock();require(owner&&view,VCF_CLOSED);return std::pair{owner,view};};
 v.reservation=std::make_unique<storage::Reservation>(initial,storage::Layout("shulker"),[lock]{auto[s,v]=lock();return saved::read_shulker(s->read(*v),v->source,v->id,1,s->limit());},[lock](const auto&a,const auto&b){auto[s,v]=lock();return s->write(*v,a,b);},[lock]{auto[s,v]=lock();s->guard(*v);});
 self->next_identity=initial.next_identity;self->views.emplace(v.id,view);return VCF_PENDING;
}
vcf_status HeldStorage::close(const Session& session){auto self=impl_;auto it=self->views.find(session.id);if(it==self->views.end())return VCF_OK;self->close(*it->second);return VCF_PENDING;}
void HeldStorage::tick(){auto self=impl_;self->retired.expire(milliseconds());if(self->views.empty())return;auto it=self->views.upper_bound(self->cursor);if(it==self->views.end())it=self->views.begin();const auto view=it->second;self->cursor=view->id;
 try{if(self->poll(*view))self->views.erase(view->id);}catch(...){vcf_status result=VCF_INTERNAL;try{throw;}catch(const Error&e){result=e.status;}catch(...){}
  self->server.getLogger().warning("Held storage ticket {} closed with status {} at stage {}.",view->id,result,view->stage);
  try{if(view->closing&&!view->restored)throw Error{VCF_QUARANTINED};self->close(*view,result);}catch(...){view->result=VCF_QUARANTINED;if(auto*p=self->lookup(view->player))p->kick("Held storage restoration could not be verified; rejoin to continue.");self->retire(*view);self->views.erase(view->id);}
 }
}
void HeldStorage::receive(endstone::PacketReceiveEvent& event){
 auto self=impl_;auto*p=event.getPlayer();if(!p||event.isCancelled())return;const auto identity=p->getUniqueId().str();const auto& data=event.getPayload();const auto packet=event.getPacketId();
 if(packet==47&&data.size()==3){const auto window=static_cast<uint8_t>(data[0]);if(self->retired.reserved(identity,window,milliseconds())&&native_stale_close(*p,window)){event.setCancelled(true);return;}}
 for(const auto&[_,view]:self->views){auto& v=*view;if(v.player!=identity||v.superseded||v.sending)continue;
  try{
   if(packet==115&&pong(data,v.nonce)){v.ack=true;return;}
   if(packet==47&&data.size()==3&&static_cast<uint8_t>(data[0])==v.window){event.setCancelled(true);v.dismissed=true;v.close_requested=true;return;}
   if(packet==147){event.setCancelled(true);require(!v.closing&&v.stage==4&&v.requests.size()<32&&data.size()<=65536,VCF_CAPACITY);v.requests.emplace_back(data.begin(),data.end());return;}
   if(packet==144&&held::inventory_input({reinterpret_cast<const uint8_t*>(data.data()),data.size()})){
    event.setCancelled(true);
    if(v.stage==0&&!v.closing){v.quiet.observed(milliseconds());return;}
    v.result=VCF_DENIED;v.close_requested=true;
    if(v.stage<4)self->server.getLogger().warning("Held storage ticket {} refused inventory input after projection stage {}.",v.id,v.stage);
    return;
   }
   if(packet==36){
    event.setCancelled(true);const auto action=held::player_action({reinterpret_cast<const uint8_t*>(data.data()),data.size()});
    if(v.stage==0&&!v.closing&&held::opening_action(action)){v.quiet.observed(milliseconds());return;}
    v.close_requested=true;v.result=VCF_DENIED;
    if(v.stage<4)self->server.getLogger().warning("Held storage ticket {} refused player action {} at preparation stage {}.",v.id,action,v.stage);
    return;
   }
   if(packet==30||packet==31||packet==32||packet==36||packet==48){event.setCancelled(true);const bool echo=packet==31&&data.size()>=3&&static_cast<uint8_t>(data[data.size()-3])==v.source&&static_cast<uint8_t>(data[data.size()-2])==v.source&&data.back()==0;if(!echo){v.close_requested=true;v.result=VCF_DENIED;if(v.stage<4)self->server.getLogger().warning("Held storage ticket {} refused input packet {} at preparation stage {}.",v.id,packet,v.stage);}return;}
  }catch(...){event.setCancelled(true);v.result=VCF_INVALID;v.close_requested=true;}
  return;
 }
}
void HeldStorage::sent(endstone::PacketSendEvent& event){
 auto self=impl_;auto*p=event.getPlayer();if(!p||event.isCancelled())return;const auto identity=p->getUniqueId().str();const auto packet=event.getPacketId();const auto& payload=event.getPayload();
 if(packet==46&&!payload.empty())self->retired.observed_open(identity,static_cast<uint8_t>(payload[0]));
 for(const auto&[_,view]:self->views){auto& v=*view;if(v.player!=identity||v.sending)continue;
  if(packet==46||packet==100){v.superseded=true;return;}
  if(v.closing)return;
  try{if(packet==49){const auto data=wire::decode_content({reinterpret_cast<const uint8_t*>(payload.data()),payload.size()});if(data.window==0)event.setCancelled(true);}
   else if(packet==50){const auto data=wire::decode_slot({reinterpret_cast<const uint8_t*>(payload.data()),payload.size()});if(data.window==0)event.setCancelled(true);}
  }catch(...){event.setCancelled(true);v.result=VCF_INVALID;v.close_requested=true;}return;
 }
}
void HeldStorage::shutdown(){auto self=impl_;self->stopped=true;const auto views=self->views;for(const auto&[_,v]:views)try{self->close(*v,VCF_CLOSED);self->retire(*v);}catch(...){}self->views.clear();}
}
