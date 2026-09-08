#pragma once
#include "core.hpp"
#include <queue>
namespace oni::vcf {
// Pure bounded scheduler. It never ticks world blocks or grants offline time.
// The owning adapter must journal/publish snapshots before acknowledging them.
class Machines {
 struct Machine {Snapshot state;Rule rule;uint64_t due,epoch;bool blocked=false;};
 struct Due {uint64_t tick,id,epoch;bool operator>(const Due&b)const{return tick>b.tick;}};
 std::map<uint64_t,Machine> machines_;
 std::priority_queue<Due,std::vector<Due>,std::greater<Due>> due_;
 uint64_t next_=1,last_tick_=0;
public:
 uint64_t add(Snapshot state,Rule rule,uint64_t now){
  require(rule.duration_ticks>0&&rule.duration_ticks<=72000);
  require(machines_.size()<1024 && now<=UINT64_MAX-rule.duration_ticks,VCF_CAPACITY);
  auto id=next_++;auto at=now+rule.duration_ticks;machines_.emplace(id,Machine{std::move(state),std::move(rule),at,1});due_.push({at,id,1});return id;
 }
 void remove(uint64_t id){machines_.erase(id);}
 const Snapshot& state(uint64_t id)const{return machines_.at(id).state;}
 bool blocked(uint64_t id)const{return machines_.at(id).blocked;}
 uint32_t tick(uint64_t now,uint32_t budget=64){
  require(now>=last_tick_);last_tick_=now;uint32_t done=0;
  while(!due_.empty()&&due_.top().tick<=now&&done<std::min(budget,256u)){
   auto d=due_.top();due_.pop();++done;auto it=machines_.find(d.id);if(it==machines_.end()||it->second.epoch!=d.epoch)continue;auto&m=it->second;
   try{m.state.execute(m.rule,m.rule.revision);m.blocked=false;}catch(const Error&){m.blocked=true;}
   // One execution per processing interval. No unbounded restart catch-up.
   if(m.rule.stock && now<=UINT64_MAX-m.rule.duration_ticks){m.due=now+m.rule.duration_ticks;due_.push({m.due,d.id,m.epoch});}
  }
  return done;
 }
};
}
