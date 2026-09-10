#include "saved_item.hpp"
#include <bit>
#include <cmath>

namespace oni::vcf::inventory::saved {
namespace {
constexpr size_t bound=VCF_ITEM_SAVE_MAX_BYTES;
struct Field {uint8_t type=0;Bytes payload;};
struct Reader {
 std::span<const uint8_t> bytes;size_t offset=0,nodes=0;
 uint64_t number(unsigned size){require(size<=8&&size<=bytes.size()-offset);uint64_t n=0;for(unsigned i=0;i<size;++i)n|=uint64_t(bytes[offset++])<<(8*i);return n;}
 void skip(size_t size){require(size<=bytes.size()-offset);offset+=size;}
 std::string string(){const auto size=number(2);require(size<=bytes.size()-offset);std::string s(reinterpret_cast<const char*>(bytes.data()+offset),size);skip(size);return s;}
 void payload(uint8_t type,unsigned depth){
  require(depth<=32&&++nodes<=65536,VCF_CAPACITY);
  switch(type){
   case 1:skip(1);break;case 2:skip(2);break;case 3:case 5:skip(4);break;case 4:case 6:skip(8);break;
   case 7:case 11:case 12:{const auto count=number(4);require(count<=bound,VCF_CAPACITY);skip(count*(type==7?1:type==11?4:8));break;}
   case 8:(void)string();break;
   case 9:{const auto child=number(1),count=number(4);require(count<=bound&&child<=12&&(child||!count));for(size_t i=0;i<count;++i)payload(static_cast<uint8_t>(child),depth+1);break;}
   case 10:{std::set<std::string> names;for(;;){const auto child=number(1);if(!child)break;require(names.insert(string()).second);payload(static_cast<uint8_t>(child),depth+1);}break;}
   default:throw Error{VCF_INVALID};
  }
 }
};
void raw(Bytes& out,std::span<const uint8_t> bytes){require(bytes.size()<=bound&&out.size()<=bound-bytes.size(),VCF_CAPACITY);out.insert(out.end(),bytes.begin(),bytes.end());}
void number(Bytes& out,uint64_t value,unsigned width){require(width<=bound-out.size(),VCF_CAPACITY);for(unsigned i=0;i<width;++i)out.push_back(static_cast<uint8_t>(value>>(8*i)));}
void string(Bytes& out,std::string_view value){require(value.size()<=UINT16_MAX,VCF_CAPACITY);number(out,value.size(),2);raw(out,{reinterpret_cast<const uint8_t*>(value.data()),value.size()});}
struct Compound {
 std::map<std::string,Field,std::less<>> fields;
 static Compound payload(Reader& reader){
  Compound result;
  for(;;){auto type=static_cast<uint8_t>(reader.number(1));if(!type)break;auto name=reader.string();const auto begin=reader.offset;reader.payload(type,1);
   auto bytes=reader.bytes.subspan(begin,reader.offset-begin);require(result.fields.emplace(std::move(name),Field{type,Bytes(bytes.begin(),bytes.end())}).second);
  }
  return result;
 }
 static Compound parse(std::span<const uint8_t> bytes){
  require(!bytes.empty()&&bytes.size()<=bound,VCF_CAPACITY);validate_nbt(bytes);Reader reader{bytes};require(reader.number(1)==10&&reader.string().empty());
  auto result=payload(reader);require(reader.offset==bytes.size());return result;
 }
 Bytes body()const{Bytes out;for(const auto&[name,field]:fields){number(out,field.type,1);string(out,name);raw(out,field.payload);}number(out,0,1);return out;}
 Bytes encode()const{Bytes out{10,0,0};const auto bytes=body();raw(out,bytes);validate_nbt(out);return out;}
 const Field& get(std::string_view key,uint8_t type)const{const auto it=fields.find(key);require(it!=fields.end()&&it->second.type==type);return it->second;}
 uint8_t byte(std::string_view key)const{const auto& field=get(key,1);require(field.payload.size()==1);return field.payload[0];}
 std::string text(std::string_view key)const{Reader reader{get(key,8).payload};auto value=reader.string();require(reader.offset==reader.bytes.size());return value;}
 Compound compound(std::string_view key)const{Reader reader{get(key,10).payload};auto value=payload(reader);require(reader.offset==reader.bytes.size());return value;}
 void byte(std::string key,uint8_t value){fields[std::move(key)]={1,{value}};}
 void text(std::string key,std::string_view value){Bytes bytes;string(bytes,value);fields[std::move(key)]={8,std::move(bytes)};}
 void compound(std::string key,const Compound& value){fields[std::move(key)]={10,value.body()};}
};
std::string identifier(const Compound& item){auto name=item.text("Name");require(!name.empty()&&name.size()<=256);const auto colon=name.find(':');require(colon>0&&colon!=std::string::npos&&colon+1<name.size());for(size_t i=0;i<name.size();++i){const char c=name[i];require((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'||c=='-'||(c==':'&&i==colon)||(c=='/'&&i>colon));}return name;}
void backing(const Compound& item){require(shulker(identifier(item))&&item.byte("Count")==1,VCF_DENIED);}
bool uuid(std::string_view value){
 if(value.size()!=36)return false;bool nonzero=false;for(size_t i=0;i<value.size();++i){if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}else{auto c=value[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;nonzero|=c!='0';}}return nonzero;
}
constexpr std::string_view identity="remote_workstations:held_id";
}
Item decode(std::span<const uint8_t> bytes,const Limit& limit){
 if(bytes.empty())return {};require(bool(limit));auto tag=Compound::parse(bytes);Item item;item.id=identifier(tag);item.count=tag.byte("Count");
 require(item.count>0&&item.count<=64);item.limit=limit(item.id);require(item.limit>0&&item.limit<=64&&item.count<=item.limit,VCF_CAPACITY);
 tag.get("Damage",2);require(!tag.fields.contains("Slot"));tag.byte("Count",1);item.nbt=tag.encode();return item;
}
Bytes encode(const Item& item){
 if(item.empty()){require(item==Item{});return {};}
 require(item.limit>0&&item.limit<=64&&item.count<=item.limit);auto tag=Compound::parse(item.nbt);require(identifier(tag)==item.id&&tag.byte("Count")==1&&!tag.fields.contains("Slot"));
 tag.get("Damage",2);tag.byte("Count",static_cast<uint8_t>(item.count));return tag.encode();
}
bool shulker(std::string_view name){
 if(name=="minecraft:shulker_box"||name=="minecraft:undyed_shulker_box")return true;
 constexpr std::string_view colors[]={"white","orange","magenta","light_blue","yellow","lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"};
 for(auto color:colors)if(name=="minecraft:"+std::string(color)+"_shulker_box")return true;return false;
}
std::array<Bytes,27> contents(std::span<const uint8_t> bytes){
 auto source=Compound::parse(bytes);backing(source);std::array<Bytes,27> result{};
 if(!source.fields.contains("tag"))return result;auto data=source.compound("tag");if(!data.fields.contains("Items"))return result;
 Reader reader{data.get("Items",9).payload};const auto type=reader.number(1),size=reader.number(4);require(size<=27&&(type==10||(size==0&&type==0)));
 for(size_t i=0;i<size;++i){auto item=Compound::payload(reader);auto slot=item.byte("Slot");require(slot<27&&result[slot].empty());item.fields.erase("Slot");require(!shulker(identifier(item)),VCF_DENIED);
  require(item.byte("Count")>0&&item.byte("Count")<=64);item.get("Damage",2);
  // The block actor's Items rows can omit this native default. Extraction
  // supplies exactly that declared default, never drops arbitrary row fields.
  if(!item.fields.contains("WasPickedUp"))item.byte("WasPickedUp",0);
  result[slot]=item.encode();
 }
 require(reader.offset==reader.bytes.size());return result;
}
Bytes with_contents(std::span<const uint8_t> bytes,const std::array<Bytes,27>& slots){
 auto source=Compound::parse(bytes);backing(source);auto data=source.fields.contains("tag")?source.compound("tag"):Compound{};
 // Preserve the exact source bytes on a no-op, including absent/typed-empty
 // Items and the stored rows' omitted native defaults.
 if(contents(bytes)==slots)return Bytes(bytes.begin(),bytes.end());
 Bytes list{10};number(list,std::count_if(slots.begin(),slots.end(),[](const auto& item){return !item.empty();}),4);
 for(uint8_t i=0;i<27;++i)if(!slots[i].empty()){
  auto item=Compound::parse(slots[i]);require(!item.fields.contains("Slot")&&!shulker(identifier(item)),VCF_DENIED);require(item.byte("Count")>0&&item.byte("Count")<=64);item.get("Damage",2);
  item.byte("Slot",i);raw(list,item.body());
 }
 data.fields["Items"]={9,std::move(list)};source.compound("tag",data);return source.encode();
}
std::string durable_id(std::span<const uint8_t> bytes){
 auto source=Compound::parse(bytes);backing(source);if(!source.fields.contains("tag"))return {};auto tag=source.compound("tag");if(!tag.fields.contains(identity))return {};
 auto value=tag.text(identity);require(uuid(value));return value;
}
Bytes with_durable_id(std::span<const uint8_t> bytes,std::string_view id){
 require(uuid(id));const auto existing=durable_id(bytes);require(existing.empty()||existing==id,VCF_CONFLICT);if(!existing.empty())return Bytes(bytes.begin(),bytes.end());
 auto source=Compound::parse(bytes);auto tag=source.fields.contains("tag")?source.compound("tag"):Compound{};tag.text(std::string(identity),id);source.compound("tag",tag);return source.encode();
}
storage::State read_shulker(const SavedInventory& image,uint32_t source,uint64_t generation,int32_t next_identity,const Limit& limit){
 require(source<9&&generation&&next_identity>0);const auto stored=contents(image[source]);storage::State state;state.generation=generation;state.next_identity=next_identity;state.storage.resize(27);
 auto add=[&](storage::NetworkSlot& slot,const Bytes& bytes){slot.value.item=decode(bytes,limit);if(!slot.value.item.empty()){require(state.next_identity<INT32_MAX,VCF_CAPACITY);slot.identity=state.next_identity++;}};
 for(uint32_t i=0;i<36;++i)add(state.player[i],image[i]);state.player[source].value.policy=0;
 // Contents have their own storage slots. Keep the locked source's model
 // stable as those contents change; the full saved source remains in image
 // for the native lease and writer. Never serialize this model as its source.
 auto model=Compound::parse(image[source]);if(model.fields.contains("tag")){auto data=model.compound("tag");data.fields.erase("Items");if(data.fields.empty())model.fields.erase("tag");else model.compound("tag",data);}
 state.player[source].value.item=decode(model.encode(),limit);
 for(uint32_t i=0;i<27;++i)add(state.storage[i],stored[i]);storage::validate(state,storage::Layout("shulker"));return state;
}
SavedInventory project_shulker(const SavedInventory& image,uint32_t source,const storage::State& before,const storage::State& after,const Limit& limit){
 const auto actual=read_shulker(image,source,before.generation,1,limit);storage::validate(before,storage::Layout("shulker"));storage::validate(after,storage::Layout("shulker"));
 require(storage::same_inventory(actual,before),VCF_STALE);require(after.generation==before.generation&&after.revision>before.revision,VCF_STALE);
 require(after.cursor.value.item.empty()&&before.cursor.value.item.empty()&&after.player[source]==before.player[source]&&after.player[source].value.policy==0,VCF_DENIED);
 // The adapter is a transfer service. A caller cannot bypass the request
 // planner to mint quantities or alter metadata through this projection.
 auto totals=[&](const storage::State& state){std::map<Bytes,uint64_t> counts;auto add=[&](const storage::NetworkSlot& slot){if(!slot.value.item.empty())counts[slot.value.item.nbt]+=slot.value.item.count;};for(uint32_t i=0;i<36;++i)if(i!=source)add(state.player[i]);for(const auto& slot:state.storage)add(slot);return counts;};
 require(totals(before)==totals(after),VCF_DENIED);
 for(uint32_t i=0;i<36;++i)require(before.player[i].value.policy==after.player[i].value.policy,VCF_DENIED);
 for(uint32_t i=0;i<27;++i)require(before.storage[i].value.policy==after.storage[i].value.policy,VCF_DENIED);
 SavedInventory next=image;for(uint32_t i=0;i<36;++i)if(i!=source)next[i]=encode(after.player[i].value.item);
 std::array<Bytes,27> slots;for(uint32_t i=0;i<27;++i)slots[i]=encode(after.storage[i].value.item);
 next[source]=with_contents(image[source],slots);return next;
}
}
