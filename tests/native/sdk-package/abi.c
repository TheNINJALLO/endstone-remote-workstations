#include <oni/vcf/abi.h>
int main(void) {
    vcf_api api = {0};
    api.size = sizeof(api);
    api.version = VCF_ABI_VERSION;
    return api.size == 0 || api.version == 0;
}
