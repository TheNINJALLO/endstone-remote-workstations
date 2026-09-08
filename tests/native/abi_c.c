#include <oni/vcf/abi.h>
#include <string.h>
int main(void){
 vcf_api api;memset(&api,0,sizeof(api));
 if(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api)-1,&api)!=VCF_BUFFER)return 1;
 if(oni_vcf_get_api(VCF_ABI_VERSION+1,sizeof(api),&api)!=VCF_VERSION)return 2;
 if(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)!=VCF_OK)return 3;
 if(api.size!=sizeof(api)||!api.prepare||!api.define_rule||!api.forget)return 4;
 return 0;
}
