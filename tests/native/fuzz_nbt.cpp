#include <oni/vcf/core.hpp>
#include <cstddef>
#include <cstdint>
#include "../../framework/saved_item.hpp"
#include "../../framework/held_input.hpp"
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size) {
    try { oni::vcf::validate_nbt({data,size}); }
    catch(const oni::vcf::Error&) {}
    namespace saved=oni::vcf::inventory::saved;
    auto limit=[](std::string_view){return 64u;};
    try {
        const auto item=saved::decode({data,size},limit);
        if(saved::decode(saved::encode(item),limit)!=item)__builtin_trap();
    } catch(const oni::vcf::Error&) {}
    try {
        const auto slots=saved::contents({data,size});
        const auto bytes=saved::with_contents({data,size},slots);
        if(bytes.size()!=size||!std::equal(bytes.begin(),bytes.end(),data))__builtin_trap();
    } catch(const oni::vcf::Error&) {}
    try {oni::vcf::held::inventory_input({data,size});} catch(const oni::vcf::Error&) {}
    try {oni::vcf::held::player_action({data,size});} catch(const oni::vcf::Error&) {}
    return 0;
}
