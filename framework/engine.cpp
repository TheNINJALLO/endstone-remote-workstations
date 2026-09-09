#include <oni/vcf/core.hpp>
#include <limits>
namespace oni::vcf {
namespace {
std::string str(vcf_string s,uint32_t maximum=1024) {
 require(s.length<=maximum && (s.data || !s.length));
 if(!s.length)return {};
 std::string r(s.data,s.length);require(r.find('\0')==std::string::npos);return r;
}
bool name_ok(std::string_view s) {
 return !s.empty() && s.size()<=64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.';});
}
}
Engine::Engine(Host host):host_(std::move(host)),thread_(std::this_thread::get_id()){}
vcf_status Engine::guard()const {
 if(std::this_thread::get_id()!=thread_)return VCF_WRONG_THREAD;
 if(!alive_)return VCF_CLOSED;
 return VCF_OK;
}
void Engine::check()const {require(guard()==VCF_OK,guard());}
vcf_handle Engine::consumer(std::string name,uint32_t version) {
 check();require(name_ok(name));require(host_.consumer_allowed && host_.consumer_allowed(name),VCF_DENIED);
 require(consumers_.size()<128,VCF_CAPACITY);
 for(const auto&[id,c]:consumers_)require(c.name!=name,VCF_CONFLICT);
 require(version==VCF_ABI_VERSION||version==VCF_ABI_VERSION_1_1||version==VCF_ABI_VERSION_1_0,VCF_VERSION);
 auto id=next_++;consumers_.emplace(id,Consumer{std::move(name),version});return id;
}
std::string Engine::qualify(vcf_handle owner,std::string_view action)const {
 auto c=consumers_.find(owner);require(c!=consumers_.end(),VCF_NOT_FOUND);require(!c->second.revoked,VCF_CLOSED);
 if(action.find(':')!=std::string_view::npos) return std::string(action);
 return c->second.name+":"+std::string(action);
}
void Engine::release(vcf_handle id){
 check();require(!callbacks_&&!dispatching_,VCF_REENTRANT);require(consumers_.contains(id),VCF_NOT_FOUND);
 if(std::erase_if(actions_,[id](const auto&v){return v.second.owner==id;}))++action_revision_;
 std::erase_if(guards_,[id](const auto&v){return v.second.owner==id;});
 for(auto&[key,s]:sessions_)if(s.owner==id){
  s.callback=nullptr;s.context=nullptr;
  if(s.host_started && s.state!=VCF_TERMINAL && host_.close){
   // Native close can synchronously dispatch Endstone events. Defer any
   // resulting owner revocation until this borrowed session storage is free.
   ++callbacks_;try{host_.close(s);}catch(...){}--callbacks_;
  }
  s.state=VCF_TERMINAL;s.result=VCF_CLOSED;
 }
 std::erase_if(queue_,[&](const auto&t){auto it=sessions_.find(t.session);return it==sessions_.end()||it->second.owner==id;});
 std::erase_if(sessions_,[id](const auto&v){return v.second.owner==id;});
 consumers_.erase(id);
 revoked_.erase(id);
}
void Engine::release_named(std::string_view name){
 check();for(auto&[id,c]:consumers_)if(c.name==name){
  // Endstone may disable a consumer from inside another callback. Revoke all
  // callable pointers immediately, but retain borrowed session storage until
  // dispatch unwinds. No exception crosses the plugin-disable event boundary.
  c.revoked=true;revoked_.insert(id);
  if(std::erase_if(actions_,[id](const auto&v){return v.second.owner==id;}))++action_revision_;
  std::erase_if(guards_,[id](const auto&v){return v.second.owner==id;});
  for(auto&[key,s]:sessions_)if(s.owner==id){s.callback=nullptr;s.context=nullptr;}
  if(!callbacks_&&!dispatching_)drain_revoked();
  return;
 }
}
void Engine::drain_revoked(){
 while(!revoked_.empty())release(*revoked_.begin());
}
void Engine::action(vcf_handle owner,std::string name,std::string permission,bool exported,vcf_callback callback,void*context) {
 check();require(name_ok(name) && callback);auto key=qualify(owner,name);
 require(actions_.size()<1024,VCF_CAPACITY);require(!actions_.contains(key),VCF_CONFLICT);
 actions_.emplace(key,Action{owner,std::move(permission),exported,callback,context});
 ++action_revision_;
}
void Engine::remove_action(vcf_handle owner,std::string_view name){
 check();auto key=qualify(owner,name);auto it=actions_.find(key);require(it!=actions_.end(),VCF_NOT_FOUND);require(it->second.owner==owner,VCF_DENIED);actions_.erase(it);++action_revision_;
}
std::vector<ActionInfo> Engine::actions(vcf_handle owner) const {
 check();qualify(owner,"");std::vector<ActionInfo> result;
 for(const auto&[name,a]:actions_)if(a.owner==owner||a.exported){
  auto c=consumers_.find(a.owner);
  if(c!=consumers_.end()&&!c->second.revoked&&host_.consumer_allowed(c->second.name))
   result.push_back({name,c->second.name,a.permission,a.exported,action_revision_});
 }
 return result;
}
vcf_handle Engine::add_guard(vcf_handle owner,std::string name,std::string kind,vcf_guard_callback callback,void* context){
 check();require(name_ok(name)&&callback);auto qualified=qualify(owner,name);
 if(!kind.empty()){auto* row=resolve(kind);require(row,VCF_NOT_FOUND);kind=std::string(row->id);}
 require(guards_.size()<64,VCF_CAPACITY);
 for(const auto&[id,g]:guards_)require(g.name!=qualified,VCF_CONFLICT);
 auto id=next_++;guards_.emplace(id,Guard{owner,std::move(qualified),std::move(kind),callback,context});return id;
}
void Engine::remove_guard(vcf_handle owner,vcf_handle id){
 check();qualify(owner,"");auto it=guards_.find(id);require(it!=guards_.end(),VCF_NOT_FOUND);require(it->second.owner==owner,VCF_DENIED);guards_.erase(it);
}
void Engine::authorize(vcf_handle owner,vcf_handle id,uint32_t phase){
 authorize_at(owner,id,phase,nullptr);
}
void Engine::authorize_block(vcf_handle owner,vcf_handle id,uint32_t phase,int32_t x,int32_t y,int32_t z){
 const auto& s=session(owner,id);const auto* row=resolve(s.kind);
 require(!s.is_menu&&row&&row->source_kind=="block"&&s.mode==VCF_REAL_SOURCE
  &&!s.dimension.empty()&&s.entity_id.empty()&&s.held_id.empty(),VCF_INVALID);
 require(x>=-30000000&&x<=30000000&&z>=-30000000&&z<=30000000&&y>=-64&&y<=319);
 const int32_t position[]={x,y,z};authorize_at(owner,id,phase,position);
}
void Engine::authorize_at(vcf_handle owner,vcf_handle id,uint32_t phase,const int32_t* position){
 auto& s=session(owner,id);require(phase>=VCF_GUARD_DISPATCH&&phase<=VCF_GUARD_ACTIVE);
 require(host_.consumer_allowed(consumers_.at(owner).name),VCF_CLOSED);
 const auto* row=resolve(s.kind);
 require(host_.permission&&host_.permission(s.player,s.is_menu?s.permission:std::string(row->permission)),VCF_DENIED);
 if(!s.permission.empty())require(host_.permission(s.player,s.permission),VCF_DENIED);
 auto view=[](const std::string& v)->vcf_string{return {v.data(),static_cast<uint32_t>(v.size())};};
 vcf_guard_event event{};event.size=sizeof(event);event.version=VCF_ABI_VERSION;
 event.ticket=id;event.requesting_consumer=owner;event.phase=phase;
 auto& request=event.request;request.size=sizeof(request);request.version=VCF_ABI_VERSION;
 request.canonical_id=view(s.kind);request.player=view(s.player);request.title=view(s.title);
 request.source_permission=view(s.permission);request.mode=s.mode;request.dimension=view(s.dimension);
 request.x=s.x;request.y=s.y;request.z=s.z;request.entity_id=view(s.entity_id);request.held_id=view(s.held_id);
 if(position){request.x=position[0];request.y=position[1];request.z=position[2];}
 // A callback may revoke another guard. Copy stable IDs, then re-resolve each
 // live registration before calling; no stale function/context survives unload.
 std::vector<vcf_handle> ids;for(const auto&[key,g]:guards_)ids.push_back(key);
 for(auto key:ids){
  auto it=guards_.find(key);if(it==guards_.end())continue;auto guard=it->second;
  if(!guard.kind.empty()&&guard.kind!=s.kind)continue;
  if(!consumers_.contains(guard.owner)||consumers_.at(guard.owner).revoked||!host_.consumer_allowed(consumers_.at(guard.owner).name))continue;
  event.version=request.version=consumers_.at(guard.owner).version;
  ++callbacks_;vcf_status result;
  try{result=guard.callback(guard.context,&event);}catch(...){result=VCF_DENIED;}
  --callbacks_;require(result==VCF_OK,VCF_DENIED);
  require(!consumers_.at(owner).revoked,VCF_CLOSED);
  require(s.state!=VCF_CLOSING&&s.state!=VCF_TERMINAL,VCF_CLOSED);
 }
}
vcf_held_info Engine::inspect_held(vcf_handle owner,std::string_view player){
 check();qualify(owner,"");require(!player.empty()&&player.size()<=128&&player.find('\0')==std::string_view::npos);
 require(host_.consumer_allowed(consumers_.at(owner).name),VCF_CLOSED);
 require(host_.permission&&host_.permission(player,"remoteworkstations.use"),VCF_DENIED);
 require(bool(host_.inspect_held),VCF_UNAVAILABLE);
 auto result=host_.inspect_held(player);
 const auto length=std::find(std::begin(result.canonical_id),std::end(result.canonical_id),'\0');
 require(length!=std::end(result.canonical_id),VCF_INTERNAL);
 const auto* row=resolve(std::string_view(result.canonical_id,static_cast<size_t>(length-std::begin(result.canonical_id))));
 require(row&&row->source_kind=="held-item",VCF_UNAVAILABLE);
 require(host_.permission(player,row->permission),VCF_DENIED);
 require(row->id!="bundle",VCF_UNAVAILABLE);
 qualify(owner,"");require(host_.consumer_allowed(consumers_.at(owner).name),VCF_CLOSED);
 // Inspection cannot qualify or unlock a native held-item editor.
 result.native_open_available=0;return result;
}
uint32_t Engine::collect_terminal(vcf_handle owner,uint32_t limit){
 check();require(!callbacks_&&!dispatching_,VCF_REENTRANT);qualify(owner,"");uint32_t n=0;
 for(auto it=sessions_.begin();it!=sessions_.end()&&n<std::min(limit,256u);){
  if(it->second.owner==owner&&it->second.state==VCF_TERMINAL){it=sessions_.erase(it);++n;}else ++it;
 }
 return n;
}
Session& Engine::session(vcf_handle owner,vcf_handle id){
 check();require(consumers_.contains(owner),VCF_NOT_FOUND);require(!consumers_.at(owner).revoked,VCF_CLOSED);auto it=sessions_.find(id);require(it!=sessions_.end(),VCF_NOT_FOUND);require(it->second.owner==owner,VCF_DENIED);return it->second;
}
vcf_handle Engine::prepare(vcf_handle owner,Session s){
 check();require(consumers_.contains(owner),VCF_NOT_FOUND);require(!consumers_.at(owner).revoked,VCF_CLOSED);require(sessions_.size()<2048,VCF_CAPACITY);
 require(!s.player.empty() && s.player.size()<=128 && s.mode<=VCF_CUSTOM_REPLACEMENT);
 const auto* row=resolve(s.kind);require(s.is_menu||row,VCF_NOT_FOUND);
 if(row){s.kind=std::string(row->id);s.inventory.slots.resize(row->capacity);}
 s.owner=owner;s.id=next_++;s.generation=s.id;sessions_.emplace(s.id,s);return s.id;
}
Item Engine::item(const vcf_item& in)const{
 check();require(in.size>=sizeof(vcf_item)&&(in.version==VCF_ABI_VERSION||in.version==VCF_ABI_VERSION_1_1||in.version==VCF_ABI_VERSION_1_0));
 Item i;i.id=str(in.identifier);i.count=in.count;
 if(!i.count){require(i.id.empty() && !in.nbt.length);return i;}
 require(host_.item_limit!=nullptr,VCF_UNAVAILABLE);i.limit=host_.item_limit(i.id);
 require(i.limit && i.count<=i.limit && i.limit<=255,VCF_INVALID);
 require(in.nbt.length<=1024*1024 && (in.nbt.data||!in.nbt.length));
 if(in.nbt.length)i.nbt.assign(in.nbt.data,in.nbt.data+in.nbt.length);
 validate_nbt(i.nbt);return i;
}
void Engine::preload(vcf_handle owner,vcf_handle id,uint32_t slot,Item i,uint32_t policy){
 auto&s=session(owner,id);require(s.state==VCF_PREPARING,VCF_CONFLICT);
 require(s.mode!=VCF_REAL_SOURCE,VCF_DENIED);require(slot<s.inventory.slots.size() && policy<=15);
 require(!((policy&VCF_PREVIEW)&&(policy&(VCF_INSERT|VCF_EXTRACT|VCF_RESULT))),VCF_INVALID);
 size_t bytes=0;for(const auto&v:s.inventory.slots)bytes+=v.item.nbt.size();
 require(bytes-s.inventory.slots[slot].item.nbt.size()+i.nbt.size()<=65536,VCF_CAPACITY);
 s.inventory.slots[slot]={std::move(i),policy};
}
void Engine::rule(vcf_handle owner,vcf_handle id,Rule r){
 auto&s=session(owner,id);require(s.state==VCF_PREPARING && s.mode!=VCF_REAL_SOURCE,VCF_DENIED);
 require(name_ok(r.id) && r.stock && r.revision && r.costs.size()<=16 && !r.costs.empty());
 require(r.output_slot<s.inventory.slots.size() && !r.output.empty());
 require(s.rules.size()<128 && !s.rules.contains(r.id),VCF_CAPACITY);
 std::set<uint32_t> seen;for(const auto&[slot,c]:r.costs)require(slot<s.inventory.slots.size() && slot!=r.output_slot && c.count && seen.insert(slot).second);
 s.rules.emplace(r.id,std::move(r));
}
void Engine::open(vcf_handle owner,vcf_handle id){
 auto&s=session(owner,id);require(s.state==VCF_PREPARING,VCF_CONFLICT);require(queue_.size()<4096,VCF_CAPACITY);
 s.state=VCF_OPENING;queue_.push_back({TaskType::open,id,{}});
}
void Engine::close(vcf_handle owner,vcf_handle id){
 auto&s=session(owner,id);if(s.state==VCF_TERMINAL||s.state==VCF_CLOSING)return;
 require(queue_.size()<4096,VCF_CAPACITY);s.state=VCF_CLOSING;queue_.push_back({TaskType::close,id,{}});
}
void Engine::forget(vcf_handle owner,vcf_handle id){
 auto&s=session(owner,id);require(!callbacks_,VCF_REENTRANT);require(s.state==VCF_TERMINAL,VCF_CONFLICT);sessions_.erase(id);
}
void Engine::emit(Session&s,uint32_t kind,std::string_view detail){
 if(!s.callback||!consumers_.contains(s.owner)||consumers_.at(s.owner).revoked)return;
 if(!host_.consumer_allowed(consumers_.at(s.owner).name))return;
 vcf_event e{sizeof(e),consumers_.at(s.owner).version,s.id,{s.player.data(),static_cast<uint32_t>(s.player.size())},kind,s.result,s.inventory.revision,{detail.data(),static_cast<uint32_t>(detail.size())}};
 ++callbacks_;try{s.callback(s.context,&e);}catch(...){s.result=VCF_INTERNAL;}--callbacks_;
}
void Engine::finish(Session&s,vcf_status result){if(s.state==VCF_TERMINAL)return;s.state=VCF_TERMINAL;s.result=result;emit(s,result==VCF_OK||result==VCF_CLOSED?VCF_EVENT_CLOSE:VCF_EVENT_FAILURE);}
vcf_handle Engine::invoke(vcf_handle owner,std::string player,std::string action){
 check();auto key=qualify(owner,action);auto it=actions_.find(key);require(it!=actions_.end(),VCF_NOT_FOUND);
 require(it->second.owner==owner||it->second.exported,VCF_DENIED);require(queue_.size()<4096,VCF_CAPACITY);
 Session s;s.player=std::move(player);s.is_menu=true;s.title="Action";auto id=prepare(owner,std::move(s));sessions_.at(id).state=VCF_OPENING;queue_.push_back({TaskType::invoke,id,std::move(key)});return id;
}
void Engine::tick(uint32_t budget){
 check();require(!callbacks_&&!dispatching_,VCF_REENTRANT);drain_revoked();
 dispatching_=true;
 struct Reset { bool& value; ~Reset(){value=false;} } reset{dispatching_};
 auto count=std::min<size_t>(queue_.size(),std::min<uint32_t>(budget,256));
 for(size_t n=0;n<count && !queue_.empty();++n){
  auto task=std::move(queue_.front());queue_.pop_front();auto it=sessions_.find(task.session);if(it==sessions_.end())continue;
  auto&s=it->second;
  if(task.type==TaskType::close){
   if(s.state==VCF_CLOSING){try{auto result=s.host_started&&host_.close?host_.close(s):VCF_OK;if(result!=VCF_PENDING)finish(s,s.result!=VCF_OK?s.result:result==VCF_OK?VCF_CLOSED:result);}catch(...){finish(s,VCF_QUARANTINED);}}
   continue;
  }
  if(s.state!=VCF_OPENING)continue;
  try{
   require(consumers_.contains(s.owner)&&!consumers_.at(s.owner).revoked,VCF_CLOSED);
   require(host_.consumer_allowed(consumers_.at(s.owner).name),VCF_CLOSED);
   if(task.type==TaskType::invoke){
    if(!s.buttons.empty())authorize(s.owner,s.id,VCF_GUARD_DISPATCH);
    auto a=actions_.find(task.action);require(a!=actions_.end(),VCF_NOT_FOUND);auto copy=a->second;
    require(copy.owner==s.owner||copy.exported,VCF_DENIED);
    require(consumers_.contains(copy.owner)&&!consumers_.at(copy.owner).revoked&&host_.consumer_allowed(consumers_.at(copy.owner).name),VCF_CLOSED);
    require(host_.permission && host_.permission(s.player,copy.permission),VCF_DENIED);
    vcf_event e{sizeof(e),consumers_.at(copy.owner).version,s.id,{s.player.data(),static_cast<uint32_t>(s.player.size())},VCF_EVENT_ACTION,VCF_OK,0,{task.action.data(),static_cast<uint32_t>(task.action.size())}};
    ++callbacks_;vcf_status result;try{result=copy.callback(copy.context,&e);}catch(...){result=VCF_INTERNAL;}--callbacks_;
    finish(s,result);continue;
   }
   authorize(s.owner,s.id,VCF_GUARD_DISPATCH);
   for(const auto&[other,t]:sessions_)require(other==s.id||t.player!=s.player||!t.host_started||t.state==VCF_TERMINAL,VCF_CONFLICT);
   require(host_.open!=nullptr,VCF_UNAVAILABLE);s.host_started=true;auto result=host_.open(s);
   if(s.state!=VCF_OPENING||result==VCF_PENDING)continue;require(result==VCF_OK,result);
   require(!consumers_.at(s.owner).revoked,VCF_CLOSED);
   s.state=VCF_ACTIVE;emit(s,VCF_EVENT_OPEN);
  }catch(const Error&e){finish(s,e.status);}catch(...){finish(s,VCF_INTERNAL);}
 }
}
void Engine::selected(vcf_handle id,std::string_view player,uint32_t index){
 check();auto it=sessions_.find(id);require(it!=sessions_.end(),VCF_STALE);auto&s=it->second;
 require(s.is_menu && s.state==VCF_ACTIVE && s.player==player,VCF_STALE);
 require(index<s.buttons.size());authorize(s.owner,s.id,VCF_GUARD_ACTIVE);
 auto key=qualify(s.owner,s.buttons[index].action);auto action=actions_.find(key);
 require(action!=actions_.end(),VCF_NOT_FOUND);
 require(action->second.owner==s.owner||action->second.exported,VCF_DENIED);
 require(queue_.size()<4096,VCF_CAPACITY);
 // The caller already owns this menu ticket. Execute the selected action on
 // that same ticket so its result is observable and no hidden child leaks.
 // Queue allocation must succeed before making the selection irrevocable.
 queue_.push_back({TaskType::invoke,id,std::move(key)});s.state=VCF_OPENING;
}
void Engine::player_gone(std::string_view player){
 check();for(auto&[id,s]:sessions_)if(s.player==player&&s.state!=VCF_TERMINAL)close(s.owner,id);
}
void Engine::opened(vcf_handle owner,vcf_handle id,vcf_status result){
 auto&s=session(owner,id);require(s.state==VCF_OPENING&&s.host_started,VCF_STALE);
 require(result!=VCF_PENDING,VCF_INVALID);
 if(result==VCF_OK){
  try{authorize(owner,id,VCF_GUARD_READY);}
  catch(const Error& e){s.result=e.status;if(!consumers_.at(owner).revoked)close(owner,id);return;}
 }
 if(result==VCF_OK){s.state=VCF_ACTIVE;emit(s,VCF_EVENT_OPEN);}else finish(s,result);
}
void Engine::retired(vcf_handle owner,vcf_handle id,vcf_status result){
 auto&s=session(owner,id);require(s.host_started&&s.state!=VCF_TERMINAL,VCF_STALE);require(result!=VCF_PENDING,VCF_INVALID);finish(s,s.result!=VCF_OK?s.result:result);
}
void Engine::shutdown(){
 check();require(!callbacks_&&!dispatching_,VCF_REENTRANT);
 while(!consumers_.empty())release(consumers_.begin()->first);
 queue_.clear();alive_=false;
}
}
