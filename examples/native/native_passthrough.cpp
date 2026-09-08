#include <oni/vcf/sdk.hpp>
#include <oni/vcf/loader.hpp>
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/scheduler/scheduler.h>
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <set>

namespace sdk = oni::vcf::sdk;
class NativePassthrough : public endstone::Plugin {
    static constexpr std::array<std::string_view,11> screens = {
        "craft", "anvil", "grindstone", "smithing", "stonecutter", "loom",
        "cartography", "inventory2x2", "armor", "offhand", "recipebook"
    };
    std::unique_ptr<sdk::Client> ui_;
    std::shared_ptr<endstone::Task> cleanup_;
    std::map<std::string,std::string> bindings_;
    std::set<vcf_handle> tickets_;
    uint64_t opened_=0,closed_=0,refused_=0,guard_checks_=0;

    static vcf_status VCF_CALL guarded(void* context,const vcf_guard_event* event) {
        auto& self=*static_cast<NativePassthrough*>(context);
        if(self.ui_&&event->requesting_consumer==self.ui_->owner())++self.guard_checks_;
        // Protection plugins can inspect the copied source descriptor here.
        // This example imposes its permission through source_permission below.
        return VCF_OK;
    }
    static vcf_status VCF_CALL completed(void* context,const vcf_event* event) {
        auto& self=*static_cast<NativePassthrough*>(context);
        if(event->kind==VCF_EVENT_OPEN)++self.opened_;
        else if(event->kind==VCF_EVENT_CLOSE)++self.closed_;
        else if(event->kind==VCF_EVENT_FAILURE)++self.refused_;
        try {
            const std::string player(event->player.data,event->player.length);
            for(auto* online:self.getServer().getOnlinePlayers()) {
                if(online->getUniqueId().str()!=player)continue;
                online->sendMessage("NativePassthrough ticket "+std::to_string(event->ticket)
                    +": event "+std::to_string(event->kind)+", status "+std::to_string(event->result));
                break;
            }
        } catch(...) { return VCF_INTERNAL; }
        return VCF_OK;
    }
    static bool known(std::string_view screen) {
        return std::find(screens.begin(),screens.end(),screen)!=screens.end();
    }
    void open(endstone::Player& player,std::string_view screen) {
        if(!ui_||!known(screen))throw std::runtime_error("Choose one of the eleven documented original entry points.");
        if(!player.hasPermission("vcf.examples.passthrough"))throw std::runtime_error("Permission denied.");
        if(tickets_.size()>=128)throw std::runtime_error("Example ticket limit reached.");
        auto capability=ui_->capability(ui_->resolve(screen));
        if(!capability.native_available)throw std::runtime_error(std::string(capability.reason.data,capability.reason.length));
        auto request=sdk::descriptor<vcf_session_desc>();auto player_id=player.getUniqueId().str();
        request.player=sdk::view(player_id);request.canonical_id=sdk::view(screen);
        request.source_permission=sdk::view("vcf.examples.passthrough");
        const bool shared=screen=="inventory2x2"||screen=="armor"||screen=="offhand"||screen=="recipebook";
        request.mode=shared?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT;
        request.callback=completed;request.context=this;
        vcf_handle ticket=0;sdk::checked(ui_->api().prepare(ui_->owner(),&request,&ticket));
        tickets_.insert(ticket);ui_->open(ticket);
    }
public:
    void onEnable() override {
        try {
            ui_=std::make_unique<sdk::Client>(sdk::discover(),"vcf_native_passthrough");
            ui_->guard("example_lifecycle",guarded,this);
            cleanup_=getServer().getScheduler().runTaskTimer(*this,[this]{
                try {
                    for(auto it=tickets_.begin();it!=tickets_.end();) {
                        if(ui_->info(*it).state==VCF_TERMINAL) {
                            sdk::checked(ui_->api().forget(ui_->owner(),*it));it=tickets_.erase(it);
                        } else ++it;
                    }
                } catch(const std::exception& e) { getLogger().error("Ticket cleanup refused: {}",e.what()); }
            },1,1);
            registerEvent(&NativePassthrough::interact,*this,endstone::EventPriority::Highest);
            registerEvent(&NativePassthrough::quit,*this);
            getLogger().info("NativePassthrough connected through SDK 1.1; eleven original-mode requests available subject to provider admission.");
        } catch(const std::exception& e) {getLogger().error("NativePassthrough unavailable: {}",e.what());ui_.reset();}
    }
    void onDisable() override {
        if(cleanup_)cleanup_->cancel();bindings_.clear();tickets_.clear();
        if(ui_) {
            auto status=ui_->dispose();
            if(status!=VCF_OK)getLogger().error("Consumer revocation status {}; restart before unloading native code.",status);
            ui_.reset();
        }
    }
    void quit(endstone::PlayerQuitEvent& event) {bindings_.erase(event.getPlayer().getUniqueId().str());}
    void interact(endstone::PlayerInteractEvent& event) {
        if(!ui_||event.isCancelled()||!event.getPlayer().isSneaking()||!event.getItem())return;
        if(event.getAction()!=endstone::PlayerInteractEvent::Action::RightClickAir
            &&event.getAction()!=endstone::PlayerInteractEvent::Action::RightClickBlock)return;
        if(event.getItem()->getType().getId()!="minecraft:compass")return;
        auto it=bindings_.find(event.getPlayer().getUniqueId().str());if(it==bindings_.end())return;
        event.setCancelled(true);
        try{open(event.getPlayer(),it->second);}
        catch(const std::exception& e){event.getPlayer().sendErrorMessage(e.what());}
    }
    bool onCommand(endstone::CommandSender& sender,const endstone::Command&,const std::vector<std::string>& args) override {
        if(!ui_){sender.sendErrorMessage("Native VCF provider is unavailable.");return true;}
        if(args.empty()||args[0]=="status") {
            sender.sendMessage("Original-mode SDK example: opens "+std::to_string(opened_)+", closes "+std::to_string(closed_)
                +", failures "+std::to_string(refused_)+", guard checks "+std::to_string(guard_checks_));return true;
        }
        auto* player=dynamic_cast<endstone::Player*>(&sender);
        if(!player){sender.sendErrorMessage("Request screens from a connected player.");return true;}
        try {
            if(args[0]=="unbind")bindings_.erase(player->getUniqueId().str());
            else if(args[0]=="bind"&&args.size()==2&&known(args[1])) {
                if(bindings_.size()>=100&&!bindings_.contains(player->getUniqueId().str()))throw std::runtime_error("Binding limit reached.");
                bindings_[player->getUniqueId().str()]=args[1];
                player->sendMessage("Sneak and right-click with a compass to request "+args[1]+". Use /vcf_native unbind to clear.");
            } else if(args.size()==1)open(*player,args[0]);
            else throw std::runtime_error("Use /vcf_native <screen> or /vcf_native bind <screen>.");
        }catch(const std::exception& e){sender.sendErrorMessage(e.what());}
        return true;
    }
};
ENDSTONE_PLUGIN("vcf_native_passthrough","0.1.0-dev",NativePassthrough) {
    description="SDK-only original-mode regression example; gameplay qualification incomplete";
    depend={"onistone_vcf"};
    permission("vcf.examples.passthrough").default_(endstone::PermissionDefault::Operator);
    command("vcf_native").description("Original native screen SDK regression example")
        .usages("/vcf_native [action: string] [screen: string]").permissions("vcf.examples.passthrough");
}
