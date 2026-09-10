#pragma once
#include "bedrock/nbt/list_tag.h"
#include "../../native_nbt_output.hpp"
#include <cstring>

namespace oni::vcf::platform::linux_native {
// Actual pinned native Result<T> owns ErrorInfo/CallStack as well as success
// values. A byte-array imitation cannot safely destroy the error alternative.
static_assert(sizeof(Bedrock::Result<std::unique_ptr<::Tag>>)==72);
static_assert(sizeof(Bedrock::Result<std::string>)==72);
static_assert(sizeof(Bedrock::Result<void>)==72);
static_assert(sizeof(::ListTag)==40);
class NbtInput final:public IDataInput {
 std::span<const uint8_t> bytes_;size_t offset_=0,calls_=0;
 uint64_t take(unsigned width){
  require(++calls_<=16384&&width<=bytes_.size()-offset_,VCF_CAPACITY);
  uint64_t value=0;for(unsigned i=0;i<width;++i)value|=uint64_t(bytes_[offset_++])<<(8*i);return value;
 }
 std::string string(unsigned width){
  const auto length=take(width);require(length<=bytes_.size()-offset_,VCF_CAPACITY);
  std::string value(reinterpret_cast<const char*>(bytes_.data()+offset_),length);offset_+=length;return value;
 }
public:
 explicit NbtInput(std::span<const uint8_t> bytes):bytes_(bytes){require(!bytes.empty()&&bytes.size()<=65536,VCF_CAPACITY);validate_nbt(bytes);}
 Bedrock::Result<std::string> readStringResult()override{return string(2);}
 Bedrock::Result<std::string> readLongStringResult()override{return string(4);}
 Bedrock::Result<float> readFloatResult()override{return std::bit_cast<float>(static_cast<uint32_t>(take(4)));}
 Bedrock::Result<double> readDoubleResult()override{return std::bit_cast<double>(take(8));}
 Bedrock::Result<uint8_t> readByteResult()override{return static_cast<uint8_t>(take(1));}
 Bedrock::Result<int16_t> readShortResult()override{return std::bit_cast<int16_t>(static_cast<uint16_t>(take(2)));}
 Bedrock::Result<int32_t> readIntResult()override{return std::bit_cast<int32_t>(static_cast<uint32_t>(take(4)));}
 Bedrock::Result<int64_t> readLongLongResult()override{return std::bit_cast<int64_t>(take(8));}
 Bedrock::Result<void> readBytesResult(void* target,uint64_t size)override{
  require(++calls_<=16384&&size<=bytes_.size()-offset_,VCF_CAPACITY);require(!size||target);
  if(size)std::memcpy(target,bytes_.data()+offset_,size);offset_+=size;return {};
 }
 size_t numBytesLeft()const override{return bytes_.size()-offset_;}
};
class NbtOutput final:public IDataOutput {
 native_nbt::Output output_;
public:
 void writeString(std::string_view v)override{output_.writeString(v);}
 void writeLongString(std::string_view v)override{output_.writeLongString(v);}
 void writeFloat(float v)override{output_.writeFloat(v);}
 void writeDouble(double v)override{output_.writeDouble(v);}
 void writeByte(char v)override{output_.writeByte(v);}
 void writeShort(int16_t v)override{output_.writeShort(v);}
 void writeInt(int32_t v)override{output_.writeInt(v);}
 void writeLongLong(int64_t v)override{output_.writeLongLong(v);}
 void writeBytes(const void* p,size_t n)override{output_.writeBytes(p,n);}
 std::vector<uint8_t> finish(){return output_.finish();}
};
}
