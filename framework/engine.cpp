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
vcf_handle Engine::consumer(std::string name) {
 check();require(name_ok(name));require(host_.consumer_allowed && host_.consumer_allowed(name),VCF_DENIED);
 require(consumers_.size()<128,VCF_CAPACITY);
 for(const auto&[id,c]:consumers_)require(c.name!=name,VCF_CONFLICT);
 auto id=next_++;consumers_.emplace(id,Consumer{std::move(name)});return id;
}
std::string Engine::qualify(vcf_handle owner,std::string_view action)const {
 auto c=consumers_.find(owner);require(c!=consumers_.end(),VCF_NOT_FOUND);require(!c->second.revoked,VCF_CLOSED);
 if(action.find(':')!=std::string_view::npos) return std::string(action);
 return c->second.name+":"+std::string(action);
}
void Engine::release(vcf_handle id){
 check();require(!callbacks_&&!dispatching_,VCF_REENTRANT);require(consumers_.contains(id),VCF_NOT_FOUND);
 std::erase_if(actions_,[id](const auto&v){return v.second.owner==id;});
 for(auto&[key,s]:sessions_)if(s.owner==id){
  s.callback=nullptr;s.context=nullptr;
  if(s.host_started && s.state!=VCF_TERMINAL && host_.close){try{host_.close(s);}catch(...){}}
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
  std::erase_if(actions_,[id](const auto&v){return v.second.owner==id;});
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
}
void Engine::remove_action(vcf_handle owner,std::string_view name){
 check();auto key=qualify(owner,name);auto it=actions_.find(key);require(it!=actions_.end(),VCF_NOT_FOUND);require(it->second.owner==owner,VCF_DENIED);actions_.erase(it);
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
 check();require(in.size>=sizeof(vcf_item)&&in.version==VCF_ABI_VERSION);
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
 vcf_event e{sizeof(e),VCF_ABI_VERSION,s.id,{s.player.data(),static_cast<uint32_t>(s.player.size())},kind,s.result,s.inventory.revision,{detail.data(),static_cast<uint32_t>(detail.size())}};
 ++callbacks_;try{s.callback(s.context,&e);}catch(...){s.result=VCF_INTERNAL;}--callbacks_;
}
void Engine::finish(Session&s,vcf_status result){s.state=VCF_TERMINAL;s.result=result;emit(s,result==VCF_OK||result==VCF_CLOSED?VCF_EVENT_CLOSE:VCF_EVENT_FAILURE);}
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
    auto a=actions_.find(task.action);require(a!=actions_.end(),VCF_NOT_FOUND);auto copy=a->second;
    require(copy.owner==s.owner||copy.exported,VCF_DENIED);
    require(consumers_.contains(copy.owner)&&!consumers_.at(copy.owner).revoked&&host_.consumer_allowed(consumers_.at(copy.owner).name),VCF_CLOSED);
    require(host_.permission && host_.permission(s.player,copy.permission),VCF_DENIED);
    vcf_event e{sizeof(e),VCF_ABI_VERSION,s.id,{s.player.data(),static_cast<uint32_t>(s.player.size())},VCF_EVENT_ACTION,VCF_OK,0,{task.action.data(),static_cast<uint32_t>(task.action.size())}};
    ++callbacks_;vcf_status result;try{result=copy.callback(copy.context,&e);}catch(...){result=VCF_INTERNAL;}--callbacks_;
    finish(s,result);continue;
   }
   const auto* row=resolve(s.kind);
   require(host_.permission && host_.permission(s.player,s.is_menu?s.permission:std::string(row->permission)),VCF_DENIED);
   if(!s.permission.empty())require(host_.permission(s.player,s.permission),VCF_DENIED);
   for(const auto&[other,t]:sessions_)require(other==s.id||t.player!=s.player||!t.host_started||t.state==VCF_TERMINAL,VCF_CONFLICT);
   require(host_.open!=nullptr,VCF_UNAVAILABLE);s.host_started=true;auto result=host_.open(s);if(result==VCF_PENDING)continue;require(result==VCF_OK,result);
   require(!consumers_.at(s.owner).revoked,VCF_CLOSED);
   s.state=VCF_ACTIVE;emit(s,VCF_EVENT_OPEN);
  }catch(const Error&e){finish(s,e.status);}catch(...){finish(s,VCF_INTERNAL);}
 }
}
void Engine::selected(vcf_handle id,std::string_view player,uint32_t index){
 check();auto it=sessions_.find(id);require(it!=sessions_.end(),VCF_STALE);auto&s=it->second;
 require(s.is_menu && s.state==VCF_ACTIVE && s.player==player,VCF_STALE);
 require(index<s.buttons.size());require(host_.permission(s.player,s.permission),VCF_DENIED);
 // Invalidate before queueing action: double clicks cannot execute twice.
 auto owner=s.owner;auto action=s.buttons[index].action;auto player_copy=s.player;
 finish(s,VCF_OK);invoke(owner,std::move(player_copy),std::move(action));
}
void Engine::player_gone(std::string_view player){
 check();for(auto&[id,s]:sessions_)if(s.player==player&&s.state!=VCF_TERMINAL)close(s.owner,id);
}
void Engine::opened(vcf_handle owner,vcf_handle id,vcf_status result){
 auto&s=session(owner,id);require(s.state==VCF_OPENING&&s.host_started,VCF_STALE);
 require(result!=VCF_PENDING,VCF_INVALID);
 if(result==VCF_OK){
  const auto* row=resolve(s.kind);
  if(!host_.consumer_allowed(consumers_.at(owner).name)||!host_.permission(s.player,s.is_menu?s.permission:std::string(row->permission))||(!s.permission.empty()&&!host_.permission(s.player,s.permission))){
   s.result=VCF_DENIED;close(owner,id);return;
  }
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
