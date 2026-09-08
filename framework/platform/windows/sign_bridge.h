// Included inside windows_bridge.cpp's private namespace. All addresses are
// full-function hash checked in the compiled exact-build manifest.
struct SignBinding { void* player; void* region; void* actor; std::int64_t identity; };
struct SignLease {
    Position position;
    uintptr_t region_identity, actor_identity;
    uint64_t generation;
    ULONGLONG expires;
    void* accepted_packet{}; // Equality only; never dereferenced or owned.
    bool accepting{}, callback_seen{};
    unsigned char actor_type=4;
};
std::unordered_map<std::int64_t,SignLease> sign_leases;
struct SignSave {
    std::shared_ptr<void> packet;
    std::int64_t player_identity;
    uint64_t generation;
    bool cancelled=false, callback_seen=false;
};
// Retain a real native shared_ptr, preventing pointer reuse while a filtered
// callback is pending. Once BDS releases its copy, the record is collectible.
std::unordered_map<void*,SignSave> sign_saves;
bool sign_hooks_installed=false;
constexpr std::array<uintptr_t,2> SIGN_GATE_SITES{0xa9368a,0xa9405d};

std::int64_t sign_player_id(void* native) {
    return *reinterpret_cast<const std::int64_t* (*)(void*)>(checked(false,0xde8bf0))(native);
}
SignBinding bind_sign(void* native,Position position,unsigned char actor_type=4) {
    if(position.x < -30000000 || position.x > 30000000 || position.z < -30000000
       || position.z > 30000000 || position.y < -64 || position.y > 319)
        throw std::runtime_error("Sign source is outside the supported world bounds");
    auto region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40))(native);
    auto actor=reinterpret_cast<void* (*)(void*,const Position*)>(checked(false,0x14fb650))(region,&position);
    if(actor_type!=4 && actor_type!=36) throw std::runtime_error("Unknown block editor type");
    if(!actor || field<unsigned char>(actor,20)!=actor_type
       || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+(actor_type==4 ? 0xa72eed0 : 0xa77ec50)
       || std::memcmp(static_cast<char*>(actor)+8,&position,sizeof(position)))
        throw std::runtime_error("A loaded native sign source is required");
    auto table=field<void*>(actor,0);
    const auto entries=actor_type==4
        ? std::array<std::pair<size_t,uintptr_t>,3>{{{18,0x4627ab0},{19,0x4627f80},{20,0x462a640}}}
        : std::array<std::pair<size_t,uintptr_t>,3>{{{18,0x290b8c0},{19,0x2902f00},{20,0xa2650}}};
    for(auto [slot,rva]:entries)
        if(field<void*>(table,slot*8)!=checked(false,rva)) throw std::runtime_error("Native sign dispatch changed");
    if(actor_type==36 && field<void*>(table,8)!=checked(false,0x5ee64b0))
        throw std::runtime_error("Jigsaw native load dispatch changed");
    return {native,region,actor,sign_player_id(native)};
}
bool sign_matches(const SignBinding& bound,const SignLease& lease) {
    return lease.region_identity==reinterpret_cast<uintptr_t>(bound.region)
        && lease.actor_identity==reinterpret_cast<uintptr_t>(bound.actor);
}
bool sign_owned(const SignBinding& bound) {
    return field<std::int64_t>(bound.actor,0x100)==bound.identity;
}
bool sign_occupied(const SignBinding& bound) {
    const auto editor=field<std::int64_t>(bound.actor,0x100);
    if(editor==-1) return false;
    // Match Player::openSign: an editor that is no longer online may be
    // replaced. Persistent stale IDs are not a permanent source lock.
    auto level=field<void*>(bound.player,0x1d8);
    return reinterpret_cast<void* (*)(void*,std::int64_t,bool)>(checked(false,0x739ab0))(level,editor,false)!=nullptr;
}
bool sign_can_edit(const SignBinding& bound) {
    if(field<unsigned char>(bound.actor,20)==36)
        return reinterpret_cast<bool (*)(void*)>(checked(false,0x2202f0))(bound.player);
    return reinterpret_cast<bool (*)(void*,void*)>(checked(false,0x462a640))(bound.actor,bound.player);
}
void release_sign_binding(const SignBinding& bound) {
    if(!sign_owned(bound)) return;
    // Feed only the native actor's CURRENT authoritative state back into its
    // own apply callback. No client NBT enters this cancellation operation.
    // 46285d3..46285e1 clears LockedForEditingBy and marks the actor dirty.
    void* packet=nullptr;
    reinterpret_cast<void* (*)(void*,void**,void*)>(checked(false,0x4627ab0))(bound.actor,&packet,bound.region);
    auto destroy=checked(false,0x3829da0);
    if(!packet || field<void*>(field<void*>(packet,0),0)!=destroy)
        throw std::runtime_error("Unknown authoritative sign packet allocation");
    try {
        if(std::memcmp(static_cast<char*>(packet)+0x30,static_cast<char*>(bound.actor)+8,sizeof(Position)))
            throw std::runtime_error("Sign packet source mismatch");
        reinterpret_cast<void (*)(void*,const void*,void*)>(checked(false,0x4627f80))(
            bound.actor,static_cast<char*>(packet)+0x40,bound.region);
    } catch(...) {
        reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1); throw;
    }
    reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1);
    if(sign_owned(bound)) throw std::runtime_error("Native sign editor lock was not released");
}
void collect_sign_saves() {
    for(auto it=sign_saves.begin();it!=sign_saves.end();) {
        if(it->second.packet.use_count()==1) it=sign_saves.erase(it);
        else ++it;
    }
}
unsigned int allow_sign_distance(void* native,void* packet,unsigned int stage,
                                const std::shared_ptr<Packet>* shared) noexcept {
    if(std::this_thread::get_id()!=owner || stage>1) return 0;
    bool tracked=false, owned_source=false;
    try {
        auto saved=sign_saves.find(packet);
        tracked=saved!=sign_saves.end();
        auto identity=sign_player_id(native);
        if(tracked && (saved->second.cancelled || saved->second.callback_seen
                       || saved->second.player_identity!=identity || stage==0)) return 2;
        auto found=sign_leases.find(identity);
        if(found==sign_leases.end()) return tracked ? 2 : 0;
        auto& lease=found->second;
        auto position=field<Position>(packet,0x30);
        if(std::memcmp(&position,&lease.position,sizeof(position))) return tracked ? 2 : 0;
        owned_source=true;
        if(!lease.accepting || GetTickCount64()>lease.expires) return 2;
        if(tracked && saved->second.generation!=lease.generation) return 2;
        auto point=field<void*>(native,0x218);
        for(unsigned int i=0;i<3;++i) if(!std::isfinite(field<float>(point,i*4))) return 2;
        auto bound=bind_sign(native,position,lease.actor_type);
        if(!sign_matches(bound,lease) || !sign_can_edit(bound)) return 2;
        if(stage==0) {
            collect_sign_saves();
            if(lease.accepted_packet || sign_saves.size()>=128 || !shared || shared->get()!=packet
               || shared->use_count()<1) return 2;
            sign_saves.emplace(packet,SignSave{std::shared_ptr<void>(*shared,packet),identity,lease.generation});
            lease.accepted_packet=packet;
        } else {
            if(!tracked || lease.accepted_packet!=packet || lease.callback_seen) return 2;
            saved->second.callback_seen=true;
            lease.callback_seen=true;
        }
        return 1;
    } catch(...) { return tracked || owned_source ? 2 : 0; }
}
void install_sign_hooks() {
    if(sign_hooks_installed) return;
    checked(false,0xa935b0); checked(false,0xa93f90);
    const std::array<std::array<unsigned char,9>,2> expected{{
        {0x0f,0x2e,0xc1,0x0f,0x82,0xeb,0x05,0x00,0x00},
        {0x0f,0x2e,0xc1,0x0f,0x82,0xc0,0x03,0x00,0x00}}};
    for(size_t i=0;i<2;++i)
        if(std::memcmp(reinterpret_cast<char*>(bedrock)+SIGN_GATE_SITES[i],expected[i].data(),9))
            throw std::runtime_error("Sign distance instruction contract changed");
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&allow_sign_distance),&pinned)) throw std::runtime_error("Cannot pin sign hook lifetime");
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("Sign hook initialization failed");
    rw_sign_accept_0=reinterpret_cast<char*>(bedrock)+0xa93693;
    rw_sign_reject_0=reinterpret_cast<char*>(bedrock)+0xa93c7e;
    rw_sign_accept_1=reinterpret_cast<char*>(bedrock)+0xa94066;
    rw_sign_reject_1=reinterpret_cast<char*>(bedrock)+0xa94426;
    void* stubs[]{reinterpret_cast<void*>(&rw_sign_gate_0),reinterpret_cast<void*>(&rw_sign_gate_1)};
    size_t created=0;
    try {
        for(size_t i=0;i<2;++i) {
            auto target=reinterpret_cast<char*>(bedrock)+SIGN_GATE_SITES[i];
            void* unused=nullptr;
            if(MH_CreateHook(target,stubs[i],&unused)!=MH_OK) throw std::runtime_error("Sign hook creation failed");
            ++created;
            if(MH_EnableHook(target)!=MH_OK) throw std::runtime_error("Sign hook activation failed");
        }
    } catch(...) {
        for(size_t i=0;i<created;++i) {
            auto target=reinterpret_cast<char*>(bedrock)+SIGN_GATE_SITES[i];
            MH_DisableHook(target); MH_RemoveHook(target);
        }
        throw;
    }
    sign_hooks_installed=true;
}
Document sign_state(endstone::Player& player,int x,int y,int z,uint64_t generation=0) {
    auto bound=bind_sign(player_handle(player),{x,y,z});
    auto locked=field<std::int64_t>(bound.actor,0x100);
    auto found=sign_leases.find(bound.identity);
    bool active=found!=sign_leases.end() && found->second.generation==generation
        && sign_matches(bound,found->second);
    Document result;
    result["waxed"]=field<unsigned char>(bound.actor,0xf8)!=0;
    result["locked"]=locked!=-1; result["owned"]=locked==bound.identity;
    result["occupied"]=sign_occupied(bound);
    result["active"]=active;
    result["open_pending"]=field<unsigned char>(bound.actor,0x198)!=0;
    result["can_interact"]=reinterpret_cast<bool (*)(void*,unsigned char)>(checked(false,0x21f0a0))(bound.player,3);
    return result;
}
void open_sign(endstone::Player& player,int x,int y,int z,bool front,uint64_t generation) {
    auto bound=bind_sign(player_handle(player),{x,y,z});
    collect_sign_saves();
    if(sign_saves.size()>=128) throw std::runtime_error("Native sign text filtering is still busy");
    if(!generation || sign_leases.contains(bound.identity) || sign_leases.size()>=100 || !ready(bound.player))
        throw std::runtime_error("Another native editor or inventory is active");
    if(field<unsigned char>(bound.actor,0xf8) || sign_occupied(bound)
       || !reinterpret_cast<bool (*)(void*,unsigned char)>(checked(false,0x21f0a0))(bound.player,3))
        throw std::runtime_error("This sign is waxed, locked, or cannot be edited by this player");
    auto opener=checked(false,0x693da0);
    if(field<void*>(field<void*>(bound.player,0),206*8)!=opener) throw std::runtime_error("Unknown native sign opener");
    install_sign_hooks();
    sign_leases.emplace(bound.identity,SignLease{{x,y,z},reinterpret_cast<uintptr_t>(bound.region),
        reinterpret_cast<uintptr_t>(bound.actor),generation,GetTickCount64()+1500});
    try {
        reinterpret_cast<void (*)(void*,const Position*,bool)>(opener)(bound.player,&sign_leases.at(bound.identity).position,front);
        if(!sign_owned(bound)) throw std::runtime_error("BDS did not acquire the sign editor lock");
    } catch(...) { sign_leases.erase(bound.identity); release_sign_binding(bound); throw; }
}
void sign_touch(endstone::Player& player,uint64_t generation,bool accept_save=false) {
    auto native=player_handle(player);
    auto found=sign_leases.find(sign_player_id(native));
    if(found==sign_leases.end() || found->second.generation!=generation)
        throw std::runtime_error("The sign editor lease has ended");
    auto& lease=found->second;
    auto bound=bind_sign(native,lease.position,lease.actor_type);
    if(!sign_matches(bound,lease) || !sign_can_edit(bound)) throw std::runtime_error("The native sign source changed");
    if(accept_save) {
        if(lease.accepting) throw std::runtime_error("The sign save has already been submitted");
        lease.accepting=true;
    }
    lease.expires=GetTickCount64()+1500;
}
void close_sign(endstone::Player& player,uint64_t generation) {
    auto native=player_handle(player);
    auto found=sign_leases.find(sign_player_id(native));
    if(found==sign_leases.end() || found->second.generation!=generation) return;
    auto lease=found->second;
    sign_leases.erase(found); // Revoke before any late native filter callback.
    for(auto& [packet,save]:sign_saves)
        if(save.player_identity==sign_player_id(native) && save.generation==generation) save.cancelled=true;
    if(lease.actor_type==4) {
        auto bound=bind_sign(native,lease.position);
        if(sign_matches(bound,lease)) release_sign_binding(bound);
    }
}
Document jigsaw_state(endstone::Player& player,int x,int y,int z,uint64_t generation=0) {
    auto bound=bind_sign(player_handle(player),{x,y,z},36);
    auto found=sign_leases.find(bound.identity);
    const bool active=found!=sign_leases.end() && found->second.actor_type==36
        && found->second.generation==generation && sign_matches(bound,found->second);
    Document result;
    result["active"]=active;
    result["can_interact"]=sign_can_edit(bound);
    result["save_complete"]=active && found->second.callback_seen;
    return result;
}
int open_jigsaw(endstone::Player& player,int x,int y,int z,uint64_t generation) {
    auto bound=bind_sign(player_handle(player),{x,y,z},36);
    collect_sign_saves();
    if(!generation || sign_saves.size()>=128 || sign_leases.contains(bound.identity)
       || sign_leases.size()>=100 || !ready(bound.player) || !sign_can_edit(bound))
        throw std::runtime_error("An authorized idle creative operator is required for this editor");
    for(const auto& [identity,lease]:sign_leases)
        if(lease.actor_type==36 && sign_matches(bound,lease))
            throw std::runtime_error("Another remote session owns this jigsaw source");
    install_sign_hooks();
    SignLease lease{{x,y,z},reinterpret_cast<uintptr_t>(bound.region),
        reinterpret_cast<uintptr_t>(bound.actor),generation,GetTickCount64()+1500};
    lease.actor_type=36;
    sign_leases.emplace(bound.identity,lease);
    try {
        // Exact ContainerType 32 branch at 17e8796..17e885f. The source
        // variant is 24 bytes, BlockPos at +0 and variant index 0 at +16.
        alignas(8) std::array<unsigned char,24> source{};
        std::memcpy(source.data(),&lease.position,12);
        const auto window=reinterpret_cast<int (*)(void*,int,const void*)>(checked(false,0x693f80))(
            bound.player,32,source.data());
        std::string packet;
        packet.push_back(static_cast<char>(window)); packet.push_back(32);
        append_signed(packet,x); append_signed(packet,y); append_signed(packet,z); packet.push_back(1);
        player.sendPacket(46,packet);
        return window;
    } catch(...) { sign_leases.erase(bound.identity); throw; }
}
void shutdown_sign_hooks() {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    if(!sign_leases.empty()) throw std::runtime_error("Close active sign editors before removing hooks");
    collect_sign_saves();
    // Pending native callbacks still need the cancellation guard. The pinned
    // companion retains only their native shared ownership until they finish.
    // A later clean shutdown/open collects finished records and removes hooks.
    if(!sign_saves.empty()) return;
    if(!sign_hooks_installed) return;
    for(auto site:SIGN_GATE_SITES) {
        auto target=reinterpret_cast<char*>(bedrock)+site;
        if(MH_DisableHook(target)!=MH_OK || MH_RemoveHook(target)!=MH_OK)
            throw std::runtime_error("Sign hook removal failed");
    }
    sign_hooks_installed=false;
}
void abandon_sign(uint64_t generation) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    for(auto it=sign_leases.begin();it!=sign_leases.end();) {
        if(it->second.generation==generation) it=sign_leases.erase(it);
        else ++it;
    }
    for(auto& [packet,save]:sign_saves) if(save.generation==generation) save.cancelled=true;
    collect_sign_saves();
}
