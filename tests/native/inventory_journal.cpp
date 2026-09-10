#include "inventory_journal.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace oni::vcf;
using namespace oni::vcf::inventory;
namespace {
void check(bool v){if(!v)throw std::runtime_error("inventory recovery assertion failed");}
constexpr std::string_view player="12345678-1234-1234-1234-123456789abc";
std::vector<uint8_t> item(uint8_t value){return {10,0,0,1,1,0,'x',value,0};}
void exercise(const std::filesystem::path& path,unsigned boundary){
 EditEvidence plan;plan.player=player;plan.before[0]=item(1);plan.before[19]=item(2);plan.after=plan.before;std::swap(plan.after[0],plan.after[19]);uint64_t tx=0;
 {
  InventoryJournal journal(path);tx=journal.reserve(plan);
  for(unsigned i=1;i<=boundary;++i)journal.record(tx,static_cast<Boundary>(i));
  journal.finish(tx,{VCF_OK,true,false,false});check(!journal.blocked(player));
 }
 {
  InventoryJournal journal(path);check(journal.blocked(player));
  auto old=journal.review(player,plan.before);check(old.size()==1&&old[0].transaction==tx&&old[0].boundary==static_cast<Boundary>(boundary)&&old[0].observed==RecoveredState::before);
  auto after=journal.review(player,plan.after);check(after[0].observed==RecoveredState::after);
  auto mixed=plan.before;mixed[0]=plan.after[0];check(journal.review(player,mixed)[0].observed==RecoveredState::mixed);
  auto unrelated=plan.before;unrelated[35]=item(3);check(journal.review(player,unrelated)[0].observed==RecoveredState::unrelated);
  bool denied=false;try{journal.reserve(plan);}catch(const Error&e){denied=e.status==VCF_QUARANTINED;}check(denied);
  denied=false;try{journal.accept_current(tx,player,plan.before,plan.after);}catch(const Error&e){denied=e.status==VCF_STALE;}check(denied&&journal.blocked(player));
  // Explicit acceptance records the reviewed state without modifying it.
  const auto unchanged=unrelated;journal.accept_current(tx,player,unrelated,unrelated);check(!journal.blocked(player)&&unrelated==unchanged);
 }
 {InventoryJournal reopened(path);check(!reopened.blocked(player));auto second=reopened.reserve(plan);check(second>tx);reopened.record(second,Boundary::prepared);}
 {InventoryJournal reopened(path);check(reopened.blocked(player)&&reopened.review(player,plan.before).size()==1);}
}
}
int main(){try{
 auto root=std::filesystem::current_path()/("inventory-recovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
 for(unsigned boundary=1;boundary<=6;++boundary)exercise(root/(std::to_string(boundary)+".vcf"),boundary);
 std::cout<<"Six durable recovery stages, endpoint/mixed classification, explicit acceptance and restart persistence passed\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}catch(const Error&e){std::cerr<<"Unexpected status "<<e.status<<'\n';return 2;}}
