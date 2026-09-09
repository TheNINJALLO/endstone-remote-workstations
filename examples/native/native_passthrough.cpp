#include <oni/vcf/sdk.hpp>
#include <oni/vcf/loader.hpp>
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/event/player/player_interact_event.h>
#include <endstone/event/player/player_quit_event.h>
#include <endstone/scheduler/scheduler.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <endstone/level/dimension.h>
#include <map>
#include <memory>
#include <optional>

namespace sdk = oni::vcf::sdk;
class NativePassthrough : public endstone::Plugin {
    static constexpr std::array<std::string_view,26> screens = {
        "craft", "anvil", "grindstone", "smithing", "stonecutter", "loom",
        "cartography", "inventory2x2", "armor", "offhand", "recipebook", "enchanting",
        "furnace", "blastfurnace", "smoker", "enderchest", "barrel",
        "dispenser", "dropper", "brewing", "beacon", "crafter", "hopper",
        "chest", "trappedchest", "doublechest"
    };
    std::unique_ptr<sdk::Client> ui_;
    std::shared_ptr<endstone::Task> cleanup_;
    std::map<std::string,std::string> bindings_;
    std::map<vcf_handle,std::string> tickets_;
    uint64_t opened_=0,closed_=0,refused_=0,guard_checks_=0;
    uint64_t held_reads_=0,held_refused_=0;
    uint64_t item_reads_=0,item_refused_=0;
    std::map<std::string,vcf_handle> observations_;
    uint64_t observed_=0,validated_=0,invalidated_=0;
    struct DeniedSource {std::string dimension;std::array<int32_t,3> position;};
    std::optional<DeniedSource> denied_source_;

