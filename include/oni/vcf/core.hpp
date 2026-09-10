#pragma once
#include "abi.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace oni::vcf {
struct Error { vcf_status status; };
inline void require(bool yes, vcf_status code=VCF_INVALID) { if (!yes) throw Error{code}; }
struct CatalogEntry {
 std::string_view id, family, permission, aliases, source_kind;
 uint32_t capacity;
 bool privileged;
};
std::span<const CatalogEntry> catalog();
const CatalogEntry* resolve(std::string_view);
void validate_nbt(std::span<const uint8_t>);
struct Item {
 std::string id;
 uint32_t count=0, limit=64;
 std::vector<uint8_t> nbt;
 bool empty() const { return count==0; }
 bool matches(const Item& b) const { return id==b.id && nbt==b.nbt && limit==b.limit; }
 bool operator==(const Item&) const = default;
};
struct Slot { Item item; uint32_t policy=VCF_INSERT|VCF_EXTRACT; bool operator==(const Slot&) const = default; };
struct Move { uint32_t from, to, count; };
struct Rule {
 std::string id; uint64_t revision=1, stock=1;
 std::vector<std::pair<uint32_t,Item>> costs;
 uint32_t output_slot=0, duration_ticks=0;
 Item output;
};
struct Snapshot {
 std::vector<Slot> slots; uint64_t revision=0;
 void transfer(std::span<const Move>);
 void execute(Rule&, uint64_t expected_rule_revision);
};
struct Button { std::string label,action,icon; };
struct ActionInfo { std::string name,consumer,permission; bool exported; uint64_t revision; };
struct Session {
 vcf_handle id=0,owner=0;
 std::string kind,player,title,permission,dimension,entity_id,held_id;
 int32_t x=0,y=0,z=0; uint32_t mode=VCF_REAL_SOURCE,state=VCF_PREPARING;
 vcf_status result=VCF_OK; uint64_t generation=0;
 vcf_callback callback=nullptr; void* context=nullptr;
 Snapshot inventory; std::map<std::string,Rule> rules;
 std::string content; std::vector<Button> buttons;
 bool is_menu=false, host_started=false;
};
struct InventoryItemSnapshot {vcf_inventory_item_info info{};std::vector<uint8_t> nbt;};
struct InventoryEdit {uint32_t slot;std::vector<uint8_t> expected,replacement;};
struct Host {
 std::function<bool(std::string_view)> consumer_allowed;
 std::function<bool(std::string_view,std::string_view)> permission;
 std::function<uint32_t(std::string_view)> item_limit;
 std::function<bool(std::string_view)> native_available;
 std::function<vcf_status(const Session&)> open;
 std::function<vcf_status(const Session&)> close;
 std::function<vcf_held_info(std::string_view)> inspect_held;
 std::function<InventoryItemSnapshot(std::string_view,uint32_t)> read_inventory_item;
 std::function<std::function<void()>(std::string_view,uint32_t)> observe_inventory_item;
 std::function<void(std::string_view,std::span<const InventoryEdit>,std::function<void()>,std::function<void()>)> apply_inventory_edit;
};
class Engine:public std::enable_shared_from_this<Engine> {
public:
 explicit Engine(Host host);
 vcf_status guard() const;
 vcf_handle consumer(std::string,uint32_t version=VCF_ABI_VERSION);
 void release(vcf_handle);
 void release_named(std::string_view);
 void action(vcf_handle,std::string,std::string,bool,vcf_callback,void*);
 void remove_action(vcf_handle,std::string_view);
 std::vector<ActionInfo> actions(vcf_handle) const;
 uint64_t action_revision()const{return action_revision_;}
 vcf_handle add_guard(vcf_handle,std::string,std::string,vcf_guard_callback,void*);
 void remove_guard(vcf_handle,vcf_handle);
 void authorize(vcf_handle,vcf_handle,uint32_t);
 // Provider-only authorization of another verified block in one source.
 // The callback descriptor is copied; the session's original source is kept.
 void authorize_block(vcf_handle,vcf_handle,uint32_t,int32_t,int32_t,int32_t);
 vcf_held_info inspect_held(vcf_handle,std::string_view);
 InventoryItemSnapshot read_inventory_item(vcf_handle,std::string_view,uint32_t);
 vcf_handle observe_inventory_item(vcf_handle,std::string_view,uint32_t);
 void validate_inventory_observation(vcf_handle,vcf_handle);
 void release_inventory_observation(vcf_handle,vcf_handle);
 void apply_inventory_edit(vcf_handle,vcf_handle,std::span<const InventoryEdit>);
 uint32_t collect_terminal(vcf_handle,uint32_t limit=32);
 vcf_handle invoke(vcf_handle,std::string,std::string);
 vcf_handle prepare(vcf_handle,Session);
 void preload(vcf_handle,vcf_handle,uint32_t,Item,uint32_t);
 void rule(vcf_handle,vcf_handle,Rule);
 void open(vcf_handle,vcf_handle);
 void close(vcf_handle,vcf_handle);
 void forget(vcf_handle,vcf_handle);
 Session& session(vcf_handle,vcf_handle);
 Item item(const vcf_item&) const;
 void tick(uint32_t budget=64);
 void selected(vcf_handle,std::string_view,uint32_t);
 void player_gone(std::string_view);
 void opened(vcf_handle,vcf_handle,vcf_status);
 void committed(vcf_handle,vcf_handle,uint64_t revision);
 void retired(vcf_handle,vcf_handle,vcf_status);
 void shutdown();
 uint32_t session_count() const { return static_cast<uint32_t>(sessions_.size()); }
 bool native_available(std::string_view id)const{return host_.native_available&&host_.native_available(id);}
private:
 struct Consumer { std::string name; uint32_t version=VCF_ABI_VERSION; bool revoked=false; };
 struct Action { vcf_handle owner; std::string permission; bool exported; vcf_callback callback; void* context; };
 struct Guard { vcf_handle owner; std::string name,kind; vcf_guard_callback callback; void* context; };
 enum class TaskType { open, close, invoke };
 struct Task { TaskType type; vcf_handle session; std::string action; };
 Host host_; std::thread::id thread_; bool alive_=true, dispatching_=false; uint32_t callbacks_=0;
 vcf_handle next_=1;
 std::map<vcf_handle,Consumer> consumers_;
 std::map<std::string,Action> actions_;
 std::map<vcf_handle,Guard> guards_;
 uint64_t action_revision_=1;
 std::map<vcf_handle,Session> sessions_;
 struct Observation {vcf_handle owner;std::string player;std::function<void()> validate;vcf_status failure=VCF_OK;};
 std::map<vcf_handle,Observation> observations_;
 bool observing_=false;
 std::deque<Task> queue_;
 std::set<vcf_handle> revoked_;
 void drain_revoked();
 void authorize_at(vcf_handle,vcf_handle,uint32_t,const int32_t*);
 void check() const;
 void inventory_permission(vcf_handle,std::string_view)const;
 std::string qualify(vcf_handle,std::string_view) const;
 void emit(Session&,uint32_t,std::string_view={});
 void finish(Session&,vcf_status);
};
void attach_engine(Engine*);
}
