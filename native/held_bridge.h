// Exact-build held-item inventory transactions. Included inside bridge namespace.
// Inventory and its nested item contents share one BDS serialized ListTag.
using HeldSave = void** (*)(const void*,void**,const void*);
using HeldChanged = void (*)(void*,int);
using HeldSet = void (*)(void*,int,const void*,bool);
HeldSave held_original_save{};
HeldChanged held_original_changed{};
HeldSet held_original_set{};
std::recursive_mutex held_mutex;
struct HeldWatch {
    void* player{};
    void* container{};
    uint64_t token{}, epoch{};
    std::string item;
};
std::unordered_map<uint64_t,std::shared_ptr<HeldWatch>> held_watches;
uint64_t held_sequence=0;
const void* held_writing{};
void* held_before_save{};
uint64_t held_guarded_saves=0;
struct HeldHookFingerprint { void* address; unsigned long size; std::string hash; };
std::vector<HeldHookFingerprint> held_hook_fingerprints;
void held_check_hooks() {
    for(auto& row:held_hook_fingerprints) {
        readable(row.address,row.size);
        Hash hash; hash.add(row.address,row.size);
        if(hash.finish()!=row.hash) throw std::runtime_error("Held native hook was replaced");
    }
}

void* held_virtual(const void* object,size_t slot) {
    return field<void*>(field<void*>(object,0),slot*8);
}
void held_destroy_tag(void* tag) noexcept {
    // Only native tags returned by the admitted save/copy implementations enter.
    if(tag) reinterpret_cast<void (*)(void*,unsigned)>(held_virtual(tag,0))(tag,1);
}
using HeldTag=std::unique_ptr<void,decltype(&held_destroy_tag)>;
HeldTag held_tag(void* value=nullptr) { return HeldTag(value,&held_destroy_tag); }
void** held_save_guard(const void* container,void** result,const void* context) {
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    if(container==held_writing && held_before_save) {
        ++held_guarded_saves;
        return reinterpret_cast<void** (*)(const void*,void**)>(held_virtual(held_before_save,9))(held_before_save,result);
    }
    return held_original_save(container,result,context);
}
void held_changed_guard(void* container,int slot) {
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    for(auto& [_,watch]:held_watches) if(watch->container==container) ++watch->epoch;
    held_original_changed(container,slot);
}
void held_set_guard(void* container,int slot,const void* stack,bool balance) {
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    // BDS elides change notifications for identical replacement stacks.
    for(auto& [_,watch]:held_watches) if(watch->container==container) ++watch->epoch;
    held_original_set(container,slot,stack,balance);
}
void held_install() {
    if(held_original_save && held_original_changed && held_original_set) { held_check_hooks(); return; }
    auto save=checked(false,0x1f41ff0);
    auto changed=checked(false,0x2ddc80);
    auto setter=checked(false,0x2069a0);
    // Validate native ListTag copy/equality/destruction before installing hooks.
    for(auto address:{0xeac810,0x1bfc420,0x1bfc110}) checked(false,address);
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(&held_save_guard),&pinned))
        throw std::runtime_error("Cannot pin held save guard");
    auto status=MH_Initialize();
    if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED) throw std::runtime_error("Held hook initialization failed");
    if(MH_CreateHook(save,reinterpret_cast<void*>(&held_save_guard),reinterpret_cast<void**>(&held_original_save))!=MH_OK)
        throw std::runtime_error("Held save hook is occupied");
    if(MH_CreateHook(changed,reinterpret_cast<void*>(&held_changed_guard),reinterpret_cast<void**>(&held_original_changed))!=MH_OK) {
        MH_RemoveHook(save); held_original_save=nullptr;
        throw std::runtime_error("Inventory change hook is occupied");
    }
    if(MH_CreateHook(setter,reinterpret_cast<void*>(&held_set_guard),reinterpret_cast<void**>(&held_original_set))!=MH_OK) {
        MH_RemoveHook(save); MH_RemoveHook(changed);
        held_original_save=nullptr; held_original_changed=nullptr;
        throw std::runtime_error("Held inventory setter hook is occupied");
    }
    if(MH_EnableHook(save)!=MH_OK || MH_EnableHook(changed)!=MH_OK || MH_EnableHook(setter)!=MH_OK) {
        MH_DisableHook(save); MH_DisableHook(changed); MH_RemoveHook(save); MH_RemoveHook(changed);
        MH_DisableHook(setter); MH_RemoveHook(setter);
        held_original_save=nullptr; held_original_changed=nullptr; held_original_set=nullptr;
        throw std::runtime_error("Held hook activation failed");
    }
    for(auto [address,size]:{std::pair<void*,unsigned long>{save,488},{changed,73},{setter,1269}}) {
        Hash hash; hash.add(address,size);
        held_hook_fingerprints.push_back({address,size,hash.finish()});
    }
}
void* held_container(endstone::Player& player) {
    auto native=player_handle(player);
    auto container=reinterpret_cast<void* (*)(void*)>(checked(true,1093360))(native);
    readable(container,0x1b8);
    if(held_virtual(container,50)!=reinterpret_cast<char*>(bedrock)+0x1f41ff0 ||
       held_virtual(container,34)!=reinterpret_cast<char*>(bedrock)+0x2ddc80 ||
       held_virtual(container,13)!=reinterpret_cast<char*>(bedrock)+0x2069a0 || field<void*>(container,0x1b0)!=native)
        throw std::runtime_error("Unknown player inventory implementation");
    if(player.getInventory().getSize()!=36) throw std::runtime_error("Held UI requires the 36-slot player inventory");
    return container;
}
std::shared_ptr<HeldWatch> held_watch_for(endstone::Player& player,uint64_t token) {
    held_check_hooks();
    auto it=held_watches.find(token);
    if(it==held_watches.end() || it->second->player!=player_handle(player) || it->second->container!=held_container(player))
        throw std::runtime_error("Held inventory ownership expired");
    return it->second;
}
uint64_t held_watch(endstone::Player& player,const std::string& item) {
    auto container=held_container(player);
    if(item.empty() || item.size()>36) throw std::runtime_error("Held identity required");
    held_install();
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    if(held_watches.size()>=100) throw std::runtime_error("Held watch limit reached");
    for(auto& [_,w]:held_watches) if(w->item==item || w->container==container)
        throw std::runtime_error("The player or item already has a held interface");
    auto token=++held_sequence;
    held_watches.emplace(token,std::make_shared<HeldWatch>(HeldWatch{player_handle(player),container,token,0,item}));
    return token;
}
uint64_t held_epoch(endstone::Player& player,uint64_t token) {
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    return held_watch_for(player,token)->epoch;
}
void held_release(uint64_t token) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    // No player or container dereference: safe after a disconnected player dies.
    held_watches.erase(token);
}
HeldTag held_save_stack(const endstone::ItemStack& item) {
    auto impl=field<void*>(&item,0);
    auto native=static_cast<char*>(impl)+8;
    readable(native,128);
    // getData() can derive a block item's state; it need not equal raw aux +32.
    if(field<unsigned char>(native,34)!=item.getAmount())
        throw std::runtime_error("Unknown public stack representation");
    unsigned char context=0;
    void* output=nullptr;
    reinterpret_cast<HeldSave>(checked(false,0x1bcbdc0))(native,&output,&context);
    if(!output) throw std::runtime_error("Native item serialization failed");
    return held_tag(output);
}
endstone::CompoundTag held_pack(const endstone::ItemStack& item) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    auto saved=held_save_stack(item);
    auto tag=reinterpret_cast<endstone::nbt::Tag (*)(const void*)>(checked(true,1544320))(saved.get());
    return tag.get<endstone::CompoundTag>();
}
endstone::ItemStack held_clone(const endstone::ItemStack& item,int amount) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    endstone::ItemStack copy(item);
    if(amount!=-1) copy.setAmount(amount);
    return copy;
}
endstone::ItemStack held_unpack(const endstone::CompoundTag& source) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    endstone::nbt::Tag public_tag(source);
    void* native_tag=nullptr;
    reinterpret_cast<void** (*)(void**,const endstone::nbt::Tag*)>(checked(true,1546784))(&native_tag,&public_tag);
    auto tag=held_tag(native_tag);
    if(!tag) throw std::runtime_error("Native stored item tag conversion failed");
    // The actual inventory loadFromTag calls this static ItemStack constructor:
    // RCX=152-byte return storage, RDX=CompoundTag. No player inventory is used.
    alignas(16) std::array<unsigned char,152> storage{};
    reinterpret_cast<void* (*)(void*,const void*)>(checked(false,0x1bc4b20))(storage.data(),tag.get());
    auto destroy=[&] { reinterpret_cast<void (*)(void*,unsigned)>(held_virtual(storage.data(),0))(storage.data(),0); };
    try {
        auto result=reinterpret_cast<endstone::ItemStack (*)(const void*)>(checked(true,823904))(storage.data());
        destroy();
        return result;
    } catch(...) { destroy(); throw; }
}
bool held_equal(const std::optional<endstone::ItemStack>& left,const std::optional<endstone::ItemStack>& right) {
    if(!left || !right) return !left && !right;
    // Public isSimilar excludes auxiliary/adventure/charged-item fields.
    auto a=held_pack(*left), b=held_pack(*right);
    return a==b;
}
uint64_t held_apply(endstone::Player& player,uint64_t token,uint64_t epoch,
                   const std::vector<std::optional<endstone::ItemStack>>& before,
                   const std::vector<std::optional<endstone::ItemStack>>& after) {
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    auto watch=held_watch_for(player,token);
    if(watch->epoch!=epoch || before.size()!=36 || after.size()!=36 || held_writing)
        throw std::runtime_error("Stale or reentrant held transaction");
    auto& inventory=player.getInventory();
    std::vector<int> changed;
    for(int i=0;i<36;++i) {
        if(!held_equal(inventory.getItem(i),before[i])) throw std::runtime_error("Real inventory changed before held commit");
        if(!held_equal(before[i],after[i])) changed.push_back(i);
    }
    unsigned char context=0;
    void* output=nullptr;
    held_original_save(watch->container,&output,&context);
    auto saved=held_tag(output);
    if(!saved) throw std::runtime_error("Cannot reserve a coherent inventory save");
    held_writing=watch->container; held_before_save=saved.get();
    try {
        for(auto i:changed) {
            inventory.setItem(i,after[i]);
            // Exercise the actual hooked virtual save at every write boundary.
            // It must serialize the coherent before state, even reentrantly.
            void* boundary=nullptr;
            reinterpret_cast<HeldSave>(held_virtual(watch->container,50))(watch->container,&boundary,&context);
            auto snapshot=held_tag(boundary);
            if(!snapshot || !reinterpret_cast<bool (*)(const void*,const void*)>(held_virtual(saved.get(),6))(saved.get(),snapshot.get()))
                throw std::runtime_error("Held save guard failed at a write boundary");
        }
        for(int i=0;i<36;++i) if(!held_equal(inventory.getItem(i),after[i]))
            throw std::runtime_error("Held transaction failed native readback");
    } catch(...) {
        // While this synchronous owner-thread transaction owns the inventory,
        // restore its complete before state; reentrant saves still see before.
        try {
            for(int i=0;i<36;++i) if(!held_equal(inventory.getItem(i),before[i])) inventory.setItem(i,before[i]);
            for(int i=0;i<36;++i) if(!held_equal(inventory.getItem(i),before[i]))
                throw std::runtime_error("Held rollback did not restore its complete inventory");
        } catch(...) {
            // Keep the coherent before save alive; never persist a partial batch.
            // A poisoned inventory refuses all subsequent held transactions.
            saved.release();
            throw std::runtime_error("Held inventory quarantined after failed rollback; restart required");
        }
        held_writing=nullptr; held_before_save=nullptr;
        throw;
    }
    held_writing=nullptr; held_before_save=nullptr;
    return watch->epoch;
}
py::dict held_health() {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    std::lock_guard<std::recursive_mutex> lock(held_mutex);
    held_check_hooks();
    py::dict result;
    result["active_watches"]=held_watches.size();
    result["guarded_saves"]=held_guarded_saves;
    result["quarantined"]=held_writing!=nullptr;
    return result;
}
