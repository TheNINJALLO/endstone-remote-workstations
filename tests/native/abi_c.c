#include <oni/vcf/abi.h>
#include <string.h>
int main(void){
 vcf_api api;memset(&api,0,sizeof(api));
 if(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api)-1,&api)!=VCF_BUFFER)return 1;
 if(oni_vcf_get_api(VCF_ABI_VERSION+1,sizeof(api),&api)!=VCF_VERSION)return 2;
 if(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api)!=VCF_OK)return 3;
 if(api.size!=sizeof(api)||!api.prepare||!api.define_rule||!api.forget)return 4;
 memset(&api,0xa5,sizeof(api));
 if(oni_vcf_get_api(VCF_ABI_VERSION_1_0,VCF_API_1_0_SIZE,&api)!=VCF_OK)return 5;
 if(api.size!=VCF_API_1_0_SIZE||api.version!=VCF_ABI_VERSION_1_0||!api.forget)return 6;
 { const unsigned char* bytes=(const unsigned char*)&api;size_t i;
   for(i=VCF_API_1_0_SIZE;i<sizeof(api);++i)if(bytes[i]!=0xa5)return 7;
 }
 memset(&api,0xa5,sizeof(api));
 if(oni_vcf_get_api(VCF_ABI_VERSION_1_1,VCF_API_1_1_SIZE,&api)!=VCF_OK)return 8;
 if(api.size!=VCF_API_1_1_SIZE||api.version!=VCF_ABI_VERSION_1_1||!api.register_guard)return 9;
 { const unsigned char* bytes=(const unsigned char*)&api;size_t i;
   for(i=VCF_API_1_1_SIZE;i<sizeof(api);++i)if(bytes[i]!=0xa5)return 10;
 }
 memset(&api,0xa5,sizeof(api));
 if(oni_vcf_get_api(VCF_ABI_VERSION_1_2,VCF_API_1_2_SIZE,&api)!=VCF_OK)return 11;
 if(api.size!=VCF_API_1_2_SIZE||api.version!=VCF_ABI_VERSION_1_2||!api.inspect_held)return 12;
 { const unsigned char* bytes=(const unsigned char*)&api;size_t i;
   for(i=VCF_API_1_2_SIZE;i<sizeof(api);++i)if(bytes[i]!=0xa5)return 13;
 }
 return 0;
}
