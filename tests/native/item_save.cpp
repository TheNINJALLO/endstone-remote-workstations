#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include "native_nbt_output.hpp"
#include <cstring>
#include <iostream>
#include <limits>

using namespace oni::vcf;
namespace {
void check(bool value){if(!value)throw std::runtime_error("saved item check failed");}
template<class F>void refused(vcf_status expected,F&& f){try{f();throw std::runtime_error("expected refusal");}catch(const Error&e){check(e.status==expected);}}
std::vector<uint8_t> fixture(){
 native_nbt::Output output;native_nbt::Interface& out=output;out.writeByte(10);out.writeString("");
 out.writeByte(9);out.writeString("empty");out.writeByte(8);out.writeInt(0);
 out.writeByte(2);out.writeString("aux");out.writeShort(-123);
 out.writeByte(4);out.writeString("long");out.writeLongLong(INT64_MIN);
 out.writeByte(5);out.writeString("float");out.writeFloat(-0.0f);
 out.writeByte(6);out.writeString("double");out.writeDouble(-0.0);
 out.writeByte(7);out.writeString("bytes");out.writeInt(3);const uint8_t bytes[]={0,128,255};out.writeBytes(bytes,3);
 out.writeByte(11);out.writeString("ints");out.writeInt(1);out.writeInt(INT32_MIN);
 out.writeByte(0);return output.finish();
}
void encoding(){
 const auto bytes=fixture();validate_nbt(bytes);
 // The type of the empty list remains String (8), not End (0).
 check(bytes[11]==8&&bytes[12]==0&&bytes[13]==0&&bytes[14]==0&&bytes[15]==0);
 auto changed=bytes;changed[11]=0;validate_nbt(changed);check(bytes!=changed);
 for(size_t i=1;i<bytes.size();++i)refused(VCF_INVALID,[&]{validate_nbt(std::span(bytes).first(i));});
 native_nbt::Output large;std::vector<uint8_t> payload(65536);large.writeBytes(payload.data(),payload.size());
 refused(VCF_CAPACITY,[&]{large.writeByte(0);});
 native_nbt::Output wide;refused(VCF_CAPACITY,[&]{wide.writeString(std::string(65536,'x'));});
 native_nbt::Output calls;for(size_t i=0;i<16384;++i)calls.writeBytes(nullptr,0);refused(VCF_CAPACITY,[&]{calls.writeBytes(nullptr,0);});
 native_nbt::Output null;refused(VCF_INVALID,[&]{null.writeBytes(nullptr,1);});
 native_nbt::Output nan;nan.writeByte(10);nan.writeString("");nan.writeByte(5);nan.writeString("x");nan.writeFloat(std::numeric_limits<float>::quiet_NaN());nan.writeByte(0);
 refused(VCF_INVALID,[&]{nan.finish();});
}
void api(){
 bool allowed=true,enabled=true,revoke=false,malformed=false;int calls=0;
 Host host;host.consumer_allowed=[&](auto){return enabled;};
 host.permission=[&](auto,auto permission){return permission!="remoteworkstations.inventory.read"||allowed;};
 host.inspect_held=[](auto){vcf_held_info info{};std::strcpy(info.canonical_id,"shulker");return info;};
 auto bytes=fixture();host.read_inventory_item=[&](auto,uint32_t slot){
  ++calls;if(slot==35)throw Error{VCF_NOT_FOUND};if(revoke)enabled=false;
  InventoryItemSnapshot result;result.nbt=bytes;auto&i=result.info;i.slot=slot;i.amount=1;i.nbt_format=VCF_BDS_ITEM_SAVE_NBT;i.nbt_bytes=static_cast<uint32_t>(bytes.size());
  std::strcpy(i.identifier,"minecraft:bundle");if(malformed)i.nbt_bytes=0;return result;
 };
 Engine engine(host);attach_engine(&engine);vcf_api table{};check(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(table),&table)==VCF_OK);
 sdk::Client client(table,"saved_reader");auto saved=client.read_inventory_item("player",19);check(saved.nbt==bytes&&saved.info.slot==19&&engine.session_count()==0);
 // An existing 1.2 binary keeps its held call and sees its own output version.
 vcf_api old{};check(oni_vcf_get_api(VCF_ABI_VERSION_1_2,VCF_API_1_2_SIZE,&old)==VCF_OK);
 auto descriptor=sdk::descriptor<vcf_consumer_desc>();descriptor.version=VCF_ABI_VERSION_1_2;descriptor.name=sdk::view("old_saved_reader");
 vcf_handle older=0;check(old.register_consumer(&descriptor,&older)==VCF_OK);
 auto held=sdk::descriptor<vcf_held_info>();held.version=VCF_ABI_VERSION_1_2;check(old.inspect_held(older,sdk::view("player"),&held)==VCF_OK&&held.version==VCF_ABI_VERSION_1_2);
 check(old.unregister_consumer(older)==VCF_OK);
 auto info=sdk::descriptor<vcf_inventory_item_info>();info.slot=99;const auto initial=info;uint32_t required=77;
 std::array<uint8_t,512> buffer;buffer.fill(0xa5);const auto untouched=buffer;
 check(table.read_inventory_item(client.owner(),sdk::view("player"),19,&info,nullptr,0,&required)==VCF_BUFFER&&required==bytes.size());
 check(std::memcmp(&info,&initial,sizeof(info))==0);
 required=77;check(table.read_inventory_item(client.owner(),sdk::view("player"),19,&info,buffer.data(),1,&required)==VCF_BUFFER&&required==bytes.size()&&buffer==untouched);
 auto invoke=[&](uint32_t slot=19){required=77;return table.read_inventory_item(client.owner(),sdk::view("player"),slot,&info,buffer.data(),buffer.size(),&required);};
 auto unchanged=[&]{check(std::memcmp(&info,&initial,sizeof(info))==0&&buffer==untouched&&required==77);};
 allowed=false;const auto previous=calls;check(invoke()==VCF_DENIED&&calls==previous);unchanged();allowed=true;
 check(invoke(36)==VCF_INVALID);unchanged();check(invoke(35)==VCF_NOT_FOUND);unchanged();
 malformed=true;check(invoke()==VCF_INTERNAL);unchanged();malformed=false;
 revoke=true;check(invoke()==VCF_CLOSED);unchanged();revoke=false;enabled=true;
 vcf_status status=VCF_OK;std::thread worker([&]{status=invoke();});worker.join();check(status==VCF_WRONG_THREAD);unchanged();
 info.version=VCF_ABI_VERSION_1_2;check(invoke()==VCF_VERSION&&required==77&&buffer==untouched);info=initial;
 check(invoke()==VCF_OK&&required==bytes.size()&&info.nbt_bytes==bytes.size());
 check(std::equal(bytes.begin(),bytes.end(),buffer.begin())&&buffer[bytes.size()]==0xa5);
 const auto owner=client.owner();check(client.dispose()==VCF_OK);check(table.read_inventory_item(owner,sdk::view("player"),19,&info,buffer.data(),buffer.size(),&required)==VCF_NOT_FOUND);
 engine.shutdown();attach_engine(nullptr);
 // A supported C ABI may expose this function on a platform with no admitted
 // native serializer. It must fail closed without calling a guessed adapter.
 Host missing;missing.consumer_allowed=[](auto){return true;};missing.permission=[](auto,auto){return true;};Engine unavailable(missing);attach_engine(&unavailable);
 sdk::Client reader(table,"no_adapter");check(table.read_inventory_item(reader.owner(),sdk::view("player"),0,&info,buffer.data(),buffer.size(),&required)==VCF_UNAVAILABLE);
 check(reader.dispose()==VCF_OK);unavailable.shutdown();attach_engine(nullptr);
}
}
int main(){try{encoding();api();std::cout<<"Native save byte preservation, bounded output, permissions, revocation, caller buffers and unavailable adapters passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
