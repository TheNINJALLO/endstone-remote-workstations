#include <oni/vcf/ledger.hpp>
#include <cstdlib>
#include <iostream>
int main(int argc,char**argv){
 if(argc!=4)return 1;
 try{
  auto step=std::stoi(argv[3]);if(step<1||step>6)return 2;
  oni::vcf::Ledger ledger(argv[2]);
  if(std::string_view(argv[1])=="write"){
   for(int n=1;n<=step;n++)ledger.append(900,static_cast<oni::vcf::Boundary>(n),{});
   std::_Exit(88); // Bypass destructors after the selected durable boundary.
  }
  if(std::string_view(argv[1])!="verify")return 3;
  if(ledger.records().size()!=static_cast<size_t>(step)||ledger.quarantined()!=std::vector<uint64_t>{900})return 4;
  std::cout<<"Fresh-process recovery: boundary "<<step<<" quarantined; no item issuance.\n";
  return 0;
 }catch(...){return 5;}
}
