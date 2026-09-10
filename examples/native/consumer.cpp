#include <oni/vcf/sdk.hpp>
#include <oni/vcf/loader.hpp>
#include <endstone/plugin/plugin.h>
#include <endstone/player.h>
#include <endstone/scheduler/scheduler.h>
#include <memory>
#include <set>
#ifndef VCF_CONSUMER_ID
#define VCF_CONSUMER_ID "vcf_catalog_showcase"
#endif
#ifndef VCF_DEMO_SCREEN
#define VCF_DEMO_SCREEN "chest"
#endif
class Consumer:public endstone::Plugin {
 vcf_api api_{};std::unique_ptr<oni::vcf::sdk::Client> ui_;int callbacks_=0;
 std::set<vcf_handle> tickets_;std::shared_ptr<endstone::Task> cleanup_;
 static vcf_status VCF_CALL action(void*context,const vcf_event*){
  ++static_cast<Consumer*>(context)->callbacks_;return VCF_OK;
 }
public:
 void onEnable()override{
  try{
   api_=oni::vcf::sdk::discover();ui_=std::make_unique<oni::vcf::sdk::Client>(api_,VCF_CONSUMER_ID);
   ui_->action("selected",action,this,"remoteworkstations.use",true);
   uint32_t count=0;oni::vcf::sdk::checked(api_.catalog_count(&count));
   for(uint32_t n=0;n<count;n++){auto c=ui_->capability(n);if(ui_->resolve(std::string_view(c.id.data,c.id.length))!=n)throw std::runtime_error("Catalog resolution mismatch");}
   cleanup_=getServer().getScheduler().runTaskTimer(*this,[this]{
    try{
     for(auto it=tickets_.begin();it!=tickets_.end();){
      if(ui_->info(*it).state==VCF_TERMINAL){oni::vcf::sdk::checked(api_.forget(ui_->owner(),*it));it=tickets_.erase(it);}
      else ++it;
     }
    }catch(const std::exception&e){getLogger().error("Consumer ticket cleanup refused: {}",e.what());}
   },1,1);
   getLogger().info("Independent native consumer connected through C ABI; {} entries resolve. Gameplay mode remains subject to provider capability.",count);
  }catch(const std::exception&e){getLogger().error("Consumer disabled: {}",e.what());ui_.reset();}
 }
 void onDisable()override{
  if(cleanup_)cleanup_->cancel();tickets_.clear();
  if(ui_){auto status=ui_->dispose();if(status!=VCF_OK)getLogger().error("Consumer revocation refused with {}; process restart required before unload.",status);ui_.reset();}
 }
 bool onCommand(endstone::CommandSender&sender,const endstone::Command&,const std::vector<std::string>&args)override{
  if(!ui_){sender.sendErrorMessage("VCF provider unavailable.");return true;}
  auto*p=sender.asPlayer();if(!p||(!args.empty()&&args[0]=="status")){sender.sendMessage("C ABI consumer active; callbacks "+std::to_string(callbacks_)+"; outstanding tickets "+std::to_string(tickets_.size()));return true;}
  try{
   if(tickets_.size()>=128)throw std::runtime_error("Example ticket limit reached.");
   auto id=p->getUniqueId().str();
   if(!args.empty()&&args[0]=="prepare"){
    auto session=ui_->prepare(id,VCF_DEMO_SCREEN,VCF_TRANSIENT,"Native consumer definition");
    tickets_.insert(session);
    // Preparation creates provider-owned state. A refused native opening must
    // remain a refusal, never become a chest/form fallback.
    ui_->open(session);sender.sendMessage("Requested "+std::string(VCF_DEMO_SCREEN)+"; completion is asynchronous and remains unqualified.");
   }else{
    auto button=oni::vcf::sdk::descriptor<vcf_button>();button.label=oni::vcf::sdk::view("Run this consumer's action");button.action=oni::vcf::sdk::view("selected");
    auto menu=oni::vcf::sdk::descriptor<vcf_menu_desc>();menu.player=oni::vcf::sdk::view(id);menu.title=oni::vcf::sdk::view(VCF_CONSUMER_ID);menu.content=oni::vcf::sdk::view("Native C ABI form/action test. This is not a native workstation demonstration.");menu.buttons=&button;menu.button_count=1;vcf_handle ticket=0;
    oni::vcf::sdk::checked(api_.show_menu(ui_->owner(),&menu,&ticket));
    tickets_.insert(ticket);
   }
  }catch(const std::exception&e){sender.sendErrorMessage(e.what());}
  return true;
 }
};
ENDSTONE_PLUGIN(VCF_CONSUMER_ID,"0.5.0-native.1",Consumer){
 description="Native SDK contract consumer; per-screen gameplay qualification is incomplete";
 depend={"onistone_vcf"};
 command(VCF_CONSUMER_ID).description("Native SDK test menu or prepare request").usages("/"+std::string(VCF_CONSUMER_ID)+" [action: string]").permissions("remoteworkstations.use");
}
