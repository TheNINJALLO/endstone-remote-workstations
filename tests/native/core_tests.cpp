#include <oni/vcf/core.hpp>
#include <oni/vcf/ledger.hpp>
#include <oni/vcf/machine.hpp>
#include <oni/vcf/sdk.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <array>
using namespace oni::vcf;
int checks=0;
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error("Failed: " #x);}while(false)
template<class F>void refuses(vcf_status code,F fn){try{fn();throw std::runtime_error("Expected refusal");}catch(const Error&e){CHECK(e.status==code);}}
Item stone(uint32_t count){return {"minecraft:stone",count,64,{}};}
vcf_status VCF_CALL increment(void*p,const vcf_event*){++*static_cast<int*>(p);return VCF_OK;}
void transfers(){
 Snapshot s{{{stone(12),3},{stone(60),3},{Item{},3}}};auto before=s;
 std::array<Move,2> bad{{{0,2,4},{0,1,8}}};refuses(VCF_CAPACITY,[&]{s.transfer(bad);});CHECK(s.slots==before.slots&&s.revision==0);
 std::array<Move,2> good{{{0,2,4},{0,1,4}}};s.transfer(good);CHECK(s.slots[0].item.count==4&&s.slots[1].item.count==64&&s.slots[2].item.count==4);
 s.slots[2].policy=VCF_PREVIEW;std::array<Move,1> preview{{{2,0,1}}};refuses(VCF_DENIED,[&]{s.transfer(preview);});
 s.slots[2].policy=3;s.slots[2].item.nbt={10,0,0,1,1,0,'x',7,0};refuses(VCF_CONFLICT,[&]{s.transfer(preview);});
 std::mt19937 rng(917);s={{{stone(64),3},{stone(64),3},{Item{},3},{Item{},3}}};
 for(int i=0;i<10000;i++){auto a=rng()%4,b=rng()%4;auto n=rng()%70;std::array<Move,1> m{{{a,b,n}}};try{s.transfer(m);}catch(const Error&){}uint32_t total=0;for(auto&v:s.slots){CHECK(v.item.count<=64);total+=v.item.count;}CHECK(total==128);}
}
void rules(){
 Snapshot s{{{stone(3),3},{Item{},VCF_RESULT}}};Rule r{"recycle",1,2,{{0,stone(2)}},1,5,{"minecraft:paper",1,64,{}}};
 s.execute(r,1);CHECK(s.slots[0].item.count==1&&s.slots[1].item.count==1&&r.stock==1);
 auto before=s;refuses(VCF_STALE,[&]{s.execute(r,1);});CHECK(s.slots==before.slots);
 refuses(VCF_CONFLICT,[&]{s.execute(r,2);});CHECK(s.slots==before.slots&&r.stock==1);
 s.slots[0].item=stone(3);s.slots[1].item.count=64;refuses(VCF_CAPACITY,[&]{s.execute(r,2);});CHECK(s.slots[0].item.count==3&&r.stock==1);
 Machines machines;s.slots[1].item=Item{};auto id=machines.add(s,r,10);CHECK(machines.tick(14)==0);CHECK(machines.tick(15)==1);CHECK(machines.state(id).slots[0].item.count==1);
}
void nbt(){
 validate_nbt({});std::vector<uint8_t> good{10,0,0,1,1,0,'x',255,0};validate_nbt(good);
 for(size_t n=1;n<good.size();n++)refuses(VCF_INVALID,[&]{validate_nbt(std::span(good).first(n));});
 auto trailing=good;trailing.push_back(0);refuses(VCF_INVALID,[&]{validate_nbt(trailing);});
 std::vector<uint8_t> duplicate{10,0,0,1,1,0,'x',1,1,1,0,'x',2,0};refuses(VCF_INVALID,[&]{validate_nbt(duplicate);});
 std::vector<uint8_t> nan{10,0,0,5,1,0,'x',0,0,192,127,0};refuses(VCF_INVALID,[&]{validate_nbt(nan);});
}
void engine(){
 bool allowed=true;Host h;h.consumer_allowed=[](auto){return true;};h.permission=[&](auto,auto){return allowed;};h.item_limit=[](auto id){return id=="minecraft:stone"?64u:0u;};h.open=[](auto&s){return s.is_menu?VCF_OK:VCF_UNAVAILABLE;};
 Engine e(h);attach_engine(&e);vcf_api api{};CHECK(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)==VCF_OK);
 sdk::Client a(api,"one"),b(api,"two");int calls=0;a.action("private",increment,&calls);a.action("public",increment,&calls,"use",true);
 CHECK(api.invoke(b.owner(),sdk::view("player"),sdk::view("one:private"),nullptr)==VCF_INVALID);
 refuses(VCF_DENIED,[&]{e.invoke(b.owner(),"player","one:private");});
 auto ticket=b.invoke("player","one:public");CHECK(calls==0);allowed=false;e.tick();CHECK(calls==0&&b.info(ticket).result==VCF_DENIED);
 allowed=true;ticket=b.invoke("player","one:public");e.tick();CHECK(calls==1&&b.info(ticket).state==VCF_TERMINAL);e.tick();CHECK(calls==1);
 auto revoked=b.invoke("player","one:public");CHECK(a.dispose()==VCF_OK);e.tick();CHECK(calls==1&&b.info(revoked).result==VCF_NOT_FOUND);
 for(const auto&row:catalog()){auto s=b.prepare("player",row.id,VCF_TRANSIENT);b.open(s);e.tick();CHECK(b.info(s).result==VCF_UNAVAILABLE);CHECK(api.forget(b.owner(),s)==VCF_OK);}
 auto s=b.prepare("player","chest",VCF_TRANSIENT);b.set(s,0,"minecraft:stone",2);b.open(s);b.close(s);e.tick();CHECK(b.info(s).state==VCF_TERMINAL);
 vcf_status thread_status=VCF_OK;std::thread worker([&]{uint32_t n=0;thread_status=api.catalog_count(&n);});worker.join();CHECK(thread_status==VCF_WRONG_THREAD);
 uint32_t count=0;CHECK(api.catalog_count(&count)==VCF_OK&&count==69);
 CHECK(b.dispose()==VCF_OK);e.shutdown();attach_engine(nullptr);
}
void journal(){
 auto dir=std::filesystem::temp_directory_path()/("vcf-ledger-"+std::to_string(std::random_device{}()));std::filesystem::create_directory(dir);auto path=dir/"ledger.vcfj";
 {Ledger l(path);l.append(55,Boundary::prepared,{});refuses(VCF_CONFLICT,[&]{Ledger second(path);});l.append(55,Boundary::debit,{});refuses(VCF_STALE,[&]{l.append(55,Boundary::debit,{});});}
 {Ledger l(path);CHECK(l.records().size()==2&&l.quarantined()==std::vector<uint64_t>{55});l.append(55,Boundary::escrow,{});}
 {std::ofstream f(path,std::ios::binary|std::ios::app);f.put('V');}
 refuses(VCF_QUARANTINED,[&]{Ledger corrupt(path);});
 // Targets are fixed files in this exclusively created temporary directory.
 std::filesystem::remove(path);std::filesystem::remove(dir);
}
int main(){try{transfers();rules();nbt();engine();journal();std::cout<<checks<<" checks passed\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
