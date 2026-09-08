// Exact BDS StructureEditorData ownership. Actual native save/load/filtering
// executes unchanged after a revocable player/source/generation admission.
struct StructureLease;
struct StructureSave {
    std::weak_ptr<StructureLease> lease;
    bool applied=false, consuming=false;
    std::atomic<unsigned int> copies{0}, moves{0};
};
struct StructureLease {
    Position position;
    uintptr_t region_identity, actor_identity;
    uint64_t generation;
    endstone::UUID player_id;
    endstone::Server* server;
    std::string permission;
    ULONGLONG expires;
    bool accepting=false;
    std::shared_ptr<StructureSave> save;
    unsigned int completed=0;
};
std::unordered_map<std::int64_t,std::shared_ptr<StructureLease>> structure_leases;
std::mutex structure_payload_mutex;
std::unordered_map<const void*,std::shared_ptr<StructureSave>> structure_payloads;
using StructureHandler=void (*)(void*,const void*,const void*);
using StructureCopy=void* (*)(void*,const void*);
using StructureDestroy=void (*)(void*);
// The first argument is {BlockSource*, legacy_namespace}; R8 and R9 are
// distinct native filter result vectors. Preserve all four arguments.
using StructureApply=void (*)(void*,const void*,const void*,const void*);
StructureHandler structure_handler_original{};
StructureCopy structure_copy_original{}, structure_move_original{};
StructureDestroy structure_destroy_original{};
StructureApply structure_apply_original{};
bool structure_hooks_installed=false, structure_stopping=false;
struct StructureFrame { const void* packet; StructureFrame* previous; bool tracked=false; };
thread_local StructureFrame* structure_frame{};

