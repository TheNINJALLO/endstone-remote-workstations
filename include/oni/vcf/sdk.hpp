#pragma once
#include "abi.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace oni::vcf::sdk {
inline vcf_string view(std::string_view value){return {value.data(),static_cast<uint32_t>(value.size())};}
inline void checked(vcf_status s){if(s!=VCF_OK)throw std::runtime_error("VCF status "+std::to_string(s));}
template<class T>T descriptor(){T t{};t.size=sizeof(T);t.version=VCF_ABI_VERSION;return t;}
class Client {
 const vcf_api* api_;vcf_handle owner_=0;
public:
 Client(const vcf_api&api,std::string_view name):api_(&api){
  auto d=descriptor<vcf_consumer_desc>();d.name=view(name);checked(api_->register_consumer(&d,&owner_));
 }
 Client(const Client&)=delete;Client&operator=(const Client&)=delete;
 ~Client(){if(owner_)api_->unregister_consumer(owner_);}
 // Explicit dispose is required before unloading. Destructor cannot report a
 // reentrant refusal; consumers must observe this status on their owner thread.
 vcf_status dispose(){auto s=owner_?api_->unregister_consumer(owner_):VCF_OK;if(s==VCF_OK)owner_=0;return s;}
 vcf_handle owner()const{return owner_;}
 const vcf_api& api()const{return *api_;}
 uint32_t resolve(std::string_view id)const{uint32_t n=0;checked(api_->resolve(view(id),&n));return n;}
 vcf_capability capability(uint32_t n)const{auto c=descriptor<vcf_capability>();checked(api_->capability(n,&c));return c;}
 vcf_handle prepare(std::string_view player,std::string_view kind,uint32_t mode,std::string_view title=""){
  auto d=descriptor<vcf_session_desc>();d.player=view(player);d.canonical_id=view(kind);d.mode=mode;d.title=view(title);vcf_handle s=0;checked(api_->prepare(owner_,&d,&s));return s;
 }
 void set(vcf_handle session,uint32_t slot,std::string_view id,uint32_t count,uint32_t policy=VCF_INSERT|VCF_EXTRACT){
  auto item=descriptor<vcf_item>();item.identifier=view(id);item.count=count;checked(api_->set_item(owner_,session,slot,&item,policy));
 }
 void open(vcf_handle s){checked(api_->open(owner_,s));}
 void close(vcf_handle s){checked(api_->close(owner_,s));}
 vcf_session_info info(vcf_handle s)const{auto d=descriptor<vcf_session_info>();checked(api_->session_info(owner_,s,&d));return d;}
 void action(std::string_view name,vcf_callback callback,void*context=nullptr,std::string_view permission="",bool exported=false){
  auto d=descriptor<vcf_action_desc>();d.name=view(name);d.permission=view(permission);d.exported=exported;d.callback=callback;d.context=context;checked(api_->register_action(owner_,&d));
 }
 vcf_handle invoke(std::string_view player,std::string_view action){vcf_handle s=0;checked(api_->invoke(owner_,view(player),view(action),&s));return s;}
};
}
