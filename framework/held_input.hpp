#pragma once
#include <oni/vcf/core.hpp>
#include <bit>
#include <cmath>
namespace oni::vcf::held {
// Wait for the opening use gesture to finish before projecting a block. The
// adapter cancels these inventory-bearing packets; none reach native mutation.
// A fixed outer timeout still bounds admission if input never becomes quiet.
class InputQuiet {
 uint64_t last_;
public:
 explicit InputQuiet(uint64_t now):last_(now){}
 void observed(uint64_t now){require(now>=last_);last_=now;}
 bool ready(uint64_t now)const{require(now>=last_);return now-last_>=350;}
};
// PlayerActionPacket: runtime ID, action, two block positions and face. The
// source header and protocol2169 schema agree on these eight varint32 fields.
inline int32_t player_action(std::span<const uint8_t> data){
 require(data.size()<=64);size_t offset=0;
 auto var=[&](unsigned bits){uint64_t n=0;for(unsigned shift=0;shift<bits;shift+=7){require(offset<data.size());const auto b=data[offset++];if(bits-shift<7)require((b&127)<(uint64_t(1)<<(bits-shift)));n|=uint64_t(b&127)<<shift;if(!(b&128)){require(!shift||(b&127));return n;}}throw Error{VCF_INVALID};};
 require(var(64)!=0);const auto encoded=static_cast<uint32_t>(var(32));
 for(unsigned i=0;i<7;++i)var(32);require(offset==data.size());
 return std::bit_cast<int32_t>((encoded>>1)^(uint32_t(0)-(encoded&1)));
}
inline bool opening_action(int32_t action){return action==11||action==12||action==28||action==29;}
// Protocol2169 PlayerAuthInput prefix. Movement is still delivered to BDS;
// inventory-bearing optional payloads are refused while a held view owns it.
inline bool inventory_input(std::span<const uint8_t> data){
 require(data.size()<=65536,VCF_CAPACITY);size_t offset=0;
 auto fixed=[&](unsigned size){require(size<=data.size()-offset);uint64_t n=0;for(unsigned i=0;i<size;++i)n|=uint64_t(data[offset++])<<(8*i);return n;};
 auto boolean=[&]{auto n=fixed(1);require(n<2);return n!=0;};
 auto var=[&](unsigned bits=32){uint64_t n=0;for(unsigned shift=0;shift<bits;shift+=7){auto b=fixed(1);if(bits-shift<7)require((b&127)<(uint64_t(1)<<(bits-shift)));n|=(b&127)<<shift;if(!(b&128)){require(!shift||(b&127));return n;}}throw Error{VCF_INVALID};};
 for(unsigned i=0;i<8;++i)require(std::isfinite(std::bit_cast<float>(static_cast<uint32_t>(fixed(4)))));require(boolean());
 auto count=var();require(count<=66);std::set<uint32_t> flags;
 for(size_t i=0;i<count;++i){auto n=var();require(!(n&1)&&n/2<=65&&flags.insert(static_cast<uint32_t>(n/2)).second);}
 var();var();var();fixed(8);var(64);for(unsigned i=0;i<3;++i)fixed(4);
 for(unsigned i=0;i<3;++i){require(boolean());if(boolean())return true;}
 return flags.contains(34)||flags.contains(35)||flags.contains(36);
}
}
