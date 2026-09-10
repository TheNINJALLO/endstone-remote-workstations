#include "native_items.hpp"
#include "native_nbt.hpp"
#include "inventory_watch.hpp"
#include "../runtime.hpp"
#include "../../native_nbt_output.hpp"
#include <endstone/player.h>
#include <endstone/inventory/item_stack.h>
#include <openssl/evp.h>
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <typeinfo>

namespace oni::vcf::platform::linux_native {
namespace {
std::array<uint8_t,32> digest(std::span<const uint8_t> bytes){
 std::array<uint8_t,32> result{};unsigned length=0;
 require(EVP_Digest(bytes.data(),bytes.size(),result.data(),&length,EVP_sha256(),nullptr)==1&&length==result.size(),VCF_INTERNAL);return result;
}
struct Memory {
 struct Region{uintptr_t first,last;bool executable;};std::vector<Region> regions;
 Memory(){
  std::ifstream input("/proc/self/maps");require(bool(input),VCF_UNAVAILABLE);
  for(std::string line;std::getline(input,line);){unsigned long first,last;char mode[5]{};
   if(std::sscanf(line.c_str(),"%lx-%lx %4s",&first,&last,mode)==3&&mode[0]=='r'){
    require(regions.size()<16384,VCF_CAPACITY);regions.push_back({first,last,mode[2]=='x'});
   }
  }
 }
 void readable(uintptr_t p,size_t n,bool executable=false)const{
  if(p&&n<=UINTPTR_MAX-p)for(const auto&r:regions)if(p>=r.first&&p+n<=r.last&&(!executable||r.executable))return;
  throw Error{VCF_UNAVAILABLE};
 }
 template<class T>T field(uintptr_t p,size_t offset=0)const{
  require(offset<=UINTPTR_MAX-p,VCF_UNAVAILABLE);readable(p+offset,sizeof(T));T value;std::memcpy(&value,reinterpret_cast<void*>(p+offset),sizeof(value));return value;
 }
 void function(uintptr_t address,size_t n,std::string_view expected)const{
  readable(address,n,true);auto bytes=digest({reinterpret_cast<const uint8_t*>(address),n});
  constexpr char hex[]="0123456789abcdef";std::array<char,64> actual{};
  for(size_t i=0;i<bytes.size();++i){actual[i*2]=hex[bytes[i]>>4];actual[i*2+1]=hex[bytes[i]&15];}
  require(std::string_view(actual.data(),actual.size())==expected,VCF_UNAVAILABLE);
 }
};
// Linux ELF FDE extents, native callers and independently compiled SDK layouts.
// These are not the legacy Windows held-save addresses or calling convention.
struct Function{uintptr_t rva;size_t size;std::string_view hash;};
constexpr Function functions[]={
 {0xbbb02d0,639,"ba11246949e9c0d6f2bf2402ccb58cab25bbeea0e1e0ce71a601a61804073cc3"},
 {0xbbb8150,4973,"c017b34a1fc37a0385bfbd6f7e7901bde494949c371eedfa2bbe064c6dbd11e6"},
 {0x453b4b0,88,"90502c2c3f10be8ed481673d3a950e2b88cb7f3fb45ac061693d277f91727884"},
 {0xbcfe480,58,"d01f44d8ced8eb9c28a77dce95eb49e378a3a010022ae9cd9db95c7b264ecc19"},
 {0xbcfe4d0,25,"65513748ca370dd8ed6c201e9da617af53dfb2ff1c14b174e8537de4fb8ee4f9"},
 {0xcc80d10,41,"fdf893165ae75742edbcaba1c541922011f4e2af3e2492cfffd4468ede1237b7"},
 {0xcc80d40,7,"c0aaa941b4b78a3377f57e1761480054a12af422e6930a97444acbea8c2661b7"},
 {0xcc80d50,368,"e5693d2fb42e1f8571f65edc8bf188c9ff0af0c21b5adec9b0967707df98dec4"},
 {0xcc873d0,184,"5358309eb5b0a885e8633af1f8352cf2fb17d4babbf457f706652ae644a7515b"},
 {0xb7af9b0,1319,"02ac6a6a60d3e13bec57fdd4a4a0f6cd534bcada1967cdaef9aff1bc4dd9470c"}
};
struct Bridge {
 uintptr_t bedrock=0,runtime=0;
 Bridge(){
  require(inspect_runtime().accepted,VCF_UNAVAILABLE);
  dl_iterate_phdr([](dl_phdr_info*i,size_t,void*p){if(!i->dlpi_name||!*i->dlpi_name){*static_cast<uintptr_t*>(p)=i->dlpi_addr;return 1;}return 0;},&bedrock);
  Dl_info info{};auto* entry=dlsym(RTLD_DEFAULT,"endstone_get_server");require(entry&&dladdr(entry,&info),VCF_UNAVAILABLE);
  runtime=reinterpret_cast<uintptr_t>(info.dli_fbase);require(bedrock&&runtime,VCF_UNAVAILABLE);
 }
 void verify()const{verify_inventory_item_save_hook();Memory m;
  m.function(runtime+0x2309a0,2139,"b953a81a95bf7bb8e1dde8d3bee511943ccf4b78f45067efae690d638f7251c8");
  m.function(runtime+0x22d3c0,27,"b92972dadf1a6fb688e703925c2eb7c0436304c9707fbbad40f0f0382a421fa0");
  m.function(runtime+0x22d3e0,45,"99433b680d1a088d08b541d8f3f33f6ab81cbf355e3e6fba7036a659f0c21705");
  m.function(runtime+0x22b460,189,"09781d63b6a048d51809bdd9d4944aa59183769cbf5434429fd1b8454b532d43");
  m.function(runtime+0x1d1590,71,"3095b4a1041f68a76329df764c77b493503910691580c3ec6b429c54507d7f27");for(const auto&f:functions)m.function(bedrock+f.rva,f.size,f.hash);
  m.function(runtime+0x1d0640,23,"c85a764d6d04dd4e07c1197b8559008e00291cb6a5a6c4b2082a7de7eddfd4b3");
 }
 uintptr_t container(endstone::Player& player)const{
  require(player.isValid()&&!player.isDead(),VCF_CLOSED);auto& inv=player.getInventory();require(inv.getSize()==36,VCF_UNAVAILABLE);
  require(std::string_view(typeid(inv).name())=="N8endstone4core23EndstonePlayerInventoryE",VCF_UNAVAILABLE);
  Memory m;const auto wrapper=reinterpret_cast<uintptr_t>(&inv);const auto table=m.field<uintptr_t>(wrapper);require(table>=16&&wrapper>=8,VCF_UNAVAILABLE);
  require(m.field<intptr_t>(table-16)==-16,VCF_UNAVAILABLE);
  const auto native_player=m.field<uintptr_t>(wrapper,8),native=m.field<uintptr_t>(wrapper-8);m.readable(native,352);
  require(m.field<uintptr_t>(native)==bedrock+0xe64d2e0&&m.field<uintptr_t>(native,344)==native_player,VCF_UNAVAILABLE);
  require(m.field<uintptr_t>(bedrock+0xe64d2e0,8*8)==bedrock+0xbcfe480
   &&m.field<uintptr_t>(bedrock+0xe64d2e0,21*8)==bedrock+0xbcfe4d0,VCF_UNAVAILABLE);
  require(reinterpret_cast<int(*)(const void*)>(bedrock+0xbcfe4d0)(reinterpret_cast<void*>(native))==36,VCF_UNAVAILABLE);return native;
 }
 std::vector<uint8_t> save(uintptr_t native)const{
  uintptr_t tag=0;const uint8_t context=0; // independently verified SaveUseCase::SaveToDisk
  // System V: hidden unique_ptr result first, native ItemStackBase second.
  reinterpret_cast<void(*)(uintptr_t*,const void*,const uint8_t*)>(bedrock+0xbbb7810)(&tag,reinterpret_cast<void*>(native),&context);
  Memory m;require(m.field<uintptr_t>(tag)==bedrock+0xe7af340,VCF_UNAVAILABLE);
  require(m.field<uintptr_t>(bedrock+0xe7af340,8)==bedrock+0xcc80d10
   &&m.field<uintptr_t>(bedrock+0xe7af340,24)==bedrock+0xcc80d40,VCF_UNAVAILABLE);
  // Only this admitted native CompoundTag can reach the native deleting
  // destructor. Linux takes one this argument; no Windows deletion flags.
  struct Owner{uintptr_t tag,destroy;~Owner(){reinterpret_cast<void(*)(void*)>(destroy)(reinterpret_cast<void*>(tag));}} owner{tag,bedrock+0xcc80d10};
  native_nbt::Output output;output.writeByte(10);output.writeString("");
  reinterpret_cast<void(*)(const void*,native_nbt::Interface*)>(bedrock+0xcc80d40)(reinterpret_cast<void*>(tag),&output);
  return output.finish();
 }
 uintptr_t slot_address(uintptr_t container,uint32_t slot)const{
  const auto item=reinterpret_cast<uintptr_t(*)(const void*,int)>(bedrock+0xbcfe480)(reinterpret_cast<void*>(container),static_cast<int>(slot));
  Memory m;m.readable(item,152);require(m.field<uintptr_t>(item)==bedrock+0xe6add70,VCF_UNAVAILABLE);
  return item;
 }
 std::vector<uint8_t> slot(uintptr_t container,uint32_t slot)const{
  const auto item=slot_address(container,slot);Memory m;
  if(!m.field<uint8_t>(item,34))return {};return save(item);
 }
};
}

struct NativeItems::Impl:Bridge {};
NativeItem::~NativeItem(){if(destructor_)reinterpret_cast<void(*)(void*)>(destructor_)(storage_.data());}
NativeItems::NativeItems():impl_(std::make_shared<Impl>()){}
void NativeItems::verify()const{impl_->verify();}
uintptr_t NativeItems::container(endstone::Player& p)const{return impl_->container(p);}
uintptr_t NativeItems::slot_address(uintptr_t inventory,uint32_t index)const{require(index<36);return impl_->slot_address(inventory,index);}
std::vector<uint8_t> NativeItems::save(uintptr_t item)const{return impl_->save(item);}
std::vector<uint8_t> NativeItems::slot(uintptr_t inventory,uint32_t index)const{require(index<36);return impl_->slot(inventory,index);}
std::unique_ptr<NativeItem> NativeItems::reconstruct(std::span<const uint8_t> bytes)const{
 verify();constexpr std::array<uint8_t,4> empty{10,0,0,0};const auto source=bytes.empty()?std::span<const uint8_t>(empty):bytes;
 NbtInput input(source);std::string name;
 // The independently compiled Result<unique_ptr<CompoundTag>> has the same
 // single-pointer success payload and 72-byte error union as unique_ptr<Tag>.
 // The native parser supplies the CompoundTag vtable; destruction dispatches
 // through its native virtual destructor. No plugin-constructed NBT enters BDS.
 using Parse=Bedrock::Result<std::unique_ptr<::Tag>>(*)(IDataInput&,std::string&);
 auto result=reinterpret_cast<Parse>(impl_->runtime+0x2309a0)(input,name);
 require(result.ignoreError(),VCF_INVALID);require(name.empty()&&input.numBytesLeft()==0);
 auto tag=std::move(result.discardError().value());require(tag!=nullptr,VCF_INVALID);
 // NbtIo allocates an Endstone-owned CompoundTag. The BDS serializer above
 // returns a BDS-owned one instead. Their verified layouts match; their code
 // and vtables do not. Keep allocator/destructor admission separate.
 Memory m;const auto parsed_table=impl_->runtime+0xac30e8;
 require(m.field<uintptr_t>(reinterpret_cast<uintptr_t>(tag.get()))==parsed_table,VCF_UNAVAILABLE);
 require(m.field<uintptr_t>(parsed_table,8)==impl_->runtime+0x22d3e0&&m.field<uintptr_t>(parsed_table,24)==impl_->runtime+0x22b460,VCF_UNAVAILABLE);
 NbtOutput output;output.writeByte(10);output.writeString("");tag->write(output);
 const auto encoded=output.finish();require(std::equal(encoded.begin(),encoded.end(),source.begin(),source.end()),VCF_INVALID);
 auto native=std::unique_ptr<NativeItem>(new NativeItem);
 reinterpret_cast<void(*)(void*,const ::Tag*)>(impl_->bedrock+0xbbb02d0)(native->storage_.data(),tag.get());
 native->destructor_=impl_->bedrock+0x453b4b0;
 require(m.field<uintptr_t>(native->address())==impl_->bedrock+0xe6add70,VCF_UNAVAILABLE);
 if(bytes.empty())require(m.field<uint8_t>(native->address(),34)==0&&m.field<uintptr_t>(native->address(),8)==0,VCF_INVALID);
 else{const auto saved=save(native->address());require(std::equal(saved.begin(),saved.end(),bytes.begin(),bytes.end()),VCF_INVALID);}
 return native;
}
endstone::ItemStack NativeItems::copy(const NativeItem& item)const{
 auto result=reinterpret_cast<endstone::ItemStack(*)(const void*)>(impl_->runtime+0x1d1590)(reinterpret_cast<const void*>(item.address()));
 require(save(result)==save(item.address()),VCF_UNAVAILABLE);return result;
}
std::vector<uint8_t> NativeItems::save(const endstone::ItemStack& item)const{
 Memory m;const auto wrapper=m.field<uintptr_t>(reinterpret_cast<uintptr_t>(&item));m.readable(wrapper,136);
 require(m.field<uintptr_t>(wrapper)==impl_->runtime+0xabf5b0,VCF_UNAVAILABLE);
 require(item.getAmount()>0&&item.getAmount()<=255&&m.field<uint8_t>(wrapper+8,34)==item.getAmount(),VCF_UNAVAILABLE);
 return save(wrapper+8);
}
}
