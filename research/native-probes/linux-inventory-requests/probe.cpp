namespace RakNet {struct RakPeerConfiguration;}
#include "bedrock/nbt/list_tag.h"
#include "bedrock/world/item/save_context.h"
#include "bridge.hpp"
#include <endstone/event/player/player_quit_event.h>
#include <atomic>
#include <sys/syscall.h>
#include <unistd.h>

namespace {
using Save=std::unique_ptr<::ListTag>(*)(const void*,const SaveContext&);
using Changed=void(*)(void*,int);
using Set=void(*)(void*,int,const void*,bool);
using Process=void(*)(void*);
struct State {
 Save save{};Changed changed{};Set set{};
 Process process{};uintptr_t manager_vtable{};
 std::atomic<uintptr_t> watched_manager{};
 std::atomic<uint64_t> request_calls{},request_batches{},request_revision{},request_depth{};
 std::atomic<uintptr_t> watched{};
 std::atomic<uint64_t> saves{},changes{},sets{},watched_saves{},watched_changes{},watched_sets{},off_thread{};
 std::array<std::atomic<uint64_t>,36> epochs{};
 pid_t main_thread{};
 void observe(const void* p,int slot,std::atomic<uint64_t>& total,std::atomic<uint64_t>& matched) noexcept {
  total.fetch_add(1,std::memory_order_relaxed);
  if(reinterpret_cast<uintptr_t>(p)!=watched.load(std::memory_order_acquire))return;
  matched.fetch_add(1,std::memory_order_relaxed);
  if(syscall(SYS_gettid)!=main_thread)off_thread.fetch_add(1,std::memory_order_relaxed);
  if(slot>=0&&slot<36)epochs[slot].fetch_add(1,std::memory_order_relaxed);
 }
};
// Process lifetime storage and RTLD_NODELETE keep callbacks and trampolines
// valid after plugin disable. No callback dereferences a watched player.
State* state=nullptr;
std::unique_ptr<::ListTag> saved(const void* p,const SaveContext& context){
 state->observe(p,-1,state->saves,state->watched_saves);
 return state->save(p,context);
}
void changed(void* p,int slot){state->observe(p,slot,state->changes,state->watched_changes);state->changed(p,slot);}
void set(void* p,int slot,const void* item,bool balance){state->observe(p,slot,state->sets,state->watched_sets);state->set(p,slot,item,balance);}
void processed(void* manager){
 state->request_calls.fetch_add(1,std::memory_order_relaxed);
 // Native callers pass one this pointer. The original immediately reads its
 // queue size at144; validate the exact manager class before observing it.
 uintptr_t vt=0;std::memcpy(&vt,manager,8);uint64_t count=0;
 if(vt==state->manager_vtable)std::memcpy(&count,static_cast<const uint8_t*>(manager)+144,8);
 const bool matched=count&&reinterpret_cast<uintptr_t>(manager)==state->watched_manager.load(std::memory_order_acquire);
 if(matched){state->request_batches.fetch_add(1);state->request_revision.fetch_add(1);state->request_depth.fetch_add(1);if(syscall(SYS_gettid)!=state->main_thread)state->off_thread.fetch_add(1);}
 struct Exit{bool active;~Exit(){if(active){state->request_revision.fetch_add(1);state->request_depth.fetch_sub(1);}}} exit{matched};
 state->process(manager);
}
}
class HookProbe:public endstone::Plugin {
 std::unique_ptr<Bridge> bridge;uintptr_t save_entry{};unsigned sequence=0;std::string watched_id;json patches;
 uintptr_t container(endstone::Player& p){
  require(p.isValid()&&!p.isDead()&&p.hasPermission("vcf_request_probe.admin"),VCF_DENIED);
  auto& inv=p.getInventory();require(inv.getSize()==36&&std::string_view(typeid(inv).name())=="N8endstone4core23EndstonePlayerInventoryE");
  Memory m;auto w=reinterpret_cast<uintptr_t>(&inv),vt=m.get<uintptr_t>(w);require(m.get<intptr_t>(vt-16)==-16);
  auto native=m.get<uintptr_t>(w-8);m.check(native,352);require(m.get<uintptr_t>(native)==bridge->bds+0xe64d2e0&&m.get<uintptr_t>(native,344)==m.get<uintptr_t>(w,8));
  require(m.get<uintptr_t>(bridge->bds+0xe64d2e0,51*8)==save_entry);
  require(m.get<uintptr_t>(bridge->bds+0xe64d2e0,8*8)==bridge->fn(0xbcfe480));return native;
 }
 void verify_patches(){for(const auto& row:patches){auto p=row.at("address").get<uintptr_t>();auto n=row.at("size").get<size_t>();Memory{}.check(p,n,true);require(hash({reinterpret_cast<const uint8_t*>(p),n})==row.at("sha256").get<std::string>(),VCF_UNAVAILABLE);}}
 std::vector<uint8_t> snapshot(uintptr_t native){
  verify_patches();uintptr_t output=0;uint8_t context=0;
  reinterpret_cast<void(*)(uintptr_t*,const void*,const uint8_t*)>(save_entry)(&output,reinterpret_cast<void*>(native),&context);
  auto owned=bridge->owned(output,true);return bridge->bytes(owned,true);
 }
 json counters(){json e=json::array();for(auto& v:state->epochs)e.push_back(v.load());return {{"all_saves",state->saves.load()},{"all_changes",state->changes.load()},{"all_sets",state->sets.load()},{"watched_saves",state->watched_saves.load()},{"watched_changes",state->watched_changes.load()},{"watched_sets",state->watched_sets.load()},{"off_thread",state->off_thread.load()},{"slot_epochs",e},{"watch_active",state->watched.load()!=0},{"manager_watch_active",state->watched_manager.load()!=0},{"request_calls",state->request_calls.load()},{"request_batches",state->request_batches.load()},{"request_revision",state->request_revision.load()},{"request_depth",state->request_depth.load()}};}
 void report(std::string_view action,json extra=json::object()){
  json r={{"sequence",++sequence},{"action",action},{"counters",counters()},{"details",extra},{"save_suppression",false}};
  std::ofstream out(getDataFolder()/("report-"+std::to_string(sequence)+".json"));out<<r.dump(2)<<'\n';require(bool(out));
 }
public:
 void onEnable()override{
  require(std::filesystem::is_regular_file("/data/.vcf-linux-fixture-v1")&&getServer().getOnlinePlayers().empty());
  require(!state&&getServer().isPrimaryThread());bridge=std::make_unique<Bridge>(getDataFolder()/"fingerprints.json");
  auto create=reinterpret_cast<void*(*)()>(bridge->fn(8529696,true));
  auto prepare=reinterpret_cast<int(*)(void*,void**,void*)>(bridge->fn(8530464,true));
  auto install=reinterpret_cast<int(*)(void*,int)>(bridge->fn(8531568,true));
  auto error=reinterpret_cast<const char*(*)(const void*)>(bridge->fn(8532256,true));
  std::array<uintptr_t,4> entries{bridge->fn(0xbcfdea0),bridge->fn(0xade2e00),bridge->fn(0xb3cb4e0),bridge->fn(0xbcd29f0)};
  for(auto rva:{0xbcd2700,0xbcd4ce0,0xbcd5960})bridge->fn(rva);
  bridge->fn(0x1c6500,true);save_entry=entries[0];
  Dl_info module{};require(dladdr(reinterpret_cast<void*>(&saved),&module)&&module.dli_fname);
  require(dlopen(module.dli_fname,RTLD_NOW|RTLD_LOCAL|RTLD_NODELETE),VCF_UNAVAILABLE);
  state=new State;state->main_thread=static_cast<pid_t>(syscall(SYS_gettid));
  state->manager_vtable=bridge->bds+0xe6c65f8;
  auto handle=create();require(handle,VCF_UNAVAILABLE);
  std::array<void*,4> trampolines{reinterpret_cast<void*>(entries[0]),reinterpret_cast<void*>(entries[1]),reinterpret_cast<void*>(entries[2]),reinterpret_cast<void*>(entries[3])};
  std::array<void*,4> detours{reinterpret_cast<void*>(&saved),reinterpret_cast<void*>(&changed),reinterpret_cast<void*>(&set),reinterpret_cast<void*>(&processed)};
  for(size_t i=0;i<4;++i)if(prepare(handle,&trampolines[i],detours[i])!=0)throw std::runtime_error(error(handle));
  state->save=reinterpret_cast<Save>(trampolines[0]);state->changed=reinterpret_cast<Changed>(trampolines[1]);state->set=reinterpret_cast<Set>(trampolines[2]);
  state->process=reinterpret_cast<Process>(trampolines[3]);
  if(install(handle,0)!=0)throw std::runtime_error(error(handle));
  for(auto entry:entries)patches.push_back({{"address",entry},{"size",32},{"sha256",hash({reinterpret_cast<const uint8_t*>(entry),32})}});
  registerEvent(&HookProbe::quit,*this);report("enabled");
  getLogger().info("Private native inventory hooks ready: save/change/set forwarded; no save suppression or automatic item writes.");
 }
 void quit(endstone::PlayerQuitEvent& e){if(e.getPlayer().getUniqueId().str()==watched_id){state->watched_manager.store(0,std::memory_order_release);state->watched.store(0,std::memory_order_release);report("quit");watched_id.clear();}}
 void onDisable()override{if(state){state->watched_manager.store(0,std::memory_order_release);state->watched.store(0,std::memory_order_release);report("disabled");}}
 bool onCommand(endstone::CommandSender& sender,const endstone::Command&,const std::vector<std::string>& args)override{
  try{
   require(!sender.asPlayer()&&getServer().isPrimaryThread()&&args.size()==1);verify_patches();const auto& action=args[0];
   if(action=="status"){report(action);sender.sendMessage("Private hook status: saves {}, changes {}, sets {}; watched {}/{}/{}.",state->saves.load(),state->changes.load(),state->sets.load(),state->watched_saves.load(),state->watched_changes.load(),state->watched_sets.load());return true;}
   if(action=="unwatch"){state->watched_manager.store(0,std::memory_order_release);state->watched.store(0,std::memory_order_release);watched_id.clear();report(action);sender.sendMessage("Private hook watch cleared.");return true;}
   auto players=getServer().getOnlinePlayers();require(players.size()==1);auto& p=*players.front();auto native=container(p);
   if(action=="watch"){
    require(state->watched.load()==0&&state->request_depth.load()==0);
    const auto wrapper=reinterpret_cast<uintptr_t>(&p.getInventory());const auto native_player=Memory{}.get<uintptr_t>(wrapper,8);
    // Actual Player header compiled independently in layout.cpp gives2560.
    // Including that header in a loadable probe emits unrelated SDK globals
    // needing hidden RakNet constructors, so only the verified field ABI is used.
    Memory{}.check(native_player,2568);
    const auto manager=Memory{}.get<uintptr_t>(native_player,2560);
    Memory{}.check(manager,152);require(Memory{}.get<uintptr_t>(manager)==state->manager_vtable,VCF_UNAVAILABLE);
    watched_id=p.getUniqueId().str();state->watched.store(native,std::memory_order_release);state->watched_manager.store(manager,std::memory_order_release);
    report(action,{{"manager_field_offset",2560}});sender.sendMessage("Private hook watch armed for one validated test inventory and native request manager.");return true;
   }
   require(state->watched.load()==native&&watched_id==p.getUniqueId().str());
   auto before=snapshot(native);auto counts=counters();
   if(action=="same-item"){
    // Explicit console-only test, exactly the pre-existing six plain stones.
    auto item=p.getInventory().getItem(7);require(item&&item->getType().getId()=="minecraft:stone"&&item->getAmount()==6&&item->getData()==0,VCF_DENIED);
    require(item->getNbt().empty(),VCF_DENIED);p.getInventory().setItem(7,item);
   }else require(action=="snapshot");
   auto after=snapshot(native);report(action,{{"before",counts},{"inventory_bytes",before.size()},{"inventory_sha256",hash(before)},{"after_sha256",hash(after)},{"inventory_unchanged",before==after}});
   require(before==after,VCF_CONFLICT);sender.sendMessage("Private hook {} complete: inventory unchanged, {} saved bytes.",action,before.size());
  }catch(const Error&e){sender.sendErrorMessage("Private hook refused: {}",static_cast<int>(e.status));}
  catch(const std::exception&e){sender.sendErrorMessage("Private hook error: {}",e.what());}
  return true;
 }
};
ENDSTONE_PLUGIN("vcf_private_request_probe","0.0.0",HookProbe){description="Private native inventory hook qualification";command("vcf_request_probe").description("Isolated console inventory hook checks").usages("/vcf_request_probe <action: string>").permissions("vcf_request_probe.admin");permission("vcf_request_probe.admin").default_(endstone::PermissionDefault::Operator);}
