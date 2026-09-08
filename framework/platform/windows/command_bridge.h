// Exact-build command editor, included in the bridge's private namespace.
// BDS owns packet construction, filtering, command execution and block saves.
struct CommandBinding { void* player; void* region; void* actor; std::int64_t identity; };
struct CommandLease;
struct CommandSave {
    std::weak_ptr<CommandLease> lease;
    bool applied=false;
    bool consuming=false;
    std::atomic<unsigned int> copies{0};
};
struct CommandLease {
    Position position;
    uintptr_t region_identity, actor_identity;
    uint64_t generation;
    endstone::UUID player_id;
    endstone::Server* server;
    std::string permission;
    ULONGLONG expires;
    bool accepting=false;
    std::shared_ptr<CommandSave> save;
    std::int64_t entity_id=0;
    std::uint64_t entity_runtime=0;
};
std::unordered_map<std::int64_t,std::shared_ptr<CommandLease>> command_leases;
std::mutex command_payload_mutex;
std::unordered_map<const void*,std::shared_ptr<CommandSave>> command_payloads;
using CommandHandler=void (*)(void*,const void*,const void*);
using CommandCopy=void* (*)(void*,const void*);
using CommandDestroy=void (*)(void*);
using CommandApply=void (*)(void*,const void*,const void*);
CommandHandler command_handler_original{};
CommandCopy command_copy_original{};
CommandDestroy command_destroy_original{};
CommandApply command_apply_original{};
bool command_hooks_installed=false;
bool command_stopping=false;
struct CommandFrame {
    const void* packet;
    CommandFrame* previous;
    bool tracked=false;
};
thread_local CommandFrame* command_frame{};

