#include "linux_identity.hpp"
#include "runtime.hpp"
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

void require(bool ok){if(!ok)throw std::runtime_error("Linux identity check failed");}
int main(int argc,char**argv){
 require(argc==2);
 char path[]="/tmp/vcf-identity-XXXXXX";int fd=mkstemp(path);require(fd>=0);unlink(path);
 try{
  using oni::vcf::platform::linux_detail::fingerprint_fd;
  require(fingerprint_fd(fd)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  require(write(fd,"abc",3)==3);
  require(fingerprint_fd(fd)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  require(ftruncate(fd,0)==0&&lseek(fd,0,SEEK_SET)==0);
  std::string million(1000000,'a');size_t done=0;
  while(done<million.size()){auto n=write(fd,million.data()+done,million.size()-done);require(n>0);done+=static_cast<size_t>(n);}
  require(fingerprint_fd(fd)=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  bool refused=false;try{fingerprint_fd(-1);}catch(const std::exception&){refused=true;}require(refused);
  require(ftruncate(fd,1024LL*1024*1024+1)==0);
  refused=false;try{fingerprint_fd(fd);}catch(const std::exception&){refused=true;}require(refused);
  int pipes[2];require(pipe(pipes)==0);refused=false;
  try{fingerprint_fd(pipes[0]);}catch(const std::exception&){refused=true;}
  close(pipes[0]);close(pipes[1]);require(refused);
  const auto admission=oni::vcf::platform::inspect_runtime();
  require(!admission.accepted&&!admission.reason.empty()&&admission.bds_sha256.size()==64);
  char directory[]="/tmp/vcf-module-XXXXXX";require(mkdtemp(directory)!=nullptr);
  const auto module_path=std::filesystem::path(directory)/"libendstone_runtime.so";
  const auto archived=std::filesystem::path(directory)/"loaded.so";
  std::filesystem::copy_file(argv[1],module_path);
  auto* module=dlopen(module_path.c_str(),RTLD_NOW|RTLD_GLOBAL);require(module!=nullptr);
  auto fake=oni::vcf::platform::inspect_runtime();
  require(!fake.accepted&&fake.runtime_sha256.size()==64&&fake.reason.starts_with("Unknown Linux"));
  std::filesystem::rename(module_path,archived);
  {std::ofstream replacement(module_path);replacement<<"Replaced pathname must not identify mapped code";}
  auto replaced=oni::vcf::platform::inspect_runtime();
  require(!replaced.accepted&&replaced.reason.find("mapped runtime inode")!=std::string::npos);
  std::filesystem::remove(module_path);std::filesystem::rename(archived,module_path);
  auto pin=reinterpret_cast<bool(*)()>(dlsym(module,"vcf_test_pin"));require(pin&&pin());
  require(dlclose(module)==0);
  auto* retained=dlopen(module_path.c_str(),RTLD_NOW|RTLD_NOLOAD);require(retained!=nullptr);
  require(pin());require(dlclose(retained)==0);
  // These two exact test-created paths are the only cleanup targets.
  std::filesystem::remove(module_path);std::filesystem::remove(directory);
  close(fd);std::puts("Linux SHA256 vectors, streaming/bounds, fake runtime refusal, mapped inode replacement and callback code pinning passed.");
 }catch(...){close(fd);throw;}
}
