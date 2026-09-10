#pragma once
#include <oni/vcf/ledger.hpp>

namespace oni::vcf::inventory {
using SavedInventory=std::array<std::vector<uint8_t>,36>;
struct CommitResult {
 vcf_status status=VCF_OK;
 bool committed=false,restored=false,uncertain=false;
 const char* operation="complete";
};
// All callbacks are synchronous on the admitted server thread. prepare owns
// detached native before/after objects; begin/end retain the native save view.
// Guard must check lifetime/permissions/reentry, not compare the old mutation
// stamp after our own admitted setters have deliberately invalidated it.
struct CommitHost {
 std::function<void()> guard,prepare,begin,end;
 std::function<SavedInventory()> read;
 std::function<void(uint32_t,bool)> write; // false=after, true=before
 std::function<void(Boundary)> record;
};
inline CommitResult commit_inventory(const SavedInventory& before,const SavedInventory& after,const CommitHost& host){
 require(bool(host.guard)&&bool(host.prepare)&&bool(host.begin)&&bool(host.end)&&bool(host.read)&&bool(host.write)&&bool(host.record));
 require(before!=after,VCF_INVALID);
 std::vector<uint32_t> changed;for(uint32_t i=0;i<36;++i)if(before[i]!=after[i])changed.push_back(i);
 auto current=before;bool barrier=false,touched=false,published=false;const char* operation="preparing items";
 auto status=[]()noexcept -> vcf_status {try{throw;}catch(const Error&e){return e.status;}catch(...){return VCF_INTERNAL;}};
 try{
  host.guard();require(host.read()==before,VCF_STALE);host.guard();host.prepare();
  host.guard();require(host.read()==before,VCF_STALE);host.guard();
  operation="recording preparation";host.record(Boundary::prepared);host.guard();
  operation="opening save barrier";
  host.begin();barrier=true;
  operation="validating initial save view";
  require(host.read()==before,VCF_STALE);host.guard();
  for(size_t n=0;n<changed.size();++n){
   const auto slot=changed[n];host.guard();require(host.read()==current,VCF_STALE);
   // A throwing native setter may already have changed its destination.
   operation="writing inventory slot";touched=true;host.write(slot,false);current[slot]=after[slot];
   operation="validating written inventory";
   require(host.read()==current,VCF_CONFLICT);host.guard();
   if(!n)host.record(Boundary::debit);
  }
  operation="recording completed inventory";host.record(Boundary::escrow);host.guard();require(host.read()==after,VCF_CONFLICT);
  host.record(Boundary::output);host.guard();
  operation="publishing inventory";host.end();barrier=false;published=true;
  operation="recording delivery";
  host.record(Boundary::delivery);host.record(Boundary::acknowledgement);
  return {VCF_OK,true,false,false};
 }catch(...){
  const auto failure=status();
  if(published)return {VCF_QUARANTINED,true,false,true,operation};
  bool restored=!touched;
  if(touched){
   try{
    auto observed=host.read();
    // Never overwrite an unrelated item or guess which domain owns it. Every
    // slot must still be one of this exact batch's before/after alternatives.
    for(size_t i=0;i<36;++i)require(observed[i]==before[i]||observed[i]==after[i],VCF_QUARANTINED);
    for(auto it=changed.rbegin();it!=changed.rend();++it){
     if(observed[*it]==before[*it])continue;
     require(host.read()==observed,VCF_QUARANTINED);
     host.write(*it,true);observed[*it]=before[*it];require(host.read()==observed,VCF_QUARANTINED);
    }
    require(host.read()==before,VCF_QUARANTINED);restored=true;
   }catch(...){restored=false;}
  }
  if(barrier){try{host.end();}catch(...){restored=false;}}
  return {restored?failure:VCF_QUARANTINED,false,touched&&restored,!restored,operation};
 }
}
}