CommandBinding bind_command(void* native,Position position) {
    if(position.x < -30000000 || position.x > 30000000 || position.z < -30000000
       || position.z > 30000000 || position.y < -64 || position.y > 319)
        throw std::runtime_error("Command source outside admitted world bounds");
    auto region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40))(native);
    auto actor=reinterpret_cast<void* (*)(void*,const Position*)>(checked(false,0x14fb650))(region,&position);
    if(!actor || field<unsigned char>(actor,20)!=26
       || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+0xa6bbe00
       || std::memcmp(static_cast<char*>(actor)+8,&position,sizeof(position))
       || field<void*>(field<void*>(actor,0),18*8)!=checked(false,0x291f3d0))
        throw std::runtime_error("A loaded native command block is required");
    return {native,region,actor,sign_player_id(native)};
}
bool command_matches(const CommandBinding& bound,const CommandLease& lease) {
    return reinterpret_cast<uintptr_t>(bound.region)==lease.region_identity
        && reinterpret_cast<uintptr_t>(bound.actor)==lease.actor_identity;
}
bool command_authorized(const CommandBinding& bound) {
    return reinterpret_cast<bool (*)(void*)>(checked(false,0x2202f0))(bound.player);
}
CommandBinding bind_command_entity(void* native,std::int64_t identity,std::uint64_t runtime_id) {
    if(identity==0 || identity==-1 || !runtime_id) throw std::runtime_error("Invalid command cart identity");
    auto level=field<void*>(native,0x1d8);
    auto unique_lookup=checked(false,0x739ab0);
    auto runtime_lookup=checked(false,0x739f60);
    checked(false,0x14e7fc0);
    if(field<void*>(field<void*>(level,0),0x1f0)!=unique_lookup
       || field<void*>(field<void*>(level,0),0x200)!=runtime_lookup)
        throw std::runtime_error("Command cart identity lookup changed");
    auto actor=reinterpret_cast<void* (*)(void*,std::int64_t,bool)>(unique_lookup)(level,identity,false);
    auto by_runtime=reinterpret_cast<void* (*)(void*,std::uint64_t,bool)>(runtime_lookup)(level,runtime_id,false);
    if(!actor || actor!=by_runtime || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+0xa6d8d00)
        throw std::runtime_error("The original native command cart is unavailable");
    auto get_region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40));
    auto region=get_region(native);
    if(get_region(actor)!=region) throw std::runtime_error("The command cart left the player dimension");
    auto items=field<void*>(field<void*>(actor,0x128),0);
    auto marker=field<void*>(items,70*8);
    if(!marker || field<unsigned char>(marker,8)!=0 || field<unsigned short>(marker,10)!=70
       || field<unsigned char>(marker,12)!=1) throw std::runtime_error("Native command cart component is absent");
    return {native,region,actor,sign_player_id(native)};
}
CommandBinding command_source(void* native,const CommandLease& lease) {
    return lease.entity_runtime ? bind_command_entity(native,lease.entity_id,lease.entity_runtime)
                                : bind_command(native,lease.position);
}
EntityBinding command_entity_binding(endstone::Player& player,endstone::Actor& source) {
    if(!source.isValid() || source.getType()!="minecraft:command_block_minecart"
       || source.getDimension().getName()!=player.getDimension().getName())
        throw std::runtime_error("A live command cart in the player dimension is required");
    auto bound=bind_command_entity(player_handle(player),source.getId(),source.getRuntimeId());
    return {bound.player,bound.actor,source.getId(),0,0};
}
bool command_target_matches(const void* packet,const CommandLease& lease) {
    if(lease.entity_runtime)
        return field<unsigned char>(packet,0x40)==0 && field<std::uint64_t>(packet,0x30)==lease.entity_runtime;
    return field<unsigned char>(packet,0x40)==1
        && !std::memcmp(static_cast<const char*>(packet)+0x30,&lease.position,sizeof(Position));
}
void erase_command_payload(const void* payload) {
    std::lock_guard lock(command_payload_mutex);
    command_payloads.erase(payload);
}
std::shared_ptr<CommandSave> command_payload(const void* payload) {
    std::lock_guard lock(command_payload_mutex);
    auto it=command_payloads.find(payload);
    return it==command_payloads.end() ? nullptr : it->second;
}
void command_handler(void* callback,const void* network,const void* packet) {
    CommandFrame frame{packet,command_frame};
    command_frame=&frame;
    try {
        command_handler_original(callback,network,packet);
    } catch(...) {
        if(frame.tracked) erase_command_payload(static_cast<const char*>(packet)+0x30);
        command_frame=frame.previous;
        throw;
    }
    if(frame.tracked) erase_command_payload(static_cast<const char*>(packet)+0x30);
    command_frame=frame.previous;
}
void* command_copy(void* target,const void* source) {
    auto save=command_payload(source);
    // The pinned handler creates three successive copies for its filter job;
    // each uses this constructor and the paired non-deleting destructor.
    if(save) {
        std::lock_guard lock(command_payload_mutex);
        if(command_payloads.size()>=4096) throw std::runtime_error("Native editor copy limit reached");
        command_payloads.insert_or_assign(target,save);
    } else erase_command_payload(target);
    try {
        auto result=command_copy_original(target,source);
        if(save) ++save->copies;
        return result;
    } catch(...) { if(save) erase_command_payload(target); throw; }
}
void command_destroy(void* payload) {
    erase_command_payload(payload);
    command_destroy_original(payload);
}
bool command_save_valid(const std::shared_ptr<CommandSave>& save,void* region,const void* packet) {
    if(std::this_thread::get_id()!=owner || save->applied || save->consuming) return false;
    auto lease=save->lease.lock();
    if(!lease || !lease->accepting || GetTickCount64()>lease->expires
       || !command_target_matches(packet,*lease)) return false;
    auto player=lease->server->getPlayer(lease->player_id);
    if(!player || !player->isValid() || player->isDead() || !player->isOp()
       || player->getGameMode()!=endstone::GameMode::Creative
       || !player->hasPermission("remoteworkstations.admin")
       || !player->hasPermission("remoteworkstations.use")
       || !player->hasPermission(lease->entity_runtime ? "remoteworkstations.open.commandblockminecart" : "remoteworkstations.open.commandblock")
       || !player->hasPermission(lease->permission)) return false;
    auto bound=command_source(player_handle(*player),*lease);
    return bound.region==region && command_matches(bound,*lease) && command_authorized(bound);
}
void command_apply(void* region,const void* packet,const void* filtered_name) {
    auto save=command_payload(static_cast<const char*>(packet)+0x30);
    if(!save) { command_apply_original(region,packet,filtered_name); return; }
    try {
        if(!command_save_valid(save,region,packet)) return;
    } catch(...) { return; }
    save->consuming=true;
    command_apply_original(region,packet,filtered_name);
    save->applied=true;
}
unsigned int allow_command_distance(void* native,const void* packet) noexcept {
    if(std::this_thread::get_id()!=owner) return 0;
    bool owned=false;
    try {
        auto found=command_leases.find(sign_player_id(native));
        if(found==command_leases.end()) return 0;
        auto lease=found->second;
        if(!command_target_matches(packet,*lease)) return 0;
        owned=true;
        if(!command_frame || command_frame->packet!=packet || command_frame->tracked
           || !lease->accepting || lease->save || GetTickCount64()>lease->expires) return 2;
        auto bound=command_source(native,*lease);
        if(!command_matches(bound,*lease) || !command_authorized(bound)) return 2;
        auto save=std::make_shared<CommandSave>();
        save->lease=lease;
        {
            std::lock_guard lock(command_payload_mutex);
            if(command_payloads.size()>=1024) return 2;
            command_payloads.insert_or_assign(static_cast<const char*>(packet)+0x30,save);
        }
        command_frame->tracked=true;
        lease->save=save;
        return 1;
    } catch(...) { return owned ? 2 : 0; }
}
constexpr std::array<uintptr_t,6> COMMAND_HOOK_SITES{0xa64cb0,0xaf9d30,0xafa0f0,0xa659a0,0xa64f3b,0xa6500f};
void install_command_hooks() {
    if(command_hooks_installed) return;
    for(size_t i=0;i<4;++i) checked(false,COMMAND_HOOK_SITES[i]);
    checked(false,0xaf9840);
    auto gate=reinterpret_cast<unsigned char*>(bedrock)+COMMAND_HOOK_SITES[4];
    constexpr unsigned char expected[]{0x0f,0x2e,0xc1,0x0f,0x82,0x0e,0x01,0,0};
    if(std::memcmp(gate,expected,sizeof(expected))) throw std::runtime_error("Command distance gate changed");
    auto entity_gate=reinterpret_cast<unsigned char*>(bedrock)+COMMAND_HOOK_SITES[5];
    constexpr unsigned char entity_expected[]{0x0f,0x2e,0xc1,0x72,0x3e};
    if(std::memcmp(entity_gate,entity_expected,sizeof(entity_expected))) throw std::runtime_error("Command entity distance gate changed");
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&command_apply),&pinned)) throw std::runtime_error("Cannot pin command hook lifetime");
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("Command hooks unavailable");
    rw_command_accept=reinterpret_cast<char*>(bedrock)+0xa64f44;
    rw_command_reject=reinterpret_cast<char*>(bedrock)+0xa65052;
    rw_command_entity_accept=reinterpret_cast<char*>(bedrock)+0xa65014;
    rw_command_entity_reject=reinterpret_cast<char*>(bedrock)+0xa65052;
    void* hooks[]{reinterpret_cast<void*>(&command_handler),reinterpret_cast<void*>(&command_copy),
        reinterpret_cast<void*>(&command_destroy),reinterpret_cast<void*>(&command_apply),reinterpret_cast<void*>(&rw_command_gate),
        reinterpret_cast<void*>(&rw_command_entity_gate)};
    void* originals[6]{};
    size_t created=0;
    try {
        for(size_t i=0;i<COMMAND_HOOK_SITES.size();++i) {
            auto target=reinterpret_cast<char*>(bedrock)+COMMAND_HOOK_SITES[i];
            if(MH_CreateHook(target,hooks[i],&originals[i])!=MH_OK) throw std::runtime_error("Command hook creation failed");
            ++created;
        }
        command_handler_original=reinterpret_cast<CommandHandler>(originals[0]);
        command_copy_original=reinterpret_cast<CommandCopy>(originals[1]);
        command_destroy_original=reinterpret_cast<CommandDestroy>(originals[2]);
        command_apply_original=reinterpret_cast<CommandApply>(originals[3]);
        for(size_t i=0;i<COMMAND_HOOK_SITES.size();++i)
            if(MH_EnableHook(reinterpret_cast<char*>(bedrock)+COMMAND_HOOK_SITES[i])!=MH_OK)
                throw std::runtime_error("Command hook activation failed");
        // Only the verified owner thread executes this packet handler. Convert
        // the installed relative branch to CALL before returning to its loop.
        // The relay remains a leaf JMP into our normal unwound assembly frame.
        for(auto call_gate: {gate,entity_gate}) {
            if(*call_gate!=0xe9) throw std::runtime_error("Unexpected command gate relay");
            DWORD previous{}, ignored{};
            if(!VirtualProtect(call_gate,1,PAGE_EXECUTE_READWRITE,&previous)) throw std::runtime_error("Command call gate protection failed");
            *call_gate=0xe8;
            const bool restored=VirtualProtect(call_gate,1,previous,&ignored)!=0;
            FlushInstructionCache(GetCurrentProcess(),call_gate,5);
            if(!restored) throw std::runtime_error("Command call gate protection restore failed");
        }
    } catch(...) {
        for(size_t i=0;i<created;++i) {
            auto target=reinterpret_cast<char*>(bedrock)+COMMAND_HOOK_SITES[i];
            MH_DisableHook(target); MH_RemoveHook(target);
        }
        throw;
    }
    command_hooks_installed=true;
}
Document command_state(endstone::Player& player,int x,int y,int z,uint64_t generation=0) {
    auto bound=bind_command(player_handle(player),{x,y,z});
    auto found=command_leases.find(bound.identity);
    auto lease=found==command_leases.end() ? nullptr : found->second;
    const bool active=lease && lease->generation==generation && command_matches(bound,*lease);
    Document result;
    result["active"]=active;
    result["can_interact"]=command_authorized(bound);
    result["save_complete"]=lease && lease->generation==generation && lease->save && lease->save->applied;
    result["native_payload_copies"]=lease && lease->save ? lease->save->copies.load() : 0;
    return result;
}
Document command_entity_state(endstone::Player& player,endstone::Actor& source,uint64_t generation=0) {
    command_entity_binding(player,source);
    auto bound=bind_command_entity(player_handle(player),source.getId(),source.getRuntimeId());
    auto found=command_leases.find(bound.identity);
    auto lease=found==command_leases.end() ? nullptr : found->second;
    Document result;
    result["active"]=lease && lease->generation==generation && command_matches(bound,*lease)
        && lease->entity_id==source.getId() && lease->entity_runtime==source.getRuntimeId();
    result["can_interact"]=command_authorized(bound);
    result["save_complete"]=lease && lease->generation==generation && lease->save && lease->save->applied;
    result["native_payload_copies"]=lease && lease->save ? lease->save->copies.load() : 0;
    result["actor_id"]=source.getId(); result["runtime_id"]=source.getRuntimeId();
    return result;
}
int open_command_entity(endstone::Player& player,endstone::Actor& source,uint64_t generation,const std::string& permission) {
    if(command_stopping) throw std::runtime_error("Restart the server after command editor shutdown");
    command_entity_binding(player,source);
    auto bound=bind_command_entity(player_handle(player),source.getId(),source.getRuntimeId());
    if(!generation || !ready(bound.player) || !command_authorized(bound)
       || command_leases.contains(bound.identity) || sign_leases.contains(bound.identity) || command_leases.size()>=100)
        throw std::runtime_error("An authorized idle creative operator is required for this editor");
    for(const auto& [identity,lease]:command_leases)
        if(command_matches(bound,*lease)) throw std::runtime_error("Another remote editor owns this command cart");
    auto mode=field<void*>(bound.player,2720);
    auto interact=field<void*>(field<void*>(mode,0),14*sizeof(void*));
    // Rejoined Creative players can retain the native SurvivalMode wrapper.
    // Its pinned branch delegates to the same full GameMode interaction body.
    if(field<void*>(mode,8)!=bound.player || (interact!=checked(false,0x1f36410)
       && interact!=checked(false,0x1f3d5d0)))
        throw std::runtime_error("Native Creative interaction dispatch changed");
    checked(false,0x276e6c0);
    install_command_hooks();
    auto lease=std::make_shared<CommandLease>(CommandLease{{0,0,0},reinterpret_cast<uintptr_t>(bound.region),
        reinterpret_cast<uintptr_t>(bound.actor),generation,player.getUniqueId(),&player.getServer(),permission,GetTickCount64()+1500});
    lease->entity_id=source.getId(); lease->entity_runtime=source.getRuntimeId();
    command_leases.emplace(bound.identity,lease);
    try {
        const std::array<float,3> hit{0.0f,1.0f,0.0f};
        if(!reinterpret_cast<bool (*)(void*,void*,const float*)>(interact)(mode,bound.actor,hit.data()))
            throw std::runtime_error("The command cart declined the interaction");
        return field<unsigned char>(bound.player,3480);
    } catch(...) { command_leases.erase(bound.identity); throw; }
}
int open_command(endstone::Player& player,int x,int y,int z,uint64_t generation,const std::string& permission) {
    if(command_stopping) throw std::runtime_error("Restart the server after command editor shutdown");
    auto bound=bind_command(player_handle(player),{x,y,z});
    if(!generation || !ready(bound.player) || !command_authorized(bound)
       || command_leases.contains(bound.identity) || sign_leases.contains(bound.identity) || command_leases.size()>=100)
        throw std::runtime_error("An authorized idle creative operator is required for this editor");
    for(const auto& [identity,lease]:command_leases)
        if(command_matches(bound,*lease)) throw std::runtime_error("Another remote editor owns this command block");
    install_command_hooks();
    auto lease=std::make_shared<CommandLease>(CommandLease{{x,y,z},reinterpret_cast<uintptr_t>(bound.region),
        reinterpret_cast<uintptr_t>(bound.actor),generation,player.getUniqueId(),&player.getServer(),permission,GetTickCount64()+1500});
    command_leases.emplace(bound.identity,lease);
    try {
        alignas(8) std::array<unsigned char,24> source{};
        std::memcpy(source.data(),&lease->position,sizeof(Position));
        const auto window=reinterpret_cast<int (*)(void*,int,const void*)>(checked(false,0x693f80))(
            bound.player,16,source.data());
        std::string packet;
        packet.push_back(static_cast<char>(window)); packet.push_back(16);
        append_signed(packet,x); append_signed(packet,y); append_signed(packet,z); packet.push_back(1);
        player.sendPacket(46,packet);
        return window;
    } catch(...) { command_leases.erase(bound.identity); throw; }
}
void command_touch(endstone::Player& player,uint64_t generation,bool accept_save=false) {
    auto native=player_handle(player);
    auto found=command_leases.find(sign_player_id(native));
    if(found==command_leases.end() || found->second->generation!=generation)
        throw std::runtime_error("Command editor lease ended");
    auto& lease=*found->second;
    auto bound=command_source(native,lease);
    if(!command_matches(bound,lease) || !command_authorized(bound)) throw std::runtime_error("Command source admission changed");
    if(accept_save) {
        if(lease.accepting) throw std::runtime_error("Command save already submitted");
        lease.accepting=true;
    }
    lease.expires=GetTickCount64()+1500;
}
void abandon_command(uint64_t generation) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    for(auto it=command_leases.begin();it!=command_leases.end();) {
        if(it->second->generation==generation) it=command_leases.erase(it); else ++it;
    }
}
void close_command(endstone::Player& player,uint64_t generation) {
    auto found=command_leases.find(sign_player_id(player_handle(player)));
    if(found!=command_leases.end() && found->second->generation==generation) command_leases.erase(found);
}
void shutdown_command_hooks() {
    if(std::this_thread::get_id()!=owner || !command_leases.empty())
        throw std::runtime_error("Close command editor leases before shutdown");
    // A native filter worker can still be inside an untracked copy/destructor.
    // Keep this pinned module and its trampolines alive until process exit.
    // With every lease revoked, wrappers only preserve normal native behavior
    // and deny retired owned saves. No Python callback or object is retained.
    command_stopping=true;
}
