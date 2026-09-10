#pragma once
#include "inventory_commit.hpp"

namespace oni::vcf::inventory {
struct EditEvidence {std::string player;SavedInventory before,after;};
enum class RecoveredState { before,after,mixed,unrelated };
struct RecoveryEntry {uint64_t transaction;Boundary boundary;RecoveredState observed;};
// Plugin journal and BDS player data are distinct durability domains. A saved
// image matching either endpoint is evidence, never proof of acknowledgement.
// Every surviving edit requires explicit administrator review after restart.
class InventoryJournal {
 Ledger ledger_;
 uint64_t next_=1;
 std::map<uint64_t,EditEvidence> edits_;
 std::map<uint64_t,Boundary> stages_;
 std::set<uint64_t> quarantined_;
 static void player_id(std::string_view player){
  require(player.size()==36);bool nonzero=false;
  for(size_t i=0;i<36;++i){if(i==8||i==13||i==18||i==23)require(player[i]=='-');
   else{const auto c=player[i];require((c>='0'&&c<='9')||(c>='a'&&c<='f'));nonzero|=c!='0';}}
  require(nonzero);
 }
 static void put(std::vector<uint8_t>& out,uint64_t value,unsigned width){for(unsigned i=0;i<width;++i)out.push_back(static_cast<uint8_t>(value>>(8*i)));}
 static void image(std::vector<uint8_t>& out,const SavedInventory& value){
  for(const auto& item:value){require(item.size()<=65536,VCF_CAPACITY);validate_nbt(item);put(out,item.size(),4);out.insert(out.end(),item.begin(),item.end());require(out.size()<=256*1024,VCF_CAPACITY);}
 }
 struct Reader {
  std::span<const uint8_t> bytes;size_t at=0;
  uint64_t get(unsigned width){require(width<=bytes.size()-at,VCF_QUARANTINED);uint64_t v=0;for(unsigned i=0;i<width;++i)v|=uint64_t(bytes[at++])<<(8*i);return v;}
  std::string player(){require(36<=bytes.size()-at,VCF_QUARANTINED);std::string v(reinterpret_cast<const char*>(bytes.data()+at),36);at+=36;player_id(v);return v;}
  SavedInventory image(){SavedInventory result;for(auto& item:result){const auto n=get(4);require(n<=65536&&n<=bytes.size()-at,VCF_QUARANTINED);item.assign(bytes.begin()+at,bytes.begin()+at+n);at+=n;validate_nbt(item);}return result;}
 };
public:
 explicit InventoryJournal(const std::filesystem::path& path):ledger_(path){
  for(const auto& record:ledger_.records()){
   require(record.transaction<UINT64_MAX,VCF_CAPACITY);next_=std::max(next_,record.transaction+1);
   if(record.boundary!=Boundary::prepared){require(record.payload.empty()&&edits_.contains(record.transaction),VCF_QUARANTINED);stages_[record.transaction]=record.boundary;continue;}
   Reader reader{record.payload};require(reader.get(4)==0x31494356,VCF_QUARANTINED);const auto kind=reader.get(1);
   if(kind==1){auto player=reader.player();auto before=reader.image();auto after=reader.image();require(before!=after,VCF_QUARANTINED);
    edits_.emplace(record.transaction,EditEvidence{std::move(player),std::move(before),std::move(after)});stages_[record.transaction]=record.boundary;quarantined_.insert(record.transaction);
   }else if(kind==2){const auto target=reader.get(8);auto player=reader.player();reader.image();
    require(edits_.contains(target)&&edits_.at(target).player==player&&quarantined_.erase(target)==1,VCF_QUARANTINED);
   }else throw Error{VCF_QUARANTINED};
   require(reader.at==reader.bytes.size(),VCF_QUARANTINED);
  }
 }
 bool blocked(std::string_view player)const{for(auto tx:quarantined_)if(edits_.at(tx).player==player)return true;return false;}
 uint64_t reserve(EditEvidence evidence){
  player_id(evidence.player);require(!blocked(evidence.player),VCF_QUARANTINED);require(evidence.before!=evidence.after);
  require(next_&&next_<UINT64_MAX&&edits_.size()<16384,VCF_CAPACITY);
  // Validate and allocate before reserving a transaction or touching BDS.
  std::vector<uint8_t> probe;image(probe,evidence.before);image(probe,evidence.after);
  auto tx=next_++;edits_.emplace(tx,std::move(evidence));
  // Reserve uncertainty tracking before any native write. Recording an
  // uncertain result must not allocate after the item state has changed.
  try{quarantined_.insert(tx);}catch(...){edits_.erase(tx);throw;}
  return tx;
 }
 void record(uint64_t tx,Boundary stage){
  require(edits_.contains(tx));std::vector<uint8_t> payload;
  if(stage==Boundary::prepared){const auto& edit=edits_.at(tx);put(payload,0x31494356,4);put(payload,1,1);payload.insert(payload.end(),edit.player.begin(),edit.player.end());image(payload,edit.before);image(payload,edit.after);}
  ledger_.append(tx,stage,payload);stages_[tx]=stage;
 }
 void finish(uint64_t tx,const CommitResult& result){if(!result.uncertain)quarantined_.erase(tx);}
 std::vector<RecoveryEntry> review(std::string_view player,const SavedInventory& actual)const{
  std::vector<RecoveryEntry> result;
  for(auto tx:quarantined_){const auto& edit=edits_.at(tx);if(edit.player!=player||!stages_.contains(tx))continue;
   auto observed=RecoveredState::mixed;
   if(actual==edit.before)observed=RecoveredState::before;
   else if(actual==edit.after)observed=RecoveredState::after;
   else for(size_t i=0;i<36;++i)if(actual[i]!=edit.before[i]&&actual[i]!=edit.after[i]){observed=RecoveredState::unrelated;break;}
   result.push_back({tx,stages_.at(tx),observed});
  }return result;
 }
 // Only an explicit administrator action may call this. It acknowledges the
 // supplied current inventory; it never restores, grants or removes an item.
 void accept_current(uint64_t tx,std::string_view player,const SavedInventory& reviewed,const SavedInventory& current){
  require(quarantined_.contains(tx)&&edits_.at(tx).player==player,VCF_NOT_FOUND);require(reviewed==current,VCF_STALE);
  require(next_&&next_<UINT64_MAX,VCF_CAPACITY);std::vector<uint8_t> payload;put(payload,0x31494356,4);put(payload,2,1);put(payload,tx,8);payload.insert(payload.end(),player.begin(),player.end());image(payload,current);
  ledger_.append(next_++,Boundary::prepared,payload);quarantined_.erase(tx);
 }
};
}
