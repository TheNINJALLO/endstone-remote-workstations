#include "platform/linux/native_nbt.hpp"
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
using namespace oni::vcf::platform::linux_native;
namespace {
void check(bool v){if(!v)throw std::runtime_error("native NBT input assertion failed");}
template<class T>T take(Bedrock::Result<T> value){check(value.ignoreError());return std::move(value.discardError().value());}
template<class F>void refused(F f){bool no=false;try{f();}catch(const Error&){no=true;}check(no);}
}
int main(){try{
 // Exercise the actual native virtual Result<T> ABI, including typed empty
 // list payloads and signed-zero floating bits, without loading BDS.
 for(uint8_t type:{uint8_t{1},uint8_t{10}}){
  std::vector<uint8_t> raw{10,0,0,9,1,0,'x',type,0,0,0,0,0};NbtInput input(raw);IDataInput& native=input;
  check(take(native.readByteResult())==10&&take(native.readStringResult()).empty());
  check(take(native.readByteResult())==9&&take(native.readStringResult())=="x"&&take(native.readByteResult())==type&&take(native.readIntResult())==0&&take(native.readByteResult())==0&&native.numBytesLeft()==0);
  refused([&]{native.readByteResult();});
 }
 NbtOutput output;output.writeByte(10);output.writeString("");output.writeByte(6);output.writeString("d");output.writeDouble(-0.0);output.writeByte(5);output.writeString("f");output.writeFloat(-0.0f);output.writeByte(0);auto raw=output.finish();
 NbtInput input(raw);take(input.readByteResult());take(input.readStringResult());take(input.readByteResult());take(input.readStringResult());check(std::bit_cast<uint64_t>(take(input.readDoubleResult()))==0x8000000000000000ULL);
 take(input.readByteResult());take(input.readStringResult());check(std::bit_cast<uint32_t>(take(input.readFloatResult()))==0x80000000U);take(input.readByteResult());check(!input.numBytesLeft());
 refused([&]{NbtInput empty({});});std::vector<uint8_t> large(65537);refused([&]{NbtInput oversized(large);});
 for(size_t n=1;n<raw.size();++n)refused([&]{NbtInput truncated(std::span(raw).first(n));});
 std::cout<<"Pinned native input Result ABI, typed empty lists, signed zero and bounded rejection passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
