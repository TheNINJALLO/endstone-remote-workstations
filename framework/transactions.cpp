#include <oni/vcf/core.hpp>
#include <limits>
#include <cmath>
#include <bit>
namespace oni::vcf {
namespace {
struct NbtReader {
 std::span<const uint8_t> b; size_t p=0; uint32_t nodes=0;
 uint64_t number(size_t n) { require(n<=b.size()-p); uint64_t v=0; for(size_t i=0;i<n;i++) v|=uint64_t(b[p++])<<(8*i); return v; }
 void skip(size_t n) { require(n<=b.size()-p); p+=n; }
 void name() { skip(static_cast<size_t>(number(2))); }
 void tag(uint8_t t,uint32_t depth) {
  require(depth<=32 && ++nodes<=65536);
  switch(t) {
   case 1: skip(1); break; case 2:skip(2);break; case 3:skip(4);break; case 4:skip(8);break;
   case 5:require(std::isfinite(std::bit_cast<float>(static_cast<uint32_t>(number(4)))));break;
   case 6:require(std::isfinite(std::bit_cast<double>(number(8))));break;
   case 7:case 11:case 12: {auto n=number(4);require(n<=65536);skip(static_cast<size_t>(n)*(t==7?1:t==11?4:8));break;}
   case 8:name();break;
   case 9:{auto child=number(1),n=number(4);require(n<=65536 && child<=12 && (child!=0 || n==0));for(uint64_t i=0;i<n;i++)tag(static_cast<uint8_t>(child),depth+1);break;}
   case 10:{ std::set<std::string> keys; for(;;){auto child=number(1);if(!child)break; auto n=number(2);require(n<=b.size()-p);std::string key(reinterpret_cast<const char*>(b.data()+p),static_cast<size_t>(n));require(keys.insert(key).second);skip(static_cast<size_t>(n));tag(static_cast<uint8_t>(child),depth+1);}break;}
   default:throw Error{VCF_INVALID};
  }
 }
};
void normalized(Item& i) {if(i.count==0)i=Item{};}
void check_item(const Item&i){require(i.count<=i.limit && i.limit<=255 && i.limit>0);if(i.count)require(!i.id.empty());}
}
void validate_nbt(std::span<const uint8_t> bytes) {
 if(bytes.empty())return;
 require(bytes.size()<=1024*1024);
 NbtReader r{bytes}; require(r.number(1)==10);r.name();r.tag(10,0);require(r.p==bytes.size());
}
void Snapshot::transfer(std::span<const Move> moves) {
 require(!moves.empty() && moves.size()<=128);require(revision!=UINT64_MAX,VCF_CAPACITY);
 auto next=slots;
 for(auto m:moves){
  require(m.from<next.size() && m.to<next.size() && m.from!=m.to && m.count>0);
  auto &a=next[m.from],&b=next[m.to];
  require((a.policy&VCF_EXTRACT) && !(a.policy&(VCF_PREVIEW|VCF_RESULT)) && (b.policy&VCF_INSERT) && !(b.policy&(VCF_PREVIEW|VCF_RESULT)),VCF_DENIED);
  require(a.item.count>=m.count && (b.item.empty()||b.item.matches(a.item)),VCF_CONFLICT);
  require(m.count<=a.item.limit && b.item.count<=a.item.limit-m.count,VCF_CAPACITY);
  if(b.item.empty()){b.item=a.item;b.item.count=0;}
  b.item.count+=m.count;a.item.count-=m.count;normalized(a.item);
 }
 slots.swap(next);++revision;
}
void Snapshot::execute(Rule& r,uint64_t expected) {
 require(r.revision==expected,VCF_STALE);require(r.stock>0,VCF_CAPACITY);
 require(r.revision!=UINT64_MAX && revision!=UINT64_MAX,VCF_CAPACITY);
 require(!r.costs.empty() && r.costs.size()<=16 && r.output.count>0);
 require(r.output_slot<slots.size() && (slots[r.output_slot].policy&VCF_RESULT) && !(slots[r.output_slot].policy&VCF_PREVIEW),VCF_DENIED);
 check_item(r.output);auto next=slots;std::set<uint32_t> seen;
 for(const auto& [slot,cost]:r.costs){
  require(slot<next.size() && slot!=r.output_slot && seen.insert(slot).second && cost.count>0);
  auto& source=next[slot];
  require(!(source.policy&(VCF_RESULT|VCF_PREVIEW)),VCF_DENIED);
  require(source.item.matches(cost) && source.item.count>=cost.count,VCF_CONFLICT);
  source.item.count-=cost.count;normalized(source.item);
 }
 auto& output=next[r.output_slot].item;
 require(output.empty() || output.matches(r.output),VCF_CONFLICT);
 require(r.output.count<=r.output.limit && output.count<=r.output.limit-r.output.count,VCF_CAPACITY);
 if(output.empty()){output=r.output;output.count=0;}
 output.count+=r.output.count;
 slots.swap(next);--r.stock;++r.revision;++revision;
}
}
