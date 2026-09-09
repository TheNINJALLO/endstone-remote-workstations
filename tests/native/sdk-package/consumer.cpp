#include <oni/vcf/sdk.hpp>
#include <oni/vcf/loader.hpp>
#include <string_view>
int main() {
    auto descriptor=oni::vcf::sdk::descriptor<vcf_session_desc>();
    if(descriptor.size!=sizeof(descriptor)||descriptor.version!=VCF_ABI_VERSION)return 1;
    auto value=oni::vcf::sdk::view("craft");
    if(std::string_view(value.data,value.length)!="craft")return 2;
    // This independent executable has no provider linked or loaded. Discovery
    // must refuse, rather than loading a second provider or a core archive.
    try { (void)oni::vcf::sdk::discover(); }
    catch(const std::runtime_error& error) {
        return std::string_view(error.what())=="Exactly one loaded VCF provider is required"?0:3;
    }
    return 4;
}
