#include "held_items.hpp"
#include <iostream>
#include <limits>
using namespace oni::vcf;
namespace {
void check(bool ok){if(!ok)throw std::runtime_error("Held snapshot assertion failed");}
template<class F>void refuses(vcf_status status,F f){try{f();throw std::runtime_error("Expected refusal");}catch(const Error& e){check(e.status==status);}}
void snapshots(){
 using namespace endstone;
 CompoundTag tiny;tiny["x"]=ShortTag{-2};
 check(held::Encoder{}.encode(tiny)==std::vector<uint8_t>({10,0,0,2,1,0,'x',254,255,0}));
 check(held::Encoder{}.encode(CompoundTag{})==std::vector<uint8_t>({10,0,0,0}));
 CompoundTag nbt;nbt["byte"]=ByteTag{255};nbt["short"]=ShortTag{-123};nbt["int"]=IntTag{-123};nbt["long"]=LongTag{INT64_MIN};
 nbt["float"]=FloatTag{-0.0f};nbt["double"]=DoubleTag{-0.0};nbt["bytes"]=ByteArrayTag{0,128,255};nbt["ints"]=IntArrayTag{INT32_MIN,0,INT32_MAX};
 CompoundTag display;display["Name"]=StringTag{"Named held box"};display["Lore"]=ListTag{StringTag{"line one"},StringTag{"line two"}};nbt["display"]=display;
 CompoundTag stored;stored["Name"]=StringTag{"minecraft:diamond"};stored["Count"]=ByteTag{3};stored["Slot"]=ByteTag{26};nbt["Items"]=ListTag{stored};
 nbt["empty"]=ListTag{};nbt["remote_workstations:held_id"]=StringTag{"12345678-1234-1234-1234-123456789abc"};
 auto saved=nbt;auto a=held::freeze("minecraft:purple_shulker_box",1,0,nbt);
 validate_nbt(a.metadata);check(nbt==saved&&a.durable_id=="12345678-1234-1234-1234-123456789abc");
 check(a==held::freeze("minecraft:purple_shulker_box",1,0,saved));
 auto changed=saved;changed["float"]=FloatTag{0.0f};check(a!=held::freeze(a.identifier,1,0,changed));
 changed=saved;changed["double"]=DoubleTag{0.0};check(a!=held::freeze(a.identifier,1,0,changed));
 changed=saved;changed["int"]=ShortTag{-123};check(a!=held::freeze(a.identifier,1,0,changed));
 check(held::digest_input(a)!=held::digest_input(held::freeze(a.identifier,1,1,saved)));
 check(held::digest_input(a)!=held::digest_input(held::freeze(a.identifier,2,0,saved)));
 check(held::kind("minecraft:light_gray_bundle")=="bundle"&&held::kind("minecraft:undyed_shulker_box")=="shulker");
 check(held::kind("minecraft:written_book")=="writtenbook"&&held::kind("minecraft:writable_book")=="bookediting");
 // A bundle's externally stored contents are absent from getNbt(). Even a
 // perfectly valid metadata compound cannot qualify its complete snapshot.
 refuses(VCF_UNAVAILABLE,[&]{held::freeze("minecraft:bundle",1,0,CompoundTag{});});
 refuses(VCF_UNAVAILABLE,[&]{held::freeze("minecraft:light_gray_bundle",1,0,saved);});
 refuses(VCF_UNAVAILABLE,[&]{held::kind("myplugin:purple_shulker_box");});
 refuses(VCF_UNAVAILABLE,[&]{held::kind("minecraft:chartreuse_shulker_box");});
 check(!held::uuid("00000000-0000-0000-0000-000000000000")&&!held::uuid("12345678-1234-1234-1234-123456789ABC"));
 changed=saved;changed["remote_workstations:held_id"]=IntTag{1};refuses(VCF_INVALID,[&]{held::freeze(a.identifier,1,0,changed);});
 changed=saved;changed["float"]=FloatTag{std::numeric_limits<float>::infinity()};refuses(VCF_INVALID,[&]{held::freeze(a.identifier,1,0,changed);});
 changed=saved;changed["double"]=DoubleTag{std::numeric_limits<double>::quiet_NaN()};refuses(VCF_INVALID,[&]{held::freeze(a.identifier,1,0,changed);});
 changed=saved;changed["large"]=StringTag{std::string(65535,'x')};refuses(VCF_CAPACITY,[&]{held::freeze(a.identifier,1,0,changed);});
 changed=saved;changed["many"]=ByteArrayTag{std::vector<uint8_t>(4097)};refuses(VCF_CAPACITY,[&]{held::freeze(a.identifier,1,0,changed);});
 CompoundTag nested;for(int i=0;i<18;++i){CompoundTag next;next["child"]=nested;nested=std::move(next);}
 refuses(VCF_CAPACITY,[&]{held::freeze(a.identifier,1,0,nested);});
}
}
int main(){try{snapshots();std::cout<<"Held metadata widths, signed zero, nested contents, identity and bounds passed\n";}
 catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error& e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
