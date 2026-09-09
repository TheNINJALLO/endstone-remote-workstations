#include <oni/vcf/core.hpp>
#include <cstring>
namespace oni::vcf {
namespace {
Engine* engine=nullptr;
template<class T> void input(const T*p){require(p&&p->size>=sizeof(T));require(p->version==VCF_ABI_VERSION||p->version==VCF_ABI_VERSION_1_3||p->version==VCF_ABI_VERSION_1_2||p->version==VCF_ABI_VERSION_1_1||p->version==VCF_ABI_VERSION_1_0,VCF_VERSION);}
std::string text(vcf_string s,uint32_t max=4096){
 require(s.length<=max && (s.data||!s.length));if(!s.length)return {};
 std::string r(s.data,s.length);require(r.find('\0')==std::string::npos);return r;
}
vcf_string view(std::string_view v){return {v.data(),static_cast<uint32_t>(v.size())};}
template<class F> vcf_status call(F&&f) noexcept {
 try{require(engine,VCF_UNAVAILABLE);require(engine->guard()==VCF_OK,engine->guard());f();return VCF_OK;}
 catch(const Error&e){return e.status;}catch(...){return VCF_INTERNAL;}
}
vcf_status VCF_CALL reg(const vcf_consumer_desc*d,vcf_handle*out){return call([&]{input(d);require(out);*out=engine->consumer(text(d->name),d->version);});}
vcf_status VCF_CALL unreg(vcf_handle h){return call([&]{engine->release(h);});}
vcf_status VCF_CALL count(uint32_t*out){return call([&]{require(out);*out=static_cast<uint32_t>(catalog().size());});}
vcf_status VCF_CALL capability(uint32_t index,vcf_capability*out){
 return call([&]{input(out);require(index<catalog().size(),VCF_NOT_FOUND);const auto&c=catalog()[index];
 const bool available=engine->native_available(c.id);
 *out={sizeof(*out),out->version,view(c.id),view(c.family),view(c.permission),view(c.aliases),view(c.source_kind),available?1u:0u,0,available?VCF_OK:VCF_UNAVAILABLE,view(available?"Experimental original-mode adapter is configured; client qualification is pending.":"Native and custom screen adapters are not qualified or configured for this C++ artifact.")};});
}
vcf_status VCF_CALL lookup(vcf_string name,uint32_t*out){return call([&]{require(out);auto c=resolve(text(name));require(c,VCF_NOT_FOUND);*out=static_cast<uint32_t>(c-catalog().data());});}
vcf_status VCF_CALL action(vcf_handle h,const vcf_action_desc*d){return call([&]{input(d);require(d->exported<=1);engine->action(h,text(d->name),text(d->permission),d->exported!=0,d->callback,d->context);});}
vcf_status VCF_CALL unaction(vcf_handle h,vcf_string name){return call([&]{engine->remove_action(h,text(name));});}
vcf_status VCF_CALL invoke(vcf_handle h,vcf_string player,vcf_string name,vcf_handle*out){return call([&]{require(out);*out=engine->invoke(h,text(player,128),text(name));});}
vcf_status VCF_CALL prepare(vcf_handle h,const vcf_session_desc*d,vcf_handle*out){
 return call([&]{input(d);require(out);Session s;s.kind=text(d->canonical_id);s.player=text(d->player,128);s.title=text(d->title);s.permission=text(d->source_permission);s.mode=d->mode;s.dimension=text(d->dimension);s.x=d->x;s.y=d->y;s.z=d->z;s.entity_id=text(d->entity_id);s.held_id=text(d->held_id);s.callback=d->callback;s.context=d->context;*out=engine->prepare(h,std::move(s));});
}
vcf_status VCF_CALL set(vcf_handle h,vcf_handle s,uint32_t slot,const vcf_item*i,uint32_t policy){
 return call([&]{input(i);engine->preload(h,s,slot,engine->item(*i),policy);});
}
vcf_status VCF_CALL rule(vcf_handle h,vcf_handle s,const vcf_rule_desc*d){
 return call([&]{input(d);require(d->cost_count>0&&d->cost_count<=16&&d->costs);Rule r;r.id=text(d->id);r.revision=d->revision;r.stock=d->stock;r.output_slot=d->output_slot;r.duration_ticks=d->duration_ticks;r.output=engine->item(d->output);
 for(uint32_t i=0;i<d->cost_count;i++){input(d->costs+i);r.costs.emplace_back(d->costs[i].slot,engine->item(d->costs[i].item));}engine->rule(h,s,std::move(r));});
}
vcf_status VCF_CALL open(vcf_handle h,vcf_handle s){return call([&]{engine->open(h,s);});}
vcf_status VCF_CALL close(vcf_handle h,vcf_handle s){return call([&]{engine->close(h,s);});}
vcf_status VCF_CALL info(vcf_handle h,vcf_handle s,vcf_session_info*out){return call([&]{input(out);auto&v=engine->session(h,s);*out={sizeof(*out),out->version,v.state,v.result,v.inventory.revision,v.generation};});}
vcf_status VCF_CALL menu(vcf_handle h,const vcf_menu_desc*d,vcf_handle*out){
 return call([&]{input(d);require(out&&d->button_count<=128&&(d->buttons||!d->button_count));Session s;s.player=text(d->player,128);s.title=text(d->title);s.content=text(d->content,16384);s.permission=text(d->permission);s.is_menu=true;s.callback=d->callback;s.context=d->context;
 for(uint32_t i=0;i<d->button_count;i++){auto&b=d->buttons[i];input(&b);s.buttons.push_back({text(b.label),text(b.action),text(b.icon)});}
 *out=engine->prepare(h,std::move(s));engine->open(h,*out);});
}
vcf_status VCF_CALL forget(vcf_handle h,vcf_handle s){return call([&]{engine->forget(h,s);});}
vcf_status VCF_CALL action_count(vcf_handle h,uint32_t*out,uint64_t*revision){
 return call([&]{require(out&&revision);auto actions=engine->actions(h);*out=static_cast<uint32_t>(actions.size());*revision=engine->action_revision();});
}
vcf_status VCF_CALL action_info(vcf_handle h,uint32_t index,uint64_t revision,vcf_action_info*out,char*storage,uint32_t capacity,uint32_t*required){
 return call([&]{input(out);require(required);require(revision==engine->action_revision(),VCF_STALE);
 auto actions=engine->actions(h);require(index<actions.size(),VCF_NOT_FOUND);const auto& a=actions[index];
 *required=static_cast<uint32_t>(a.name.size()+a.consumer.size()+a.permission.size()+3);
 require(storage&&capacity>=*required,VCF_BUFFER);
 uint32_t offset=0;auto copy=[&](const std::string& value)->vcf_string{
  auto* start=storage+offset;std::memcpy(start,value.c_str(),value.size()+1);offset+=static_cast<uint32_t>(value.size()+1);return {start,static_cast<uint32_t>(value.size())};
 };
 *out={sizeof(*out),out->version,copy(a.name),copy(a.consumer),copy(a.permission),a.exported?1u:0u,a.revision};});
}
vcf_status VCF_CALL register_guard(vcf_handle h,const vcf_guard_desc*d,vcf_handle*out){
 return call([&]{input(d);require(out);*out=engine->add_guard(h,text(d->name),text(d->canonical_id),d->callback,d->context);});
}
vcf_status VCF_CALL unregister_guard(vcf_handle h,vcf_handle id){return call([&]{engine->remove_guard(h,id);});}
vcf_status VCF_CALL inspect_held(vcf_handle h,vcf_string player,vcf_held_info*out){
 return call([&]{input(out);require(out->version==VCF_ABI_VERSION||out->version==VCF_ABI_VERSION_1_3||out->version==VCF_ABI_VERSION_1_2,VCF_VERSION);
 auto result=engine->inspect_held(h,text(player,128));result.size=sizeof(result);result.version=out->version;*out=result;});
}
vcf_status VCF_CALL read_inventory_item(vcf_handle h,vcf_string player,uint32_t slot,vcf_inventory_item_info*out,uint8_t*buffer,uint32_t capacity,uint32_t*required){
 return call([&]{input(out);require(out->version==VCF_ABI_VERSION||out->version==VCF_ABI_VERSION_1_3,VCF_VERSION);require(required&&capacity<=VCF_ITEM_SAVE_MAX_BYTES&&(!capacity||buffer));
 auto result=engine->read_inventory_item(h,text(player,128),slot);const auto length=static_cast<uint32_t>(result.nbt.size());
 if(!buffer||capacity<length){*required=length;throw Error{VCF_BUFFER};}
 result.info.size=sizeof(result.info);result.info.version=out->version;
 std::memcpy(buffer,result.nbt.data(),length);*out=result.info;*required=length;});
}
vcf_status VCF_CALL observe_inventory_item(vcf_handle h,vcf_string player,uint32_t slot,vcf_handle*out){
 return call([&]{require(out);*out=engine->observe_inventory_item(h,text(player,128),slot);});
}
vcf_status VCF_CALL validate_inventory_observation(vcf_handle h,vcf_handle id){return call([&]{engine->validate_inventory_observation(h,id);});}
vcf_status VCF_CALL release_inventory_observation(vcf_handle h,vcf_handle id){return call([&]{engine->release_inventory_observation(h,id);});}
const vcf_api api{sizeof(vcf_api),VCF_ABI_VERSION,reg,unreg,count,capability,lookup,action,unaction,invoke,prepare,set,rule,open,close,info,menu,forget,action_count,action_info,register_guard,unregister_guard,inspect_held,read_inventory_item,observe_inventory_item,validate_inventory_observation,release_inventory_observation};
}
void attach_engine(Engine* e){engine=e;}
}
extern "C" VCF_EXPORT vcf_status VCF_CALL oni_vcf_get_api(uint32_t version,uint32_t size,vcf_api*out) {
 if(version!=VCF_ABI_VERSION&&version!=VCF_ABI_VERSION_1_3&&version!=VCF_ABI_VERSION_1_2&&version!=VCF_ABI_VERSION_1_1&&version!=VCF_ABI_VERSION_1_0)return VCF_VERSION;
 const auto required=version==VCF_ABI_VERSION?static_cast<uint32_t>(sizeof(vcf_api)):version==VCF_ABI_VERSION_1_3?VCF_API_1_3_SIZE:version==VCF_ABI_VERSION_1_2?VCF_API_1_2_SIZE:version==VCF_ABI_VERSION_1_1?VCF_API_1_1_SIZE:VCF_API_1_0_SIZE;
 if(!out||size<required)return VCF_BUFFER;
 auto table=oni::vcf::api;table.size=required;table.version=version;
 std::memcpy(out,&table,required);return VCF_OK;
}
