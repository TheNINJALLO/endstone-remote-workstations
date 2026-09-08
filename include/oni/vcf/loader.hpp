#pragma once
#include "abi.h"
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <TlHelp32.h>
#else
#include <dlfcn.h>
#include <link.h>
#endif
namespace oni::vcf::sdk {
using GetApi=vcf_status(VCF_CALL*)(uint32_t,uint32_t,vcf_api*);
// Discover the provider's actual loaded module, including Endstone shadow copies.
// Do not LoadLibrary the original plugin file: that creates a second API owner.
inline vcf_api discover(){
 GetApi found=nullptr;bool ambiguous=false;
#ifdef _WIN32
 auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
 if(snapshot==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot enumerate native modules");
 MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
 if(Module32FirstW(snapshot,&entry))do{
  auto fn=reinterpret_cast<GetApi>(GetProcAddress(entry.hModule,"oni_vcf_get_api"));
  if(fn){if(found&&found!=fn)ambiguous=true;found=fn;}
 }while(Module32NextW(snapshot,&entry));
 CloseHandle(snapshot);
#else
 struct Search{GetApi function=nullptr;bool ambiguous=false;} search;
 dl_iterate_phdr([](dl_phdr_info*info,size_t,void*context){
  if(!info->dlpi_name||!*info->dlpi_name)return 0;
  auto module=dlopen(info->dlpi_name,RTLD_NOW|RTLD_NOLOAD);if(!module)return 0;
  auto fn=reinterpret_cast<GetApi>(dlsym(module,"oni_vcf_get_api"));
  auto&s=*static_cast<Search*>(context);
  if(fn){if(s.function&&s.function!=fn)s.ambiguous=true;s.function=fn;}
  dlclose(module);return 0;
 },&search);
 found=search.function;ambiguous=search.ambiguous;
#endif
 if(!found||ambiguous)throw std::runtime_error("Exactly one loaded VCF provider is required");
 vcf_api api{};if(found(VCF_ABI_VERSION,sizeof(api),&api)!=VCF_OK)throw std::runtime_error("Incompatible VCF C ABI");return api;
}
}
