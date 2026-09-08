#include <oni/vcf/sdk.hpp>
#include <oni/vcf/core.hpp>
#include <iostream>
int main(){
 oni::vcf::Host host;host.consumer_allowed=[](auto){return true;};
 oni::vcf::Engine engine(host);oni::vcf::attach_engine(&engine);
 vcf_api api{};oni::vcf::sdk::checked(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api),&api));
 oni::vcf::sdk::Client client(api,"catalog_showcase");
 uint32_t count=0;oni::vcf::sdk::checked(api.catalog_count(&count));
 for(uint32_t n=0;n<count;++n){
  auto c=client.capability(n);std::string id(c.id.data,c.id.length);
  if(client.resolve(id)!=n)return 1;
  std::cout<<id<<": "<<std::string(c.reason.data,c.reason.length)<<'\n';
 }
 return 0;
}
