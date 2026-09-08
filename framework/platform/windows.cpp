#include "runtime.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <filesystem>
#include <stdexcept>
namespace oni::vcf::platform {
namespace {
std::string digest(HMODULE module){
 wchar_t path[32768]{};auto n=GetModuleFileNameW(module,path,32768);if(!n||n>=32768)throw std::runtime_error("module path");
 std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f)throw std::runtime_error("module file");
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("SHA256 provider");
 if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(alg,0);throw std::runtime_error("SHA256 state");}
 std::array<unsigned char,65536> buffer;std::array<unsigned char,32> out{};
 bool ok=true;while(f){f.read(reinterpret_cast<char*>(buffer.data()),buffer.size());auto count=f.gcount();if(count>0&&BCryptHashData(hash,buffer.data(),static_cast<ULONG>(count),0)<0){ok=false;break;}}
 ok=ok&&f.eof()&&BCryptFinishHash(hash,out.data(),static_cast<ULONG>(out.size()),0)>=0;BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(alg,0);
 if(!ok)throw std::runtime_error("module hash");
 constexpr char hex[]="0123456789abcdef";std::string result;for(auto b:out){result+=hex[b>>4];result+=hex[b&15];}return result;
}
}
Admission inspect_runtime(){
 Admission a;try{
  auto runtime=GetModuleHandleW(L"endstone_runtime.dll");if(!runtime){a.reason="Exact Endstone runtime module was not found; Onistone needs an independent admission.";return a;}
  a.bds_sha256=digest(GetModuleHandleW(nullptr));a.runtime_sha256=digest(runtime);
  a.accepted=a.bds_sha256=="92d09c7b74ac6a9805bafc166d8e0a13ac9e5db73dbbb0819e5a14093699d44f"&&a.runtime_sha256=="0c6f0861c5f9a677058b25776d975654a3586a80d2e4421b069b5e98f536f819";
  a.reason=a.accepted?"Exact Windows Endstone/BDS fingerprints admitted; per-feature qualification is separate.":"Unknown Windows BDS or loader runtime; native operations refused.";
 }catch(const std::exception&e){a.reason=e.what();}return a;
}
bool pin_provider(){
 HMODULE h=nullptr;
 return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&pin_provider),&h)!=0;
}
}
