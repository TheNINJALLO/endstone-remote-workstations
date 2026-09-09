#include <oni/vcf/packet_items.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace oni::vcf;
namespace w=oni::vcf::wire;
namespace {
uint32_t checks=0;
void check(bool yes){++checks;if(!yes)throw std::runtime_error("item wire assertion failed");}
template<class F>void refused(F f){try{f();throw std::runtime_error("expected item wire refusal");}catch(const Error&){++checks;}}
std::vector<uint8_t> hex(const std::string& s){
    std::vector<uint8_t> b;for(size_t i=0;i<s.size();i+=2)b.push_back(static_cast<uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return b;
}
std::vector<w::RegistryEntry> registry(){return {{"minecraft:air",0,false,0,{10,0,0}},{"minecraft:stone",1,false,0,{10,0,0}}};}
void conformance(){
    std::ifstream stream(std::string(VCF_FIXTURES)+"/item-wire-conformance-2169.json");auto data=nlohmann::json::parse(stream);
    check(data.at("kind").get<std::string>().starts_with("generated independent"));
    for(const auto& row:data.at("packets")){
        auto bytes=hex(row.at("payload_hex"));auto parse=[&](std::span<const uint8_t> b){
            auto id=row.at("packet_id").get<int>();
            if(id==49)return w::encode(w::decode_content(b));
            if(id==50)return w::encode(w::decode_slot(b));
            return w::encode_registry(w::decode_registry(b));
        };
        check(parse(bytes)==bytes);
        for(size_t n=0;n<bytes.size();++n)refused([&]{parse(std::span(bytes).first(n));});
        bytes.push_back(0);refused([&]{parse(bytes);});
        if(row.at("packet_id")==162){
            bytes.pop_back();auto entries=w::decode_registry(bytes);check(entries.size()==3);
            check(entries[2].numeric_id==-32768&&entries[2].version==-2&&entries[2].component_based);
            check(entries[1].components.size()>100);
        }
    }
}
void malformed(){
    auto entries=registry();entries.push_back(entries[0]);refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].numeric_id=0;refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].identifier="minecraft:bad:alias";refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,3,1,'x',0,3,1,'x',0,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,9,1,'x',0,2,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,7,1,'x',1,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,3,1,'x',0x80,0,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,3,1,0xff,0,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0,6,1,'x',0,0,0,0,0,0,0xf0,0x7f,0};refused([&]{w::encode_registry(entries);});
    entries=registry();entries[1].components={10,0};
    for(int i=0;i<34;++i)entries[1].components.insert(entries[1].components.end(),{10,1,'x'});
    entries[1].components.insert(entries[1].components.end(),35,0);refused([&]{w::encode_registry(entries);});
    auto bytes=w::encode_registry(registry());bytes[0]=0x82;bytes.insert(bytes.begin()+1,0);refused([&]{w::decode_registry(bytes);});
    bytes.assign(8*1024*1024+1,0);refused([&]{w::decode_registry(bytes);});
    bytes.assign(65537,0);refused([&]{w::decode_content(bytes);});refused([&]{w::decode_slot(bytes);});
    w::Content c;c.items.resize(55);refused([&]{w::encode(c);});
    w::Slot s;s.slot=256;refused([&]{w::encode(s);});s.slot=0;s.item.count=256;refused([&]{w::encode(s);});
}
void observation(){
    w::Observation o;w::Content c;c.items.resize(36);c.container={12,{}};
    c.items[2]={1,7,9,101,33,{0xff,0xff,1,10,0,0,0,1,2,3}};
    o.content(c);check(!o.inventory_ready());refused([&]{o.inventory();});
    o.registry(registry());check(o.epoch()==1&&o.registry_ready());check(o.find("minecraft:stone")->numeric_id==1&&!o.find("example:missing"));
    w::Slot slot;slot.slot=2;slot.item=c.items[2];slot.item.count=3;
    o.slot(slot);check(!o.inventory_ready());o.content(c);check(o.inventory_ready()&&o.inventory()[2]==c.items[2]);
    o.slot(slot);check(o.inventory()[2].count==3&&o.inventory()[2].user_data==c.items[2].user_data);
    auto foreign=slot;foreign.window=70;foreign.slot=200;o.slot(foreign);check(o.inventory()[2].count==3);
    auto other=c;other.window=70;other.items.resize(54);o.content(other);check(o.inventory()[2].count==3);
    o.registry(registry());check(o.epoch()==1&&o.inventory_ready());
    auto changed=registry();changed[1].version=1;o.registry(changed);check(o.epoch()==2&&!o.inventory_ready());
    o.content(c);auto bad=c;bad.container.dynamic_id=1;refused([&]{o.content(bad);});check(!o.inventory_ready());
    o.content(c);slot.slot=36;refused([&]{o.slot(slot);});check(!o.inventory_ready());
    o.content(c);bad=c;bad.items.resize(35);refused([&]{o.content(bad);});check(!o.inventory_ready());
    o.content(c);auto wrong=registry();wrong[0].identifier="bad";refused([&]{o.registry(wrong);});check(o.epoch()==2&&o.inventory_ready());
    o.invalidate();check(!o.inventory_ready());
}
void properties(){
    std::mt19937 rng(2169);
    for(int n=0;n<10000;++n){
        w::Slot slot;slot.window=static_cast<uint8_t>(rng());slot.slot=rng()%256;
        if(rng()%2)slot.container=w::Container{static_cast<uint8_t>(rng()),rng()};
        if(rng()%2)slot.storage=w::Descriptor{};
        auto& d=slot.item;d.numeric_id=static_cast<int16_t>(rng());d.count=static_cast<uint16_t>(rng()%256);d.auxiliary=rng();
        if(rng()%2)d.network_id=static_cast<int32_t>(rng());d.block_runtime_id=rng();
        d.user_data.resize(rng()%128);for(auto& b:d.user_data)b=static_cast<uint8_t>(rng());
        auto bytes=w::encode(slot);check(w::decode_slot(bytes)==slot);
        w::Content c;c.window=slot.window;c.container=slot.container.value_or(w::Container{});c.items={slot.item};c.storage=slot.storage.value_or(w::Descriptor{});
        bytes=w::encode(c);check(w::decode_content(bytes)==c);
    }
}
void linux_live_empty_inventory(){
    // Exact empty-inventory wire shape observed from the private Linux BDS
    // 1.26.45.1 fixture, stock PC 1.26.45 client, protocol 2169. 300 bytes,
    // only byte 1 is nonzero (36 slots). Contains no player/item metadata.
    // SHA256 9eb16cf918c14fdd34c720d989e3279b59944af76f9a047b752ea403dfa2a0d6.
    std::vector<uint8_t> bytes(300,0);bytes[1]=36;
    auto content=w::decode_content(bytes);
    check(content.window==0&&content.items.size()==36&&content.container.role==0);
    check(w::encode(content)==bytes);
    w::Observation o;o.registry(registry());o.content(content);
    check(o.inventory_ready());for(const auto& d:o.inventory())check(d==w::Descriptor{});
    w::Slot delta;delta.container=w::Container{};delta.item={1,1,0,123,0,{}};
    o.slot(delta);check(o.inventory_ready()&&o.inventory()[0]==delta.item);
    for(uint16_t role=1;role<256;++role){
        if(role==12)continue;
        o.content(content);auto bad=content;bad.container.role=static_cast<uint8_t>(role);
        refused([&]{o.content(bad);});check(!o.inventory_ready());
        o.content(content);delta.container->role=static_cast<uint8_t>(role);
        refused([&]{o.slot(delta);});check(!o.inventory_ready());
    }
    o.content(content);delta.container=w::Container{0,7};refused([&]{o.slot(delta);});check(!o.inventory_ready());
    w::Inbox inbox;inbox.submit("player",162,w::encode_registry(registry()));inbox.submit("player",49,bytes);
    inbox.poll(2);check(inbox.stats().registries==1&&inbox.stats().inventories==1&&inbox.stats().rejected==0);
}
void linux_dynamic_registry_inventory(){
    // Independently generated empty descriptors with the structural shape of
    // the login capture: window125/count64/role63/present LE32 dynamic ID.
    // No captured inventory, book data, runtime item ID or real dynamic ID.
    std::vector<uint8_t> bytes(528,0);bytes[0]=125;bytes[1]=64;
    bytes[514]=63;bytes[515]=1;bytes[516]=7;
    auto dynamic=w::decode_content(bytes);
    check(dynamic.window==125&&dynamic.items.size()==64&&dynamic.container.role==63&&dynamic.container.dynamic_id==7);
    check(w::encode(dynamic)==bytes);
    for(size_t n=0;n<bytes.size();++n)refused([&]{w::decode_content(std::span(bytes).first(n));});
    auto trailing=bytes;trailing.push_back(0);refused([&]{w::decode_content(trailing);});
    for(unsigned window=0;window<256;++window){
        if(window==125)continue;
        auto invalid=dynamic;invalid.window=window;refused([&]{w::encode(invalid);});
        auto wire=bytes;wire[0]=static_cast<uint8_t>(window);
        if(window>=128){wire[0]=static_cast<uint8_t>((window&127)|128);wire.insert(wire.begin()+1,1);}
        refused([&]{w::decode_content(wire);});
    }
    for(unsigned role=0;role<256;++role){
        if(role==63)continue;
        auto invalid=dynamic;invalid.container.role=static_cast<uint8_t>(role);refused([&]{w::encode(invalid);});
        auto wire=bytes;wire[514]=static_cast<uint8_t>(role);refused([&]{w::decode_content(wire);});
    }
    auto no_id=bytes;no_id[515]=0;no_id.erase(no_id.begin()+516,no_id.begin()+520);
    refused([&]{w::decode_content(no_id);});
    auto invalid=dynamic;invalid.container.dynamic_id.reset();refused([&]{w::encode(invalid);});
    invalid=dynamic;invalid.items.resize(65);refused([&]{w::encode(invalid);});
    auto too_many=bytes;too_many[1]=65;too_many.insert(too_many.begin()+514,8,0);refused([&]{w::decode_content(too_many);});
    w::Content main;main.items.resize(36);main.items[7]={1,6,0,101,0,{}};
    w::Inbox inbox;inbox.submit("player",162,w::encode_registry(registry()));
    inbox.submit("player",49,w::encode(main));inbox.submit("player",49,bytes);
    check(inbox.poll(3)==3&&inbox.stats().registries==1&&inbox.stats().inventories==1&&inbox.stats().rejected==0);
    w::Observation observed;observed.registry(registry());observed.content(main);observed.content(dynamic);
    w::Slot delta;delta.window=125;delta.slot=63;delta.container=dynamic.container;delta.item={1,1,0,102,0,{}};
    observed.slot(w::decode_slot(w::encode(delta)));
    check(observed.inventory_ready()&&observed.inventory()[7]==main.items[7]&&observed.inventory()[0].count==0);
    inbox.submit("player",49,no_id);inbox.poll();
    check(inbox.stats().rejected==1&&!inbox.stats().registries&&!inbox.stats().inventories&&inbox.stats().resident_wire_bytes==0);
}
void inbox(){
    w::Inbox inbox;auto entries=registry();auto bytes=w::encode_registry(entries);
    w::Content content;content.items.resize(36);content.container={12,{}};auto items=w::encode(content);
    inbox.submit("a",162,bytes);inbox.submit("a",49,items);inbox.submit("b",162,bytes);inbox.submit("b",49,items);
    check(inbox.stats().pending==4&&!inbox.stats().registries&&!inbox.stats().inventories);
    check(inbox.poll()==1&&inbox.stats().pending==3);check(inbox.poll(4)==3);
    check(inbox.stats().registries==2&&inbox.stats().inventories==2&&inbox.stats().queued_bytes==0);
    inbox.submit("a",162,bytes);check(inbox.stats().inventories==1);inbox.poll();check(inbox.stats().inventories==2);
    entries[1].version=1;auto changed=w::encode_registry(entries);inbox.submit("a",162,changed);inbox.poll();
    check(inbox.stats().registries==2&&inbox.stats().inventories==1);
    inbox.submit("a",49,items);inbox.poll();check(inbox.stats().inventories==2);
    inbox.submit("b",49,std::array<uint8_t,1>{0});inbox.poll();check(inbox.stats().players==1&&inbox.stats().rejected==1);
    inbox.submit("b",49,items);inbox.poll();check(inbox.stats().inventories==1&&inbox.stats().registries==1);
    inbox.remove("b");inbox.remove("a");check(inbox.stats().players==0&&inbox.stats().resident_wire_bytes==0);
    for(int i=0;i<33;++i)inbox.submit("overflow",49,items);
    check(inbox.stats().pending==0&&inbox.stats().queued_bytes==0&&inbox.stats().players==0&&inbox.stats().rejected==2);
    for(int i=0;i<17;++i)inbox.submit(std::to_string(i),162,bytes);
    check(inbox.stats().players==16&&inbox.stats().pending==16&&inbox.stats().rejected==3);
    check(inbox.poll(32)==16&&inbox.stats().registries==16);
    inbox.clear();check(inbox.stats().players==0&&inbox.stats().queued_bytes==0&&inbox.stats().resident_wire_bytes==0);
    std::vector<w::RegistryEntry> many;
    for(int i=0;i<5000;++i)many.push_back({"example:item"+std::to_string(i),static_cast<int16_t>(i),false,0,{10,0,0}});
    auto large=w::encode_registry(many);
    for(int i=0;i<14;++i)inbox.submit(std::to_string(i),162,large);
    check(inbox.poll(32)==14&&inbox.stats().registries==13&&inbox.stats().rejected==4);
    for(int i=0;i<14;++i)inbox.remove(std::to_string(i));
    check(inbox.stats().resident_wire_bytes==0&&inbox.stats().players==0);
    std::vector<uint8_t> oversized_queue(8*1024*1024,0);
    inbox.submit("a",162,oversized_queue);inbox.submit("b",162,oversized_queue);inbox.submit("c",162,oversized_queue);
    check(inbox.stats().queued_bytes==16*1024*1024&&inbox.stats().players==2&&inbox.stats().rejected==5);
    check(inbox.poll(4)==2&&inbox.stats().queued_bytes==0&&inbox.stats().players==0&&inbox.stats().rejected==7);
    refused([&]{inbox.poll(0);});refused([&]{inbox.poll(33);});
}
}
int main(){try{conformance();malformed();observation();properties();linux_live_empty_inventory();linux_dynamic_registry_inventory();inbox();std::cout<<checks<<" item wire checks passed\n";return 0;}
    catch(const Error&e){std::cerr<<"unexpected status "<<e.status<<" after "<<checks<<" checks\n";return 1;}
    catch(const std::exception&e){std::cerr<<e.what()<<" after "<<checks<<" checks\n";return 1;}}
