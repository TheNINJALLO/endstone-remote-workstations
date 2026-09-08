#include <oni/vcf/core.hpp>
#include <cstddef>
#include <cstdint>
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size) {
    try { oni::vcf::validate_nbt({data,size}); }
    catch(const oni::vcf::Error&) {}
    return 0;
}