CommandBinding bind_structure(void* native,Position position) {
    if(position.x < -30000000 || position.x > 30000000 || position.z < -30000000
       || position.z > 30000000 || position.y < -64 || position.y > 319)
        throw std::runtime_error("Structure source outside admitted world bounds");
    auto region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40))(native);
    auto actor=reinterpret_cast<void* (*)(void*,const Position*)>(checked(false,0x14fb650))(region,&position);
    if(!actor || field<unsigned char>(actor,20)!=32
       || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+0xa72f380
       || std::memcmp(static_cast<char*>(actor)+8,&position,sizeof(position))
       || field<void*>(field<void*>(actor,0),18*8)!=checked(false,0x290b8c0))
        throw std::runtime_error("A loaded native structure block is required");
    return {native,region,actor,sign_player_id(native)};
}
bool structure_matches(const CommandBinding& bound,const StructureLease& lease) {
    return reinterpret_cast<uintptr_t>(bound.region)==lease.region_identity
        && reinterpret_cast<uintptr_t>(bound.actor)==lease.actor_identity;
}
bool structure_target(const void* packet,const StructureLease& lease) {
    return !std::memcmp(static_cast<const char*>(packet)+0x30,&lease.position,sizeof(Position));
}
void erase_structure_payload(const void* payload) {
    std::lock_guard lock(structure_payload_mutex);
    structure_payloads.erase(payload);
}
std::shared_ptr<StructureSave> structure_payload(const void* payload) {
    std::lock_guard lock(structure_payload_mutex);
    auto it=structure_payloads.find(payload);
    return it==structure_payloads.end() ? nullptr : it->second;
}
void structure_handler(void* callback,const void* network,const void* packet) {
    StructureFrame frame{packet,structure_frame}; structure_frame=&frame;
    try { structure_handler_original(callback,network,packet); }
    catch(...) {
        if(frame.tracked) erase_structure_payload(static_cast<const char*>(packet)+0x40);
        structure_frame=frame.previous; throw;
    }
    if(frame.tracked) erase_structure_payload(static_cast<const char*>(packet)+0x40);
    structure_frame=frame.previous;
}
void* structure_transfer(void* target,const void* source,bool move) {
    auto save=structure_payload(source);
    if(save) {
        std::lock_guard lock(structure_payload_mutex);
        if(structure_payloads.size()>=4096) throw std::runtime_error("Structure copy limit reached");
        structure_payloads.insert_or_assign(target,save);
    } else erase_structure_payload(target);
    try {
        auto result=(move ? structure_move_original : structure_copy_original)(target,source);
        if(save) {
            if(move) ++save->moves; else ++save->copies;
        }
        // A moved-from native object remains tracked until its destructor;
        // callbacks retaining either copy cannot escape revocation.
        return result;
    } catch(...) { if(save) erase_structure_payload(target); throw; }
}
void* structure_copy(void* target,const void* source) { return structure_transfer(target,source,false); }
void* structure_move(void* target,const void* source) { return structure_transfer(target,source,true); }
void structure_destroy(void* payload) {
    erase_structure_payload(payload); structure_destroy_original(payload);
}
bool structure_save_valid(const std::shared_ptr<StructureSave>& save,void* context,const void* packet) {
    if(std::this_thread::get_id()!=owner || save->applied || save->consuming) return false;
    auto lease=save->lease.lock();
    if(!lease || lease->save!=save || !lease->accepting || GetTickCount64()>lease->expires
       || !structure_target(packet,*lease)) return false;
    auto player=lease->server->getPlayer(lease->player_id);
    if(!player || !player->isValid() || player->isDead() || !player->isOp()
       || player->getGameMode()!=endstone::GameMode::Creative
       || !player->hasPermission("remoteworkstations.admin")
       || !player->hasPermission("remoteworkstations.use")
       || !player->hasPermission("remoteworkstations.open.structure")
       || !player->hasPermission(lease->permission)) return false;
    auto bound=bind_structure(player_handle(*player),lease->position);
    return bound.region==field<void*>(context,0) && structure_matches(bound,*lease) && command_authorized(bound);
}
void structure_apply(void* context,const void* packet,const void* filtered,const void* auxiliary) {
    auto save=structure_payload(static_cast<const char*>(packet)+0x40);
    if(!save) { structure_apply_original(context,packet,filtered,auxiliary); return; }
    try { if(!structure_save_valid(save,context,packet)) return; } catch(...) { return; }
    save->consuming=true;
    structure_apply_original(context,packet,filtered,auxiliary);
    save->applied=true;
    if(auto lease=save->lease.lock()) ++lease->completed;
}
unsigned int allow_structure_distance(void* native,const void* packet) noexcept {
    if(std::this_thread::get_id()!=owner) return 0;
    bool owned=false;
    try {
        auto found=structure_leases.find(sign_player_id(native));
        if(found==structure_leases.end()) return 0;
        auto lease=found->second;
        if(!structure_target(packet,*lease)) return 0;
        owned=true;
        if(!structure_frame || structure_frame->packet!=packet || structure_frame->tracked
           || !lease->accepting || lease->save || GetTickCount64()>lease->expires) return 2;
        auto bound=bind_structure(native,lease->position);
        if(!structure_matches(bound,*lease) || !command_authorized(bound)) return 2;
        auto save=std::make_shared<StructureSave>(); save->lease=lease;
        {
            std::lock_guard lock(structure_payload_mutex);
            if(structure_payloads.size()>=1024) return 2;
            structure_payloads.insert_or_assign(static_cast<const char*>(packet)+0x40,save);
        }
        structure_frame->tracked=true; lease->save=save;
        return 1;
    } catch(...) { return owned ? 2 : 0; }
}
constexpr std::array<uintptr_t,6> STRUCTURE_HOOK_SITES{0xa6b3b0,0xafc080,0xb0acb0,0xafc450,0xa6bd40,0xa6b480};
void install_structure_hooks() {
    if(structure_hooks_installed) return;
    for(size_t i=0;i<5;++i) checked(false,STRUCTURE_HOOK_SITES[i]);
    checked(false,0xa6bbb0); checked(false,0xa6efa0);
    auto gate=reinterpret_cast<unsigned char*>(bedrock)+STRUCTURE_HOOK_SITES[5];
    constexpr unsigned char expected[]{0x0f,0x2e,0xc1,0x0f,0x82,0xc0,0x04,0,0};
    if(std::memcmp(gate,expected,sizeof(expected))) throw std::runtime_error("Structure distance gate changed");
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&structure_apply),&pinned)) throw std::runtime_error("Cannot pin structure hook lifetime");
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("Structure hooks unavailable");
    rw_structure_accept=reinterpret_cast<char*>(bedrock)+0xa6b489;
    rw_structure_reject=reinterpret_cast<char*>(bedrock)+0xa6b949;
    void* hooks[]{reinterpret_cast<void*>(&structure_handler),reinterpret_cast<void*>(&structure_copy),
        reinterpret_cast<void*>(&structure_move),reinterpret_cast<void*>(&structure_destroy),
        reinterpret_cast<void*>(&structure_apply),reinterpret_cast<void*>(&rw_structure_gate)};
    void* originals[6]{}; size_t created=0;
    try {
        for(size_t i=0;i<STRUCTURE_HOOK_SITES.size();++i) {
            auto target=reinterpret_cast<char*>(bedrock)+STRUCTURE_HOOK_SITES[i];
            if(MH_CreateHook(target,hooks[i],&originals[i])!=MH_OK) throw std::runtime_error("Structure hook creation failed");
            ++created;
        }
        structure_handler_original=reinterpret_cast<StructureHandler>(originals[0]);
        structure_copy_original=reinterpret_cast<StructureCopy>(originals[1]);
        structure_move_original=reinterpret_cast<StructureCopy>(originals[2]);
        structure_destroy_original=reinterpret_cast<StructureDestroy>(originals[3]);
        structure_apply_original=reinterpret_cast<StructureApply>(originals[4]);
        for(size_t i=0;i<STRUCTURE_HOOK_SITES.size();++i)
            if(MH_EnableHook(reinterpret_cast<char*>(bedrock)+STRUCTURE_HOOK_SITES[i])!=MH_OK)
                throw std::runtime_error("Structure hook activation failed");
        if(*gate!=0xe9) throw std::runtime_error("Unexpected structure gate relay");
        DWORD previous{}, ignored{};
        if(!VirtualProtect(gate,1,PAGE_EXECUTE_READWRITE,&previous)) throw std::runtime_error("Structure gate protection failed");
        *gate=0xe8;
        const bool restored=VirtualProtect(gate,1,previous,&ignored)!=0;
        FlushInstructionCache(GetCurrentProcess(),gate,5);
        if(!restored) throw std::runtime_error("Structure gate protection restore failed");
    } catch(...) {
        for(size_t i=0;i<created;++i) {
            auto target=reinterpret_cast<char*>(bedrock)+STRUCTURE_HOOK_SITES[i];
            MH_DisableHook(target); MH_RemoveHook(target);
        }
        throw;
    }
    structure_hooks_installed=true;
}
Document structure_state(endstone::Player& player,int x,int y,int z,uint64_t generation=0) {
    auto bound=bind_structure(player_handle(player),{x,y,z});
    auto found=structure_leases.find(bound.identity);
    auto lease=found==structure_leases.end() ? nullptr : found->second;
    const bool active=lease && lease->generation==generation && structure_matches(bound,*lease);
    Document result; result["active"]=active; result["can_interact"]=command_authorized(bound);
    result["save_complete"]=active && lease->save && lease->save->applied;
    result["pending"]=active && lease->accepting && (!lease->save || !lease->save->applied);
    result["completed_updates"]=active ? lease->completed : 0;
    result["native_payload_copies"]=active && lease->save ? lease->save->copies.load() : 0;
    result["native_payload_moves"]=active && lease->save ? lease->save->moves.load() : 0;
    return result;
}
int open_structure(endstone::Player& player,int x,int y,int z,uint64_t generation,const std::string& permission) {
    if(structure_stopping) throw std::runtime_error("Restart the server after structure editor shutdown");
    auto bound=bind_structure(player_handle(player),{x,y,z});
    if(!generation || !ready(bound.player) || !command_authorized(bound) || structure_leases.contains(bound.identity)
       || command_leases.contains(bound.identity) || sign_leases.contains(bound.identity) || structure_leases.size()>=100)
        throw std::runtime_error("An authorized idle Creative operator is required for this editor");
    for(const auto& [identity,lease]:structure_leases)
        if(structure_matches(bound,*lease)) throw std::runtime_error("Another remote editor owns this structure block");
    install_structure_hooks();
    auto lease=std::make_shared<StructureLease>(StructureLease{{x,y,z},reinterpret_cast<uintptr_t>(bound.region),
        reinterpret_cast<uintptr_t>(bound.actor),generation,player.getUniqueId(),&player.getServer(),permission,GetTickCount64()+1500});
    structure_leases.emplace(bound.identity,lease);
    try {
        alignas(8) std::array<unsigned char,24> source{};
        std::memcpy(source.data(),&lease->position,sizeof(Position));
        auto window=reinterpret_cast<int (*)(void*,int,const void*)>(checked(false,0x693f80))(bound.player,14,source.data());
        std::string packet; packet.push_back(static_cast<char>(window)); packet.push_back(14);
        append_signed(packet,x); append_signed(packet,y); append_signed(packet,z); packet.push_back(1);
        player.sendPacket(46,packet); return window;
    } catch(...) { structure_leases.erase(bound.identity); throw; }
}
void structure_touch(endstone::Player& player,uint64_t generation,bool accept_save=false) {
    auto native=player_handle(player);
    auto found=structure_leases.find(sign_player_id(native));
    if(found==structure_leases.end() || found->second->generation!=generation)
        throw std::runtime_error("Structure editor lease ended");
    auto& lease=*found->second;
    auto bound=bind_structure(native,lease.position);
    if(!structure_matches(bound,lease) || !command_authorized(bound)) throw std::runtime_error("Structure source admission changed");
    if(accept_save) {
        if(lease.accepting && (!lease.save || !lease.save->applied)) throw std::runtime_error("Structure update is still pending");
        lease.save.reset(); lease.accepting=true;
    }
    lease.expires=GetTickCount64()+1500;
}
void abandon_structure(uint64_t generation) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    for(auto it=structure_leases.begin();it!=structure_leases.end();)
        if(it->second->generation==generation) it=structure_leases.erase(it); else ++it;
}
void close_structure(endstone::Player& player,uint64_t generation) {
    auto found=structure_leases.find(sign_player_id(player_handle(player)));
    if(found!=structure_leases.end() && found->second->generation==generation) structure_leases.erase(found);
}
void shutdown_structure_hooks() {
    if(std::this_thread::get_id()!=owner || !structure_leases.empty()) throw std::runtime_error("Close structure leases before shutdown");
    // Native filter workers may still execute untracked constructors. Keep the
    // module/trampolines pinned; revocation makes every owned pending save inert.
    structure_stopping=true;
}
