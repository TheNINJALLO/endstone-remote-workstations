#pragma once
#include <oni/vcf/core.hpp>
#include <endstone/nbt/tag.h>
#include <bit>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace endstone {class Player;}
namespace oni::vcf::held {
// Detached bytes preserve the public SDK's tag widths, signed zero and list
// element types. They are private snapshots, never a packet or ownership lock.
class Encoder {
 std::vector<uint8_t> bytes_;size_t elements_=0;
 void room(size_t n){require(n<=65536&&bytes_.size()<=65536-n,VCF_CAPACITY);}
 void put(uint64_t n,unsigned width){room(width);for(unsigned i=0;i<width;++i)bytes_.push_back(static_cast<uint8_t>(n>>(8*i)));}
 void string(std::string_view s){require(s.size()<=UINT16_MAX,VCF_CAPACITY);room(2+s.size());put(s.size(),2);bytes_.insert(bytes_.end(),s.begin(),s.end());}
 void compound(const endstone::CompoundTag& value,unsigned depth){
  require(depth<=16,VCF_CAPACITY);
  for(const auto&[name,tag]:value){require(tag.type()!=endstone::nbt::Type::End);put(static_cast<uint8_t>(tag.type()),1);string(name);payload(tag,depth+1);}
  put(0,1);
 }
 void payload(const endstone::nbt::Tag& tag,unsigned depth){
  require(depth<=16&&++elements_<=4096,VCF_CAPACITY);
  tag.visit([&](const auto& value){using T=std::remove_cvref_t<decltype(value)>;
   if constexpr(std::is_same_v<T,std::monostate>){throw Error{VCF_INVALID};}
   else if constexpr(std::is_same_v<T,endstone::CompoundTag>)compound(value,depth);
   else if constexpr(std::is_same_v<T,endstone::ListTag>){
    require(value.size()<=4096-elements_,VCF_CAPACITY);put(static_cast<uint8_t>(value.type()),1);put(value.size(),4);
    for(const auto& entry:value){require(entry.type()==value.type());payload(entry,depth+1);}
   }else if constexpr(std::is_same_v<T,endstone::ByteArrayTag>||std::is_same_v<T,endstone::IntArrayTag>){
    require(value.size()<=4096-elements_,VCF_CAPACITY);elements_+=value.size();put(value.size(),4);
    for(auto item:value)put(static_cast<std::make_unsigned_t<decltype(item)>>(item),sizeof(item));
   }else if constexpr(std::is_same_v<T,endstone::StringTag>)string(value.value());
   else if constexpr(std::is_same_v<T,endstone::FloatTag>){require(std::isfinite(value.value()));put(std::bit_cast<uint32_t>(value.value()),4);}
   else if constexpr(std::is_same_v<T,endstone::DoubleTag>){require(std::isfinite(value.value()));put(std::bit_cast<uint64_t>(value.value()),8);}
   else {using V=typename T::value_type;put(static_cast<std::make_unsigned_t<V>>(value.value()),sizeof(V));}
  });
 }
public:
 std::vector<uint8_t> encode(const endstone::CompoundTag& nbt){bytes_.clear();elements_=0;put(10,1);put(0,2);compound(nbt,0);return std::move(bytes_);}
};
inline std::string_view kind(std::string_view identifier){
 if(identifier=="minecraft:written_book")return "writtenbook";
 if(identifier=="minecraft:writable_book")return "bookediting";
 if(identifier=="minecraft:undyed_shulker_box"||identifier=="minecraft:shulker_box")return "shulker";
 if(identifier=="minecraft:bundle")return "bundle";
 constexpr std::string_view colors[]={"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"};
 for(auto color:colors){
  if(identifier=="minecraft:"+std::string(color)+"_shulker_box")return "shulker";
  if(identifier=="minecraft:"+std::string(color)+"_bundle")return "bundle";
 }
 throw Error{VCF_UNAVAILABLE};
}
inline bool uuid(std::string_view value){
 if(value.size()!=36)return false;bool nonzero=false;
 for(size_t i=0;i<value.size();++i){
  if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;continue;}
  const char c=value[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;nonzero|=c!='0';
 }
 return nonzero;
}
struct Frozen {
 std::string canonical_id,identifier,durable_id;uint32_t amount;int32_t auxiliary;
 std::vector<uint8_t> metadata;
 bool operator==(const Frozen&)const=default;
};
inline Frozen freeze(std::string identifier,int amount,int auxiliary,const endstone::CompoundTag& nbt){
 require(amount>0&&amount<=64);const auto canonical=kind(identifier);
 // The exact Linux client test filled a real bundle with six stones while
 // ItemStack::getNbt() and its digest stayed unchanged. Its separate contents
 // are not exposed by this public SDK snapshot. Never return a partial bundle
 // as a content snapshot, including empty bundles that may later be filled.
 require(canonical!="bundle",VCF_UNAVAILABLE);
 Frozen result{std::string(canonical),std::move(identifier),{},static_cast<uint32_t>(amount),auxiliary,Encoder{}.encode(nbt)};
 constexpr auto identity="remote_workstations:held_id";
 if(nbt.contains(identity)){
  const auto& tag=nbt.at(identity);require(tag.type()==endstone::nbt::Type::String);
  result.durable_id=tag.get<endstone::StringTag>().value();require(uuid(result.durable_id));
 }
 return result;
}
inline std::vector<uint8_t> digest_input(const Frozen& snapshot){
 std::vector<uint8_t> bytes{'V','C','F','H',1};
 auto put=[&](uint32_t n){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<uint8_t>(n>>(8*i)));};
 put(static_cast<uint32_t>(snapshot.identifier.size()));bytes.insert(bytes.end(),snapshot.identifier.begin(),snapshot.identifier.end());
 put(snapshot.amount);put(static_cast<uint32_t>(snapshot.auxiliary));put(static_cast<uint32_t>(snapshot.metadata.size()));bytes.insert(bytes.end(),snapshot.metadata.begin(),snapshot.metadata.end());
 return bytes;
}
vcf_held_info inspect(endstone::Player&);
}
