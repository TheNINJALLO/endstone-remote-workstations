#include <oni/vcf/packet_items.hpp>
#include <cstddef>
#include <cstdint>
namespace w=oni::vcf::wire;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size){
    auto exact=[&](const auto& encoded){if(encoded.size()!=size||!std::equal(encoded.begin(),encoded.end(),data))__builtin_trap();};
    try{exact(w::encode(w::decode_content({data,size})));}catch(const oni::vcf::Error&){}
    try{exact(w::encode(w::decode_slot({data,size})));}catch(const oni::vcf::Error&){}
    try{exact(w::encode_registry(w::decode_registry({data,size})));}catch(const oni::vcf::Error&){}
    return 0;
}
