#include "saved_item.hpp"
#include "held_input.hpp"
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
namespace saved=inventory::saved;
using Bytes=saved::Bytes;
void check(bool ok){if(!ok)throw std::runtime_error("saved-item check failed");}
template<class F>void refused(F f){try{f();}catch(const Error&){return;}throw std::runtime_error("expected saved-item refusal");}
void number(Bytes& out,uint64_t n,unsigned size){for(unsigned i=0;i<size;++i)out.push_back(static_cast<uint8_t>(n>>(8*i)));}
void text(Bytes& out,std::string_view s){number(out,s.size(),2);out.insert(out.end(),s.begin(),s.end());}
void field(Bytes& out,uint8_t type,std::string_view key,const Bytes& payload){number(out,type,1);text(out,key);out.insert(out.end(),payload.begin(),payload.end());}
Bytes string(std::string_view s){Bytes b;text(b,s);return b;}
Bytes item(std::string_view id,uint8_t count,Bytes tag={}){
 Bytes b{10,0,0};field(b,1,"Count",{count});field(b,2,"Damage",{0,0});field(b,8,"Name",string(id));field(b,1,"WasPickedUp",{0});if(!tag.empty())field(b,10,"tag",tag);b.push_back(0);return b;
}
uint32_t limit(std::string_view id){return id=="minecraft:stone"?64:(saved::shulker(id)||id=="minecraft:shield"?1:0);}
storage::Reference player(uint8_t slot,int32_t id){return {12,slot,{},id};}
storage::Reference box(uint8_t slot,int32_t id){return {30,slot,{},id};}
storage::Reference cursor(int32_t id){return {59,0,{},id};}
storage::Request move(int32_t id,storage::Reference from,storage::Reference to,uint8_t count){return {id,0,{{storage::Kind::take,from,to,count,false}}};}
int main(){try{
 Bytes metadata;field(metadata,9,"TypedEmpty",{4,0,0,0,0});field(metadata,6,"negative_zero",{0,0,0,0,0,0,0,128});field(metadata,8,"owner_custom",string("kept"));metadata.push_back(0);
 auto stones=item("minecraft:stone",6,metadata);auto value=saved::decode(stones,limit);check(saved::encode(value)==stones&&value.count==6);
 auto split=value;split.count=2;check(value.matches(split)&&saved::decode(saved::encode(split),limit)==split);
 for(size_t n=1;n<stones.size();++n)refused([&]{saved::decode(std::span(stones).first(n),limit);});
 check(saved::encode(saved::decode({},limit)).empty());auto trailing=stones;trailing.push_back(0);refused([&]{saved::decode(trailing,limit);});
 refused([&]{saved::decode(item("minecraft:missing",1),limit);});refused([&]{saved::decode(item("minecraft:stone",65),limit);});
 refused([&]{saved::decode(item("minecraft:stone",0),limit);});refused([&]{saved::decode(item("minecraft:STONE",1),limit);});
 auto backing=item("minecraft:undyed_shulker_box",1,metadata);std::array<Bytes,27> slots{};check(saved::contents(backing)==slots&&saved::with_contents(backing,slots)==backing);
 const std::string uuid="cb44017a-6817-4e27-8a27-aa91e9e5f6cd";auto identified=saved::with_durable_id(backing,uuid);check(saved::durable_id(identified)==uuid&&saved::with_durable_id(identified,uuid)==identified);
 refused([&]{saved::with_durable_id(identified,"ab44017a-6817-4e27-8a27-aa91e9e5f6cd");});refused([&]{saved::with_durable_id(backing,"00000000-0000-0000-0000-000000000000");});
 slots[0]=stones;slots[26]=item("minecraft:shield",1,metadata);auto filled=saved::with_contents(identified,slots);
 check(saved::contents(filled)==slots&&saved::durable_id(filled)==uuid&&saved::with_contents(filled,slots)==filled);
 auto emptied=saved::with_contents(filled,{});check(saved::contents(emptied)==std::array<Bytes,27>{});
 auto nested=slots;nested[1]=backing;refused([&]{saved::with_contents(identified,nested);});
 auto bad_source=item("minecraft:undyed_shulker_box",2);refused([&]{saved::contents(bad_source);});
 Bytes bad_meta;field(bad_meta,9,"Items",{10,28,0,0,0});bad_meta.push_back(0);refused([&]{saved::contents(item("minecraft:shulker_box",1,bad_meta));});
 inventory::SavedInventory image{};image[0]=identified;image[7]=stones;image[15]=item("minecraft:shield",1,metadata);
 auto before=saved::read_shulker(image,0,17,1000,limit);check(before.player[0].value.policy==0&&before.storage.size()==27);
 auto req=move(-1,player(7,before.player[7].identity),box(26,0),2);auto after=storage::plan(before,storage::Layout("shulker"),req,17,0);
 auto next=saved::project_shulker(image,0,before,after,limit);check(saved::decode(next[7],limit).count==4&&saved::decode(saved::contents(next[0])[26],limit).count==2);
 check(next[15]==image[15]&&saved::durable_id(next[0])==uuid);check(storage::same_inventory(after,saved::read_shulker(next,0,17,5000,limit)));
 auto second=storage::plan(after,storage::Layout("shulker"),move(-3,box(26,after.storage[26].identity),player(7,after.player[7].identity),2),17,after.revision);
 auto restored=saved::project_shulker(next,0,after,second,limit);check(restored[7]==image[7]&&saved::contents(restored[0])==std::array<Bytes,27>{});
 refused([&]{storage::plan(before,storage::Layout("shulker"),move(-3,player(0,before.player[0].identity),box(0,0),1),17,0);});
 refused([&]{saved::project_shulker(image,0,after,second,limit);});
 auto forged=after;forged.player[0].value.policy=3;refused([&]{saved::project_shulker(image,0,before,forged,limit);});
 forged=after;++forged.storage[26].value.item.count;refused([&]{saved::project_shulker(image,0,before,forged,limit);});
 unsigned writes=0;storage::Reservation reserve(before,storage::Layout("shulker"),[&]{return saved::read_shulker(image,0,17,1,limit);},[&](const auto& a,const auto& b){image=saved::project_shulker(image,0,a,b,limit);++writes;return VCF_OK;},[]{});
 auto pick=move(-1,player(7,before.player[7].identity),cursor(0),6);check(!reserve.apply(pick,17,0).committed&&writes==0);
 reserve.cancel();check(writes==0&&image[7]==stones&&reserve.state().cursor.value.item.empty());
 storage::Reservation finish(before,storage::Layout("shulker"),[&]{return saved::read_shulker(image,0,17,1,limit);},[&](const auto& a,const auto& b){image=saved::project_shulker(image,0,a,b,limit);++writes;return VCF_OK;},[]{});
 finish.apply(pick,17,0);auto place=move(-3,cursor(finish.state().cursor.identity),box(0,0),6);check(finish.apply(place,17,1).committed&&writes==1);
 check(finish.apply(place,17,2).replayed&&writes==1&&image[7].empty()&&saved::contents(image[0])[0]==stones);
 // Detached source changes, including same contents with different metadata,
 // refuse instead of overriding an intervening inventory owner.
 auto changed=image;changed[15]=item("minecraft:stone",1);refused([&]{saved::project_shulker(changed,0,before,after,limit);});
 for(auto name:{"minecraft:red_shulker_box","minecraft:light_gray_shulker_box","minecraft:shulker_box"})check(saved::shulker(name));
 check(!saved::shulker("minecraft:bogus_shulker_box")&&!saved::shulker("minecraft:bundle"));
 Bytes movement(32,0);movement.insert(movement.end(),{1,0,0,0,0});movement.insert(movement.end(),8,0);movement.push_back(0);movement.insert(movement.end(),12,0);movement.insert(movement.end(),{1,0,1,0,1,0});
 check(!held::inventory_input(movement));for(size_t n=0;n<movement.size();++n)refused([&]{held::inventory_input(std::span(movement).first(n));});
 for(size_t i=movement.size()-5;i<movement.size();i+=2){auto action=movement;action[i]=1;check(held::inventory_input(action));}
 auto action=movement;action[33]=1;action.insert(action.begin()+34,68);check(held::inventory_input(action));
 action=movement;action[32]=2;refused([&]{held::inventory_input(action);});
 held::InputQuiet quiet(100);check(!quiet.ready(449)&&quiet.ready(450));quiet.observed(420);check(!quiet.ready(769)&&quiet.ready(770));
 refused([&]{quiet.observed(419);});refused([&]{quiet.ready(419);});
 Bytes stop_use{1,58,0,0,0,0,0,0,1};check(held::player_action(stop_use)==29&&held::opening_action(29));
 for(size_t n=0;n<stop_use.size();++n)refused([&]{held::player_action(std::span(stop_use).first(n));});
 stop_use.push_back(0);refused([&]{held::player_action(stop_use);});
 check(held::opening_action(11)&&held::opening_action(12)&&held::opening_action(28));
 for(int32_t action:{-1,0,4,7,14,30,37})check(!held::opening_action(action));
 std::cout<<"Complete saved-item shulker model, metadata, identity, cursor and multi-commit checks passed\n";
}catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 1;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
