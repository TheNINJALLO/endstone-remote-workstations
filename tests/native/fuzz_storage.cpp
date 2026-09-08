#include <oni/vcf/storage.hpp>
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size){
    try{(void)oni::vcf::storage::decode({data,size});}
    catch(const oni::vcf::Error&){}
    try{
        auto parsed=oni::vcf::storage::decode_responses({data,size});
        auto encoded=oni::vcf::storage::encode_responses(parsed);
        if(encoded.size()!=size||!std::equal(encoded.begin(),encoded.end(),data))__builtin_trap();
    }catch(const oni::vcf::Error&){}
    return 0;
}
