#include "inventory_commit.hpp"
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
using namespace oni::vcf::inventory;
namespace {
void check(bool v){if(!v)throw std::runtime_error("inventory commit assertion failed");}
struct Fixture {
 SavedInventory before{},after{},live{},saved{};
 CommitHost host;
 bool barrier=false,allowed=true,prepared=false;
 unsigned writes=0,rollbacks=0,ends=0;
 int fail_stage=0,fail_write=0;bool throw_after=false,foreign=false,fail_rollback=false;
 std::vector<Boundary> records;
 Fixture(){
  // Model three indivisible saved stacks, including an empty destination.
  before[2]={1,2,3};before[19]={4,5,6};after=before;after[2].clear();after[19]=before[2];after[35]=before[19];live=before;
  host.guard=[&]{require(allowed,VCF_DENIED);};host.read=[&]{return live;};
  host.prepare=[&]{prepared=true;};host.begin=[&]{check(prepared&&!barrier);barrier=true;};
  host.end=[&]{check(barrier);barrier=false;++ends;};
  host.write=[&](uint32_t slot,bool rollback){
   check(barrier&&prepared);
   if(rollback){++rollbacks;if(fail_rollback)throw Error{VCF_INTERNAL};live[slot]=before[slot];return;}
   ++writes;if(int(writes)==fail_write&&!throw_after)throw Error{VCF_INTERNAL};live[slot]=after[slot];
   // A native save reentered between setters must only see the baseline.
   saved=barrier?before:live;check(saved==before);
   if(foreign)live[0]={99};
   if(int(writes)==fail_write&&throw_after)throw Error{VCF_INTERNAL};
  };
  host.record=[&](Boundary boundary){if(int(boundary)==fail_stage)throw Error{VCF_INTERNAL};records.push_back(boundary);};
 }
 CommitResult run(){return commit_inventory(before,after,host);}
};
void successful(){Fixture f;auto r=f.run();check(r.status==VCF_OK&&r.committed&&!r.restored&&!r.uncertain);check(f.live==f.after&&!f.barrier&&f.writes==3&&f.ends==1&&f.records.size()==6);}
void persistence_failures(){
 for(int stage=1;stage<=6;++stage){Fixture f;f.fail_stage=stage;auto r=f.run();check(!f.barrier);
  if(stage>=5)check(r.status==VCF_QUARANTINED&&r.committed&&r.uncertain&&f.live==f.after&&f.rollbacks==0);
  else check(r.status==VCF_INTERNAL&&!r.committed&&!r.uncertain&&f.live==f.before&&r.restored==(stage>1));
 }
}
void setters(){
 for(int n=1;n<=3;++n)for(bool after:{false,true}){Fixture f;f.fail_write=n;f.throw_after=after;auto r=f.run();check(r.status==VCF_INTERNAL&&!r.committed&&r.restored&&!r.uncertain&&f.live==f.before&&!f.barrier);}
 Fixture bad;bad.fail_write=2;bad.fail_rollback=true;auto r=bad.run();check(r.status==VCF_QUARANTINED&&r.uncertain&&!r.restored&&!bad.barrier);
 Fixture foreign;foreign.foreign=true;r=foreign.run();check(r.status==VCF_QUARANTINED&&r.uncertain&&foreign.rollbacks==0&&foreign.live[0]==std::vector<uint8_t>{99});
}
void authorization(){
 Fixture revoked;revoked.allowed=false;auto r=revoked.run();check(r.status==VCF_DENIED&&!r.uncertain&&revoked.writes==0&&!revoked.prepared);
 Fixture prepared;prepared.host.prepare=[&]{prepared.live[0]={88};};r=prepared.run();check(r.status==VCF_STALE&&prepared.writes==0&&prepared.records.empty()&&prepared.live[0]==std::vector<uint8_t>{88});
 Fixture middle;auto writer=middle.host.write;middle.host.write=[&](uint32_t slot,bool rollback){writer(slot,rollback);if(!rollback)middle.allowed=false;};r=middle.run();check(r.status==VCF_DENIED&&r.restored&&middle.live==middle.before);
 Fixture journal;auto record=journal.host.record;journal.host.record=[&](Boundary b){record(b);journal.allowed=false;};r=journal.run();check(r.status==VCF_DENIED&&journal.writes==0&&journal.ends==0);
}
}
int main(){try{successful();persistence_failures();setters();authorization();std::cout<<"Native inventory commit admission, reentrant save contract, rollback and uncertain recovery passed\n";}
 catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
