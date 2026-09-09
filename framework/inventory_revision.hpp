#pragma once
#include <oni/vcf/core.hpp>
#include <mutex>

namespace oni::vcf::inventory {
// An optimistic lease observes mutations; it does not freeze vanilla inventory.
// Keep it alive from capture through admission and compare the full saved bytes
// as well. Revisions also invalidate an A -> B -> A change with identical bytes.
class Revision {
public:
 struct Stamp {
  uint64_t revision=0,request_revision=0;
  std::array<uint64_t,36> slots{};
  bool operator==(const Stamp&)const=default;
 };
 enum class Kind { slot,request };
private:
 mutable std::mutex mutex_;
 std::thread::id thread_;
 Stamp stamp_{};
 uint32_t depth_=0;
 bool retired_=false,poisoned_=false;
 void advance(uint64_t& value)noexcept {
  if(value==UINT64_MAX)poisoned_=true;else ++value;
 }
 void touch(Kind kind,int slot)noexcept {
  advance(stamp_.revision);
  if(kind==Kind::request)advance(stamp_.request_revision);
  else if(slot>=0&&slot<36)advance(stamp_.slots[static_cast<size_t>(slot)]);
  // Invalid/whole-container slot notifications still advance the global stamp.
 }
public:
 explicit Revision(std::thread::id thread=std::this_thread::get_id()):thread_(thread){}
 void begin(Kind kind,int slot=-1)noexcept {
  std::lock_guard lock(mutex_);
  if(std::this_thread::get_id()!=thread_)poisoned_=true;
  if(depth_==UINT32_MAX)poisoned_=true;else ++depth_;
  touch(kind,slot);
 }
 void end(Kind kind,int slot=-1)noexcept {
  std::lock_guard lock(mutex_);
  if(std::this_thread::get_id()!=thread_||!depth_)poisoned_=true;
  if(depth_)--depth_;
  touch(kind,slot);
 }
 void retire()noexcept {std::lock_guard lock(mutex_);retired_=true;}
 void poison()noexcept {std::lock_guard lock(mutex_);poisoned_=true;}
 Stamp capture()const {
  require(std::this_thread::get_id()==thread_,VCF_WRONG_THREAD);
  std::lock_guard lock(mutex_);
  require(!retired_,VCF_CLOSED);require(!poisoned_,VCF_QUARANTINED);
  require(!depth_,VCF_REENTRANT);return stamp_;
 }
 void validate(const Stamp& expected)const {require(capture()==expected,VCF_STALE);}
};
// End runs during exception unwinding, so a failed native request also stales
// an old lease and never leaves the watch permanently inside a mutation.
class Mutation {
 std::shared_ptr<Revision> revision_;Revision::Kind kind_;int slot_;
public:
 Mutation(std::shared_ptr<Revision> revision,Revision::Kind kind,int slot=-1)noexcept
  :revision_(std::move(revision)),kind_(kind),slot_(slot){if(revision_)revision_->begin(kind_,slot_);}
 ~Mutation(){if(revision_)revision_->end(kind_,slot_);}
 Mutation(const Mutation&)=delete;Mutation&operator=(const Mutation&)=delete;
};
}
