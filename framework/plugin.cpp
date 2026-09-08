#include <oni/vcf/core.hpp>
#include <oni/vcf/sdk.hpp>
#include "platform/runtime.hpp"
#ifdef _WIN32
#include "platform/windows/bridge.hpp"
#include "platform/windows/native_ui.hpp"
#endif
#include <nlohmann/json.hpp>
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/scheduler/scheduler.h>
#include <endstone/inventory/item_type.h>
#include <endstone/form/action_form.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/event/actor/player_death_event.h>
#include <endstone/event/player/player_teleport_event.h>
#include <endstone/event/player/player_dimension_change_event.h>
#include <endstone/event/server/plugin_disable_event.h>
#include <endstone/event/server/packet_receive_event.h>
#include <endstone/event/server/packet_send_event.h>
#include <fstream>
using namespace oni::vcf;
class VirtualContainerFramework:public endstone::Plugin {
 std::unique_ptr<Engine> engine_;std::unique_ptr<sdk::Client> self_;vcf_api api_{};
 platform::Admission admission_;std::shared_ptr<endstone::Task> task_;
 bool original_native_enabled_=false;
#ifdef _WIN32
 std::unique_ptr<platform::windows::NativeUi> native_ui_;
#endif
 std::shared_ptr<bool> lifetime_=std::make_shared<bool>(false);
 std::map<std::string,uint32_t> catalog_actions_;
 struct FormLease {vcf_handle owner,ticket;bool sending=true,observed=false,superseded=false;};
 std::map<std::string,FormLease> forms_;
 void retire_form(vcf_handle owner,vcf_handle id,vcf_status result){
  try{engine_->retired(owner,id,result);}catch(const Error&){}
 }
 void poll_forms(){
  std::vector<FormLease> done;
  for(auto it=forms_.begin();it!=forms_.end();){
   if(it->second.superseded){done.push_back(it->second);it=forms_.erase(it);}else ++it;
  }
  for(const auto& lease:done)retire_form(lease.owner,lease.ticket,VCF_CLOSED);
 }
 endstone::Player* player(std::string_view id){
  for(auto*p:getServer().getOnlinePlayers())if(p->getUniqueId().str()==id)return p;
  return nullptr;
 }
 static vcf_status VCF_CALL choose(void* context,const vcf_event* event){
  auto&self=*static_cast<VirtualContainerFramework*>(context);
  try {
   std::string detail(event->detail.data,event->detail.length);auto colon=detail.find(':');
   auto action=detail.substr(colon+1);auto it=self.catalog_actions_.find(action);if(it==self.catalog_actions_.end())return VCF_NOT_FOUND;
   const auto&row=catalog()[it->second];auto*p=self.player(std::string_view(event->player.data,event->player.length));if(!p)return VCF_CLOSED;
   if(!p->hasPermission(std::string(row.permission)))return VCF_DENIED;
#ifdef _WIN32
   if(self.original_native_enabled_&&platform::windows::NativeUi::supports(row.id)){
    auto mode=row.id=="inventory2x2"||row.id=="armor"||row.id=="offhand"||row.id=="recipebook"?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT;
    auto ticket=self.self_->prepare(p->getUniqueId().str(),row.id,mode);self.self_->open(ticket);return VCF_OK;
   }
#endif
   p->sendMessage(std::string(row.id)+": C++ native/custom screen adapter is not yet qualified. This form is the catalog, not that screen.");
   return VCF_OK;
  }catch(...){return VCF_INTERNAL;}
 }
 vcf_status show(const Session&s){
  auto*p=player(s.player);if(!p)return VCF_CLOSED;
  if(!s.is_menu){
#ifdef _WIN32
   return original_native_enabled_&&native_ui_?native_ui_->open(s):VCF_UNAVAILABLE;
#else
   return VCF_UNAVAILABLE;
#endif
  }
  if(p->getGameVersion()!="1.26.45")return VCF_UNAVAILABLE;
  endstone::ActionForm form;form.setTitle(s.title).setContent(s.content);
  auto weak=std::weak_ptr<bool>(lifetime_);
  for(const auto&b:s.buttons)form.addButton(b.label,b.icon.empty()?std::nullopt:std::optional<std::string>{b.icon});
  form.setOnSubmit([weak,this,id=s.id,owner=s.owner](endstone::Player*p,int index){
   auto live=weak.lock();if(!live||!*live||!p||index<0)return;
   auto it=forms_.find(p->getUniqueId().str());
   if(it==forms_.end()||it->second.ticket!=id||it->second.superseded)return;
   forms_.erase(it);
   try{engine_->selected(id,p->getUniqueId().str(),static_cast<uint32_t>(index));}
   catch(const Error&e){retire_form(owner,id,e.status);}catch(...){retire_form(owner,id,VCF_INTERNAL);}
  });
  form.setOnClose([weak,this,id=s.id,owner=s.owner,identity=s.player](endstone::Player*){
   auto live=weak.lock();if(!live||!*live)return;
   auto it=forms_.find(identity);if(it==forms_.end()||it->second.ticket!=id)return;
   const bool superseded=it->second.superseded;forms_.erase(it);
   // The client's form is already closed. Do not send a broad closeForm() that
   // might target a replacement installed by another plugin's callback.
   retire_form(owner,id,superseded?VCF_CLOSED:VCF_OK);
  });
  require(forms_.contains(s.player)||forms_.size()<100,VCF_CAPACITY);
  forms_.insert_or_assign(s.player,FormLease{s.owner,s.id});
  try{p->sendForm(form);}catch(...){forms_.erase(s.player);throw;}
  auto it=forms_.find(s.player);
  if(it==forms_.end()||it->second.ticket!=s.id)return VCF_CONFLICT;
  it->second.sending=false;
  if(!it->second.observed||it->second.superseded){forms_.erase(it);return VCF_UNAVAILABLE;}
  return VCF_OK;
 }
 void catalog_menu(endstone::Player&p){
  std::vector<std::string> labels,actions;labels.reserve(catalog().size());actions.reserve(catalog().size());
  for(const auto&row:catalog()){
   labels.emplace_back(std::string(row.id)+" - migration pending");actions.emplace_back("catalog."+std::string(row.id));
  }
  std::vector<vcf_button> buttons;for(size_t i=0;i<labels.size();++i){auto b=sdk::descriptor<vcf_button>();b.label=sdk::view(labels[i]);b.action=sdk::view(actions[i]);buttons.push_back(b);}
  auto d=sdk::descriptor<vcf_menu_desc>();auto id=p.getUniqueId().str();d.player=sdk::view(id);d.title=sdk::view("Onistone VCF - development catalog");
  d.content=sdk::view("All 69 entries retained. Native and custom screen qualification is incomplete. Select an entry for its current result.");
  d.permission=sdk::view("remoteworkstations.use");d.buttons=buttons.data();d.button_count=static_cast<uint32_t>(buttons.size());vcf_handle ticket=0;
  sdk::checked(api_.show_menu(self_->owner(),&d,&ticket));
 }
public:
 void onEnable()override{
  try{
   admission_=platform::inspect_runtime();
   getLogger().info("BDS SHA256: {}",admission_.bds_sha256);
   getLogger().info("Loader runtime SHA256: {}",admission_.runtime_sha256);
   if(!admission_.accepted){getLogger().error("VCF refused admission: {}",admission_.reason);return;}
   // Endstone owns outstanding std::function form objects. Pin code for this
   // process before registering callbacks; disable revokes their weak lease.
   require(platform::pin_provider(),VCF_UNAVAILABLE);
#ifdef _WIN32
   platform::windows::initialize_bridge();
   getLogger().info("Windows native primitive manifest verified in loaded memory; gameplay qualification remains separate.");
#endif
   if(getServer().getPluginManager().getPlugin("remote_workstations")){
    getLogger().error("VCF refuses to enable beside legacy remote_workstations. Stop, back up, and migrate the old installation.");return;
   }
   std::filesystem::create_directories(getDataFolder());
   auto config_path=getDataFolder()/"config.json";
   if(!std::filesystem::exists(config_path)){
    std::ofstream config(config_path);config<<"{\n  \"schema_version\": 1,\n  \"experimental_original_windows\": false\n}\n";
   }
   require(std::filesystem::file_size(config_path)<=4096,VCF_CAPACITY);
   std::ifstream config_file(config_path);auto config=nlohmann::json::parse(config_file);
   require(config.is_object()&&config.size()==2&&config.value("schema_version",0)==1&&config.contains("experimental_original_windows")&&config["experimental_original_windows"].is_boolean());
   original_native_enabled_=config["experimental_original_windows"].get<bool>();
   Host host;
   host.consumer_allowed=[this](std::string_view name){auto*p=getServer().getPluginManager().getPlugin(std::string(name));return p&&p->isEnabled();};
   host.permission=[this](std::string_view id,std::string_view permission){auto*p=player(id);return p&&(permission.empty()||p->hasPermission(std::string(permission)));};
   host.item_limit=[this](std::string_view name){auto*type=getServer().getRegistry<endstone::ItemType>().get(endstone::ItemTypeId(name));return type?static_cast<uint32_t>(type->getMaxStackSize()):0;};
   host.native_available=[this](std::string_view id){
#ifdef _WIN32
    return original_native_enabled_&&native_ui_&&platform::windows::NativeUi::supports(id);
#else
    return false;
#endif
   };
   host.open=[this](const Session&s){return show(s);};
   host.close=[this](const Session&s)->vcf_status{
    if(s.is_menu){
     auto it=forms_.find(s.player);if(it==forms_.end()||it->second.ticket!=s.id)return VCF_OK;
     const bool owned=it->second.observed&&!it->second.superseded;forms_.erase(it);
     if(owned)if(auto*p=player(s.player))p->closeForm();
     return VCF_OK;
    }
#ifdef _WIN32
    if(native_ui_)return native_ui_->close(s);
#endif
    return VCF_OK;
   };
   engine_=std::make_unique<Engine>(std::move(host));attach_engine(engine_.get());
#ifdef _WIN32
   native_ui_=std::make_unique<platform::windows::NativeUi>(getServer(),*engine_);
#endif
   sdk::checked(oni_vcf_get_api(VCF_ABI_VERSION,sizeof(api_),&api_));
   self_=std::make_unique<sdk::Client>(api_,"onistone_vcf");
   for(uint32_t i=0;i<catalog().size();++i){auto name="catalog."+std::string(catalog()[i].id);catalog_actions_.emplace(name,i);self_->action(name,choose,this,std::string(catalog()[i].permission));}
   *lifetime_=true;
   task_=getServer().getScheduler().runTaskTimer(*this,[this]{try{
    poll_forms();
#ifdef _WIN32
    if(native_ui_)native_ui_->tick();
#endif
    engine_->tick();engine_->collect_terminal(self_->owner());
   }catch(...){getLogger().error("VCF scheduler stopped by invariant failure.");task_->cancel();}},0,1);
   registerEvent(&VirtualContainerFramework::quit,*this);
   registerEvent(&VirtualContainerFramework::death,*this);
   registerEvent(&VirtualContainerFramework::teleport,*this);
   registerEvent(&VirtualContainerFramework::dimension,*this);
   registerEvent(&VirtualContainerFramework::consumer_disabled,*this);
   registerEvent(&VirtualContainerFramework::received,*this,endstone::EventPriority::Highest);
   registerEvent(&VirtualContainerFramework::sent,*this,endstone::EventPriority::Monitor);
   std::filesystem::create_directories(getDataFolder());
   std::ofstream receipt(getDataFolder()/"native-startup.txt");receipt<<"native C++ plugin; no project Python runtime\nBDS "<<admission_.bds_sha256<<"\nEndstone "<<admission_.runtime_sha256<<"\n69 retained entries; all-UI acceptance NOT QUALIFIED\n";
   getLogger().info("Native VCF enabled: C ABI 1.1 (1.0 compatible), 69 catalog entries retained, C++ forms/actions/guards active; all-UI acceptance NOT QUALIFIED.");
  }catch(const std::exception&e){getLogger().error("VCF startup failed: {}",e.what());onDisable();}
   catch(const Error&e){getLogger().error("VCF startup failed with status {}",e.status);onDisable();}
 }
 void onDisable()override{
  *lifetime_=false;
  if(task_)task_->cancel();
#ifdef _WIN32
  if(native_ui_)native_ui_->shutdown();
#endif
  if(engine_){try{engine_->shutdown();}catch(...){}}
  forms_.clear();
#ifdef _WIN32
  native_ui_.reset();
  try{platform::windows::shutdown_editors();}catch(...){}
#endif
  self_.reset();attach_engine(nullptr);engine_.reset();
 }
 void quit(endstone::PlayerQuitEvent&e){if(engine_)engine_->player_gone(e.getPlayer().getUniqueId().str());}
 void death(endstone::PlayerDeathEvent&e){if(engine_)engine_->player_gone(e.getPlayer().getUniqueId().str());}
 void teleport(endstone::PlayerTeleportEvent&e){if(engine_)engine_->player_gone(e.getPlayer().getUniqueId().str());}
 void dimension(endstone::PlayerDimensionChangeEvent&e){if(engine_)engine_->player_gone(e.getPlayer().getUniqueId().str());}
 void consumer_disabled(endstone::PluginDisableEvent&e){if(engine_)engine_->release_named(e.getPlugin().getName());}
 void received(endstone::PacketReceiveEvent&e){
#ifdef _WIN32
  if(native_ui_)native_ui_->receive(e);
#endif
 }
 void sent(endstone::PacketSendEvent&e){
  if(e.getPlayer()&&!e.isCancelled()){
   auto it=forms_.find(e.getPlayer()->getUniqueId().str());
   if(it!=forms_.end()){
    if(e.getPacketId()==100){
     if(it->second.sending&&!it->second.observed)it->second.observed=true;
     else it->second.superseded=true;
    }else if(e.getPacketId()==46)it->second.superseded=true;
   }
  }
#ifdef _WIN32
  if(native_ui_)native_ui_->sent(e);
#endif
 }
 bool onCommand(endstone::CommandSender&sender,const endstone::Command&command,const std::vector<std::string>&args)override{
  try{
   if(!engine_){sender.sendErrorMessage(admission_.reason.empty()?"Native VCF is not active; inspect startup diagnostics.":admission_.reason);return true;}
   auto name=command.getName();
   if(name=="vcf"||name=="workstations"){
    if(!args.empty()&&(args[0]=="status"||args[0]=="diagnose"||args[0]=="sessions")){
     if(!sender.hasPermission("remoteworkstations.status")){sender.sendErrorMessage("Permission denied.");return true;}
     sender.sendMessage("Native VCF C ABI 1.1; sessions "+std::to_string(engine_->session_count())+"; 69 entries retained; all-UI NOT QUALIFIED.");return true;
    }
    if(!args.empty()&&(args[0]=="capabilities"||args[0]=="list")){
     for(const auto&row:catalog())sender.sendMessage(std::string(row.id)+": native/custom migration unqualified");return true;
    }
    if(args.size()==2&&args[0]=="open")name=args[1];
    else {if(auto*p=dynamic_cast<endstone::Player*>(&sender))catalog_menu(*p);else sender.sendErrorMessage("Open the catalog from Minecraft.");return true;}
   }
   auto*row=resolve(name);if(!row){sender.sendErrorMessage("Unknown UI entry.");return true;}
   if(!sender.hasPermission(std::string(row->permission))){sender.sendErrorMessage("Permission denied.");return true;}
#ifdef _WIN32
   if(original_native_enabled_&&platform::windows::NativeUi::supports(row->id)){
    auto*p=dynamic_cast<endstone::Player*>(&sender);if(!p){sender.sendErrorMessage("Open native screens from Minecraft.");return true;}
    auto mode=row->id=="inventory2x2"||row->id=="armor"||row->id=="offhand"||row->id=="recipebook"?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT;
    auto ticket=self_->prepare(p->getUniqueId().str(),row->id,mode);self_->open(ticket);return true;
   }
#endif
   sender.sendErrorMessage(std::string(row->id)+": the native C++ adapter is not yet qualified; the legacy source contract is retained in the migration audit.");return true;
  }catch(...){sender.sendErrorMessage("VCF request refused; inspect diagnostics.");return true;}
 }
};
ENDSTONE_PLUGIN("onistone_vcf","0.1.0-dev",VirtualContainerFramework){
 description="Native virtual-container framework development build; all-UI qualification incomplete";
 authors={"TheNINJALLO"};website="https://github.com/TheNINJALLO/endstone-remote-workstations";
 command("vcf").description("Native VCF catalog and diagnostics").usages("/vcf [action: string] [type: string]").permissions("remoteworkstations.use");
 command("workstations").description("Migrated original UI entry point").usages("/workstations [action: string] [type: string]").permissions("remoteworkstations.use");
 permission("remoteworkstations.use").default_(endstone::PermissionDefault::True);
 for(auto p:{"admin","status","diagnostics","contexts"})permission("remoteworkstations."+std::string(p)).default_(endstone::PermissionDefault::Operator);
 for(const auto&row:catalog()){
  permission(std::string(row.permission)).default_(row.privileged?endstone::PermissionDefault::Operator:endstone::PermissionDefault::True);
  auto aliases=row.aliases;
  while(!aliases.empty()){auto split=aliases.find('|');auto name=std::string(aliases.substr(0,split));
   command(name).description("Request "+std::string(row.id)).usages("/"+name).permissions(std::string(row.permission));
   if(split==std::string_view::npos)break;aliases.remove_prefix(split+1);
  }
 }
}