    static vcf_status VCF_CALL guarded(void* context,const vcf_guard_event* event) {
        auto& self=*static_cast<NativePassthrough*>(context);
        if(self.ui_&&event->requesting_consumer==self.ui_->owner())++self.guard_checks_;
        // Both halves of a paired chest are checked using the same ticket.
        // The example policy applies only to this consumer's own requests.
        if(self.ui_&&event->requesting_consumer==self.ui_->owner()&&self.denied_source_){
            const auto& r=event->request;const auto& denied=*self.denied_source_;
            if(r.mode==VCF_REAL_SOURCE&&std::string_view(r.dimension.data,r.dimension.length)==denied.dimension
                &&std::array<int32_t,3>{r.x,r.y,r.z}==denied.position)return VCF_DENIED;
        }
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
    static bool linked(std::string_view screen){
        return screen=="furnace"||screen=="blastfurnace"||screen=="smoker"||screen=="enderchest"||screen=="barrel"
            ||screen=="dispenser"||screen=="dropper"||screen=="brewing"||screen=="beacon"||screen=="crafter"||screen=="hopper"
            ||screen=="chest"||screen=="trappedchest"||screen=="doublechest";
    }
    static std::array<int32_t,3> coordinates(std::string_view text){
        std::array<int32_t,3> result{};
        for(size_t i=0;i<3;++i){
            auto comma=text.find(',');auto part=text.substr(0,comma);
            auto parsed=std::from_chars(part.data(),part.data()+part.size(),result[i]);
            if(part.empty()||parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()
                ||(i<2)==(comma==std::string_view::npos))throw std::runtime_error("Use absolute block coordinates x,y,z.");
            if(i<2)text.remove_prefix(comma+1);
        }
        return result;
    }
    void open(endstone::Player& player,std::string_view screen,std::string_view source={}) {
        if(!ui_||!known(screen))throw std::runtime_error("Choose one of the documented original entry points.");
        if(!player.hasPermission("vcf.examples.passthrough"))throw std::runtime_error("Permission denied.");
        if(tickets_.size()>=128)throw std::runtime_error("Example ticket limit reached.");
        auto capability=ui_->capability(ui_->resolve(screen));
        if(!capability.native_available)throw std::runtime_error(std::string(capability.reason.data,capability.reason.length));
        auto request=sdk::descriptor<vcf_session_desc>();auto player_id=player.getUniqueId().str();
        request.player=sdk::view(player_id);request.canonical_id=sdk::view(screen);
        request.source_permission=sdk::view("vcf.examples.passthrough");
        const bool shared=screen=="inventory2x2"||screen=="armor"||screen=="offhand"||screen=="recipebook";
        request.mode=(shared||linked(screen))?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT;
        std::string dimension;
        if(linked(screen)){
            auto xyz=coordinates(source);dimension=player.getDimension().getName();request.dimension=sdk::view(dimension);
            request.x=xyz[0];request.y=xyz[1];request.z=xyz[2];
        }else if(!source.empty())throw std::runtime_error("This entry does not accept a block source.");
        request.callback=completed;request.context=this;
        vcf_handle ticket=0;sdk::checked(ui_->api().prepare(ui_->owner(),&request,&ticket));
        tickets_.emplace(ticket,player_id);ui_->open(ticket);
    }
public:
    void onEnable() override {
        try {
            ui_=std::make_unique<sdk::Client>(sdk::discover(),"vcf_native_passthrough");
            ui_->guard("example_lifecycle",guarded,this);
            cleanup_=getServer().getScheduler().runTaskTimer(*this,[this]{
                try {
                    for(auto it=tickets_.begin();it!=tickets_.end();) {
                        if(ui_->info(it->first).state==VCF_TERMINAL) {
                            sdk::checked(ui_->api().forget(ui_->owner(),it->first));it=tickets_.erase(it);
                        } else ++it;
                    }
                } catch(const std::exception& e) { getLogger().error("Ticket cleanup refused: {}",e.what()); }
            },1,1);
            registerEvent(&NativePassthrough::interact,*this,endstone::EventPriority::Highest);
            registerEvent(&NativePassthrough::quit,*this);
            getLogger().info("NativePassthrough connected through SDK 1.4; twenty-six original-mode requests available subject to provider admission; saved-item observations available.");
        } catch(const std::exception& e) {getLogger().error("NativePassthrough unavailable: {}",e.what());ui_.reset();}
    }
    void onDisable() override {
        if(cleanup_)cleanup_->cancel();bindings_.clear();tickets_.clear();observations_.clear();
        if(ui_) {
            auto status=ui_->dispose();
            if(status!=VCF_OK)getLogger().error("Consumer revocation status {}; restart before unloading native code.",status);
            ui_.reset();
        }
    }
    void quit(endstone::PlayerQuitEvent& event) {
        auto id=event.getPlayer().getUniqueId().str();bindings_.erase(id);
        if(auto it=observations_.find(id);it!=observations_.end()){
            if(ui_)ui_->api().release_inventory_observation(ui_->owner(),it->second);observations_.erase(it);
        }
    }
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
                +", failures "+std::to_string(refused_)+", guard checks "+std::to_string(guard_checks_)
                +", outstanding tickets "+std::to_string(tickets_.size()));
            sender.sendMessage("Held inspections: successes "+std::to_string(held_reads_)+", refusals "+std::to_string(held_refused_));
            sender.sendMessage("Saved item reads: successes "+std::to_string(item_reads_)+", refusals "+std::to_string(item_refused_));
            sender.sendMessage("Item observations: created "+std::to_string(observed_)+", valid "+std::to_string(validated_)+", invalid "+std::to_string(invalidated_)+", outstanding "+std::to_string(observations_.size()));return true;
        }
        if(args[0]=="validate-items"&&args.size()==1&&!sender.asPlayer()){
            for(const auto&[id,token]:observations_){
                auto result=ui_->api().validate_inventory_observation(ui_->owner(),token);
                if(result==VCF_OK)++validated_;else ++invalidated_;
                sender.sendMessage("Item observation "+std::to_string(token)+": status "+std::to_string(result));
            }
            sender.sendMessage("Validated "+std::to_string(observations_.size())+" owned item observations.");return true;
        }
        if(args[0]=="guard-clear"&&args.size()==1){
            denied_source_.reset();sender.sendMessage("Cleared the example source guard.");return true;
        }
        if(args[0]=="guard-deny"&&args.size()==2){
            try{
                const auto split=args[1].find('|');
                if(split==std::string::npos||split==0||split>128)throw std::runtime_error("Use dimension|x,y,z.");
                auto xyz=coordinates(std::string_view(args[1]).substr(split+1));
                denied_source_=DeniedSource{args[1].substr(0,split),xyz};
                sender.sendMessage("Example source guard denies "+args[1]+" for this consumer's tickets.");
            }catch(const std::exception& e){sender.sendErrorMessage(e.what());}
            return true;
        }
        auto* player=sender.asPlayer();
        if(args.size()==1&&args[0]=="close") {
            try {
                uint32_t count=0;
                for(const auto&[ticket,owner]:tickets_) {
                    if(player&&owner!=player->getUniqueId().str())continue;
                    if(ui_->info(ticket).state==VCF_TERMINAL)continue;
                    ui_->close(ticket);++count;
                }
                sender.sendMessage("Queued closure of "+std::to_string(count)+" owned SDK tickets.");
            } catch(const std::exception& e) {sender.sendErrorMessage(e.what());}
            return true;
        }
        if(!player){sender.sendErrorMessage("Request screens from a connected player.");return true;}
        try {
            if(args[0]=="observe-item"&&args.size()==2){
                uint32_t slot=0;auto parsed=std::from_chars(args[1].data(),args[1].data()+args[1].size(),slot);
                if(parsed.ec!=std::errc{}||parsed.ptr!=args[1].data()+args[1].size()||slot>=36)throw std::runtime_error("Choose inventory slot 0 through 35.");
                auto id=player->getUniqueId().str();
                if(auto old=observations_.find(id);old!=observations_.end()){
                    ui_->api().release_inventory_observation(ui_->owner(),old->second);observations_.erase(old);
                }
                observations_[id]=ui_->observe_inventory_item(id,slot);++observed_;
                player->sendMessage("Observing inventory slot "+std::to_string(slot)+". Use /vcf_native validate-item, then /vcf_native release-item.");
            }else if(args[0]=="validate-item"&&args.size()==1){
                auto it=observations_.find(player->getUniqueId().str());if(it==observations_.end())throw std::runtime_error("Observe an item first.");
                const auto result=ui_->api().validate_inventory_observation(ui_->owner(),it->second);
                if(result==VCF_OK)++validated_;else ++invalidated_;
                player->sendMessage("Item observation "+std::to_string(it->second)+": status "+std::to_string(result));
            }else if(args[0]=="release-item"&&args.size()==1){
                auto it=observations_.find(player->getUniqueId().str());if(it==observations_.end())throw std::runtime_error("Observe an item first.");
                auto result=ui_->api().release_inventory_observation(ui_->owner(),it->second);
                if(result!=VCF_NOT_FOUND)sdk::checked(result);observations_.erase(it);player->sendMessage("Released the item observation.");
            }else if(args[0]=="read-item"&&args.size()==2){
                uint32_t slot=0;auto parsed=std::from_chars(args[1].data(),args[1].data()+args[1].size(),slot);
                if(parsed.ec!=std::errc{}||parsed.ptr!=args[1].data()+args[1].size()||slot>=36)throw std::runtime_error("Choose inventory slot 0 through 35.");
                sdk::SavedInventoryItem saved;
                try{saved=ui_->read_inventory_item(player->getUniqueId().str(),slot);++item_reads_;}
                catch(...){++item_refused_;throw;}
                std::string hash;constexpr char hex[]="0123456789abcdef";
                for(auto byte:saved.info.digest){hash+=hex[byte>>4];hash+=hex[byte&15];}
                player->sendMessage(std::string("Saved item ")+saved.info.identifier+", slot "+std::to_string(slot)
                    +", amount "+std::to_string(saved.info.amount)+", native save bytes "+std::to_string(saved.nbt.size())+". Digest "+hash);
            }else if(args[0]=="inspect-held"&&args.size()==1){
                vcf_held_info held{};
                try{held=ui_->inspect_held(player->getUniqueId().str());++held_reads_;}
                catch(...){++held_refused_;throw;}
                std::string hash;constexpr char hex[]="0123456789abcdef";
                for(auto byte:held.digest){hash+=hex[byte>>4];hash+=hex[byte&15];}
                player->sendMessage(std::string("Held ")+held.canonical_id+": "+held.identifier+", slot "+std::to_string(held.slot)
                    +", amount "+std::to_string(held.amount)+", metadata bytes "+std::to_string(held.metadata_bytes)
                    +", durable ID "+(held.durable_id[0]?"present":"absent")+", native editor unavailable. Digest "+hash);
            }else if(args[0]=="unbind")bindings_.erase(player->getUniqueId().str());
            else if(args[0]=="bind"&&args.size()==2&&known(args[1])&&!linked(args[1])) {
                if(bindings_.size()>=100&&!bindings_.contains(player->getUniqueId().str()))throw std::runtime_error("Binding limit reached.");
                bindings_[player->getUniqueId().str()]=args[1];
                player->sendMessage("Sneak and right-click with a compass to request "+args[1]+". Use /vcf_native unbind to clear.");
            } else if(args.size()==2&&linked(args[0]))open(*player,args[0],args[1]);
            else if(args.size()==1)open(*player,args[0]);
            else throw std::runtime_error("Use /vcf_native <screen>, /vcf_native <source-screen> <x,y,z>, or /vcf_native bind <screen>.");
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
