#include "runtime.hpp"
#include "linux_identity.hpp"
#include <openssl/evp.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace oni::vcf::platform {
namespace {
struct File {
 int fd=-1;
 explicit File(const char* path,int flags=O_RDONLY|O_CLOEXEC):fd(open(path,flags)) {
  if(fd<0)throw std::runtime_error("Cannot open loaded Linux module for fingerprinting");
 }
 ~File(){if(fd>=0)close(fd);}
 File(const File&)=delete;
 File& operator=(const File&)=delete;
};
bool same_file(const struct stat&a,const struct stat&b){
 return a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_size==b.st_size&&
  a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&
  a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
}
// Verify the actual mapped inode at the exported server entry point. A replaced
// pathname must not allow a different loaded image to inherit its digest.
bool mapped_inode_matches(const void* address,const struct stat&file){
 std::ifstream maps("/proc/self/maps");if(!maps)return false;
 const auto location=reinterpret_cast<uintptr_t>(address);
 for(std::string line;std::getline(maps,line);){
  unsigned long begin=0,end=0,offset=0,inode=0;unsigned int dev_major=0,dev_minor=0;char permissions[5]{};
  if(std::sscanf(line.c_str(),"%lx-%lx %4s %lx %x:%x %lu",&begin,&end,permissions,&offset,&dev_major,&dev_minor,&inode)!=7)continue;
  if(location>=begin&&location<end)return permissions[2]=='x'&&inode==file.st_ino&&dev_major==major(file.st_dev)&&dev_minor==minor(file.st_dev);
 }
 return false;
}
}
namespace linux_detail {
std::string fingerprint_fd(int fd){
 struct stat before{},after{};
 if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<0||before.st_size>1024LL*1024*1024||lseek(fd,0,SEEK_SET)<0)
  throw std::runtime_error("Invalid or oversized Linux module file");
 std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> hash(EVP_MD_CTX_new(),EVP_MD_CTX_free);
 if(!hash||EVP_DigestInit_ex(hash.get(),EVP_sha256(),nullptr)!=1)throw std::runtime_error("SHA256 initialization failed");
 std::array<unsigned char,65536> buffer{};uint64_t bytes=0;
 for(;;){
  const auto n=read(fd,buffer.data(),buffer.size());
  if(n<0){if(errno==EINTR)continue;throw std::runtime_error("Linux module read failed");}
  if(n==0)break;
  bytes+=static_cast<uint64_t>(n);
  if(bytes>static_cast<uint64_t>(before.st_size)||EVP_DigestUpdate(hash.get(),buffer.data(),static_cast<size_t>(n))!=1)
   throw std::runtime_error("Linux module changed or hashing failed");
 }
 if(bytes!=static_cast<uint64_t>(before.st_size)||fstat(fd,&after)||!same_file(before,after))
  throw std::runtime_error("Linux module changed while fingerprinting");
 std::array<unsigned char,EVP_MAX_MD_SIZE> out{};unsigned int length=0;
 if(EVP_DigestFinal_ex(hash.get(),out.data(),&length)!=1||length!=32)throw std::runtime_error("SHA256 finalization failed");
 constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
 for(unsigned int i=0;i<length;++i){result+=hex[out[i]>>4];result+=hex[out[i]&15];}
 return result;
}
}
Admission inspect_runtime(){
 Admission a;
 try{
  File executable("/proc/self/exe");a.bds_sha256=linux_detail::fingerprint_fd(executable.fd);
  const auto* entry=dlsym(RTLD_DEFAULT,"endstone_get_server");Dl_info runtime{};
  if(!entry||!dladdr(entry,&runtime)||!runtime.dli_fname||std::filesystem::path(runtime.dli_fname).filename()!="libendstone_runtime.so"){
   a.reason="Exact loaded Linux Endstone server entry point not found; Onistone requires independent admission.";return a;
  }
  File loader(runtime.dli_fname,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);struct stat identity{};
  if(fstat(loader.fd,&identity)||!mapped_inode_matches(entry,identity)){
   a.reason="Linux loader pathname does not match the mapped runtime inode.";return a;
  }
  a.runtime_sha256=linux_detail::fingerprint_fd(loader.fd);
  a.accepted=a.bds_sha256=="8ba803f23d681816495c7ac83bdba4b9cd7165a3bee5aedd18fa0c8c3d408ec2"&&
   a.runtime_sha256=="ac665adb20c9d5c640da9e88de956d728f6dd7771a4461ce8205824ec9b300bc";
  a.reason=a.accepted?"Exact Linux Endstone/BDS files admitted for the public SDK; private UI adapters and gameplay remain unqualified.":
   "Unknown Linux BDS or loader fingerprint; provider activation refused.";
 }catch(const std::exception&e){a.reason=e.what();}
 return a;
}
bool pin_provider(){
 Dl_info own{};
 if(!dladdr(reinterpret_cast<const void*>(&pin_provider),&own)||!own.dli_fname)return false;
 // Promote only this already-loaded module. Never load an original plugin path
 // alongside Endstone's shadow copy. Outstanding callbacks retain valid code.
 auto* module=dlopen(own.dli_fname,RTLD_NOW|RTLD_NOLOAD|RTLD_NODELETE);
 if(!module)return false;
 return dlclose(module)==0;
}
}
