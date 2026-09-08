#include <oni/vcf/storage.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace oni::vcf;
namespace s=oni::vcf::storage;
namespace {
int checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("storage assertion failed");}
template<class F>void refused(F f,vcf_status expected=-1){
    try{f();throw std::runtime_error("expected refusal");}
    catch(const Error& e){check(expected<0||e.status==expected);}
}
std::vector<uint8_t> hex(std::string text){
    std::vector<uint8_t> bytes;std::string digits;
    for(char c:text)if(c!=' ')digits+=c;
    for(size_t i=0;i<digits.size();i+=2)bytes.push_back(static_cast<uint8_t>(std::stoul(digits.substr(i,2),nullptr,16)));
    return bytes;
}
s::State initial(uint32_t count=1){
    s::State value;value.generation=917;value.next_identity=22;value.storage.resize(27);
    value.player[1]={{{"minecraft:stone",count,64,{10,0,0,1,1,0,'x',7,0}},3},21};return value;
}
s::Reference ref(uint8_t role,uint8_t slot,int32_t identity){return {role,slot,std::nullopt,identity};}
s::Request move(int32_t id,s::Reference from,s::Reference to,uint8_t count=1){return {id,-1,{{s::Kind::place,from,to,count,false}}};}
void captures(){
    std::ifstream stream(std::string(VCF_FIXTURES)+"/helper-ender-2169.json");
    auto data=nlohmann::json::parse(stream);std::vector<s::Request> requests;
    for(const auto& packet:data.at("packets"))if(packet.at("packet_id")==148){
        auto bytes=hex(packet.at("payload_hex"));auto parsed=s::decode_responses(bytes);check(parsed.size()==1);
        check(s::encode_responses(parsed)==bytes);
        for(size_t n=0;n<bytes.size();++n)refused([&]{s::decode_responses(std::span(bytes).first(n));});
        bytes.push_back(0);refused([&]{s::decode_responses(bytes);});
    }else if(packet.at("packet_id")==147){
        auto bytes=hex(packet.at("payload_hex"));auto decoded=s::decode(bytes);check(decoded.size()==1);
        requests.push_back(decoded.front());
        for(size_t n=0;n<bytes.size();++n)refused([&]{s::decode(std::span(bytes).first(n));});
        bytes.push_back(0);refused([&]{s::decode(bytes);});
    }
    check(requests.size()==4&&requests[0].id==-237&&requests[0].actions[0].source.network_id==21);
    auto actual=initial();auto before=actual;int writes=0;
    s::Reservation reservation(actual,s::Layout("enderchest"),[&]{return actual;},[&](const auto& expected,const auto& next){
        check(s::same_inventory(expected,actual));++writes;actual=next;return VCF_OK;
    });
    auto a=reservation.apply(requests[0],917,0);check(a.committed&&writes==1&&actual.storage[0].value.item.count==1);
    auto replay=reservation.apply(requests[0],917,0);check(replay.replayed&&writes==1);
    auto altered=requests[0];altered.actions[0].count=2;refused([&]{reservation.apply(altered,917,1);},VCF_STALE);
    a=reservation.apply(requests[1],917,1);check(!a.committed&&writes==1&&reservation.state().cursor.value.item.count==1);
    check(actual.storage[0].value.item.count==1&&actual.cursor.value.item.empty());
    a=reservation.apply(requests[2],917,2);check(a.committed&&writes==2&&s::same_inventory(actual,before));
    // The captured failed request references another stack identity (13).
    refused([&]{reservation.apply(requests[3],917,3);},VCF_STALE);check(writes==2);
}
void lifecycle(){
    auto actual=initial(12);auto before=actual;int writes=0;
    s::Reservation reservation(actual,s::Layout("chest"),[&]{return actual;},[&](auto&,auto& next){actual=next;++writes;return VCF_OK;});
    auto take=move(-1,ref(28,1,21),ref(59,0,0),5);
    refused([&]{reservation.apply(take,99,0);},VCF_STALE);
    reservation.apply(take,917,0);check(writes==0&&reservation.state().cursor.value.item.count==5);
    reservation.cancel();check(reservation.closed()&&s::same_inventory(actual,before)&&reservation.state().cursor.value.item.empty());
    refused([&]{reservation.apply(take,917,0);},VCF_CLOSED);
    auto send=move(-3,ref(28,1,21),ref(7,0,0),5);
    s::Reservation conflict(actual,s::Layout("chest"),[&]{return actual;},[](auto&,auto&){return VCF_CONFLICT;});
    refused([&]{conflict.apply(send,917,0);},VCF_CONFLICT);check(!conflict.quarantined()&&s::same_inventory(actual,before));
    bool unreadable=false;
    s::Reservation uncertain(actual,s::Layout("chest"),[&]{if(unreadable)throw std::runtime_error("post-write read failure");return actual;},[&](auto&,auto& next){actual=next;unreadable=true;return VCF_OK;});
    refused([&]{uncertain.apply(send,917,0);},VCF_QUARANTINED);check(uncertain.closed()&&uncertain.quarantined());
    uncertain.cancel();refused([&]{uncertain.apply(send,917,0);},VCF_QUARANTINED);
    check(actual.storage[0].value.item.count==5); // No compensating overwrite/mint.
}
void policies_and_batches(){
    auto before=initial(12);auto move1=move(-1,ref(28,1,21),ref(7,0,0),5);
    auto batch=move1;batch.actions.push_back(move1.actions[0]);batch.actions[1].count=64;
    refused([&]{s::plan(before,s::Layout("chest"),batch,917,0);},VCF_CONFLICT);check(before.player[1].value.item.count==12);
    before.storage[0].value.policy=VCF_PREVIEW;refused([&]{s::plan(before,s::Layout("chest"),move1,917,0);},VCF_DENIED);
    for(auto name:{"chest","doublechest","hopper","dispenser","barrel","dropper","trappedchest","enderchest","shulker"}){
        s::Layout layout(name);auto role=uint8_t(std::string_view(name)=="shulker"?30:7);
        check(layout.resolve(ref(role,static_cast<uint8_t>(layout.capacity()-1),0)).area==s::Area::storage);
        refused([&]{layout.resolve(ref(role,static_cast<uint8_t>(layout.capacity()),0));});
        auto dynamic=ref(role,0,0);dynamic.dynamic_id=42;refused([&]{layout.resolve(dynamic);},VCF_DENIED);
    }
    refused([]{s::Layout("furnace");},VCF_UNAVAILABLE);
    refused([]{s::Layout("bundle");},VCF_UNAVAILABLE);
}
void conservation(){
    auto state=initial(64);state.player[1].value.item.nbt.clear();state.storage[0]={{{"minecraft:stone",64,64,{}},3},22};state.next_identity=23;
    s::Layout layout("chest");std::mt19937 random(2169);
    auto wire=[&](uint32_t slot){
        if(slot<36)return ref(12,static_cast<uint8_t>(slot),state.player[slot].identity);
        if(slot<63)return ref(7,static_cast<uint8_t>(slot-36),state.storage[slot-36].identity);
        return ref(59,0,state.cursor.identity);
    };
    for(int i=0;i<10000;++i){
        auto request=move(-1,wire(static_cast<uint32_t>(random()%64)),wire(static_cast<uint32_t>(random()%64)),static_cast<uint8_t>(random()%65));
        if(random()%3==0)request.actions[0].kind=s::Kind::swap;
        try{state=s::plan(state,layout,request,917,state.revision);}catch(const Error&){}
        uint32_t total=state.cursor.value.item.count;for(const auto& slot:state.player)total+=slot.value.item.count;
        for(const auto& slot:state.storage)total+=slot.value.item.count;check(total==128);s::validate(state,layout);
    }
}
}
int main(){try{captures();lifecycle();policies_and_batches();conservation();std::cout<<checks<<" storage checks passed\n";}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
