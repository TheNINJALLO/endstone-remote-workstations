#pragma once
#include <oni/vcf/core.hpp>
#include <bit>

namespace oni::vcf::native_nbt {
// Matches the pinned Linux IDataOutput virtual interface. The Linux ABI probe
// independently checks every argument and slot. Windows must verify its own
// caller before using this interface against a native Tag.
class Interface {
public:
 virtual ~Interface()=default;
 virtual void writeString(std::string_view)=0;
 virtual void writeLongString(std::string_view)=0;
 virtual void writeFloat(float)=0;
 virtual void writeDouble(double)=0;
 virtual void writeByte(char)=0;
 virtual void writeShort(int16_t)=0;
 virtual void writeInt(int32_t)=0;
 virtual void writeLongLong(int64_t)=0;
 virtual void writeBytes(const void*,size_t)=0;
};
// Native Tag::write calls these methods on a detached save. Keeping the raw
// payload avoids losing empty list types through the public NBT converter.
class Output final:public Interface {
 std::vector<uint8_t> bytes_;size_t calls_=0;
 void room(size_t n){require(++calls_<=16384&&n<=65536&&bytes_.size()<=65536-n,VCF_CAPACITY);}
 void put(uint64_t n,unsigned width){room(width);for(unsigned i=0;i<width;++i)bytes_.push_back(static_cast<uint8_t>(n>>(8*i)));}
public:
 void writeString(std::string_view v)override{require(v.size()<=UINT16_MAX,VCF_CAPACITY);put(v.size(),2);writeBytes(v.data(),v.size());}
 void writeLongString(std::string_view v)override{require(v.size()<=65536,VCF_CAPACITY);put(v.size(),4);writeBytes(v.data(),v.size());}
 void writeFloat(float v)override{put(std::bit_cast<uint32_t>(v),4);}
 void writeDouble(double v)override{put(std::bit_cast<uint64_t>(v),8);}
 void writeByte(char v)override{put(static_cast<uint8_t>(v),1);}
 void writeShort(int16_t v)override{put(static_cast<uint16_t>(v),2);}
 void writeInt(int32_t v)override{put(static_cast<uint32_t>(v),4);}
 void writeLongLong(int64_t v)override{put(static_cast<uint64_t>(v),8);}
 void writeBytes(const void* p,size_t n)override{
  room(n);if(n){require(p);const auto*b=static_cast<const uint8_t*>(p);bytes_.insert(bytes_.end(),b,b+n);}
 }
 std::vector<uint8_t> finish(){validate_nbt(bytes_);return std::move(bytes_);}
};
}
