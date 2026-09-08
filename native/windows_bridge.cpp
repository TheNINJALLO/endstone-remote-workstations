// RemoteWorkstations Windows bridge. BDS owns every item and container manager.
// Linked containers use a pinned per-instance validity-table adapter, not a
// global gameplay hook. It changes distance only; original checks still execute.
// The exact-build manifest documents every private entry point used here.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <pybind11/pybind11.h>
#include <endstone/player.h>
#include <endstone/server.h>
#include <endstone/block/block.h>
#include <endstone/level/dimension.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <typeinfo>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <cmath>
#include <MinHook.h>
#include "native_build.h"

class Packet;
static_assert(sizeof(std::shared_ptr<Packet>)==16);

extern "C" {
void rw_command_gate();
void rw_command_entity_gate();
void rw_structure_gate();
void* rw_structure_accept{};
void* rw_structure_reject{};
void* rw_command_accept{};
void* rw_command_reject{};
void* rw_command_entity_accept{};
void* rw_command_entity_reject{};
void rw_sign_gate_0();
void rw_sign_gate_1();
void* rw_sign_accept_0{};
void* rw_sign_reject_0{};
void* rw_sign_accept_1{};
void* rw_sign_reject_1{};
}

namespace py=pybind11;
namespace {
HMODULE bedrock{}, runtime{};
std::thread::id owner;
struct Position { int x,y,z; };
static_assert(sizeof(Position)==12 && sizeof(void*)==8);

class Hash {
    BCRYPT_HASH_HANDLE hash{};
public:
    Hash() {
        // Windows 10+ CNG pseudo-handle avoids opening a provider for every call.
        // Each invocation still hashes and compares the entire native function.
        if (BCryptCreateHash(BCRYPT_SHA256_ALG_HANDLE,&hash,nullptr,0,nullptr,0,0)<0) {
            throw std::runtime_error("SHA256 initialization failed");
        }
    }
    Hash(const Hash&)=delete;
    ~Hash() { if(hash) BCryptDestroyHash(hash); }
    void add(const void* data, ULONG size) {
        if(BCryptHashData(hash,(PUCHAR)data,size,0)<0) throw std::runtime_error("SHA256 update failed");
    }
    std::string finish() {
        std::array<UCHAR,32> bytes{};
        if(BCryptFinishHash(hash,bytes.data(),32,0)<0) throw std::runtime_error("SHA256 finalization failed");
        constexpr char digits[]="0123456789abcdef";
        std::string text(64,'0');
        for(size_t i=0;i<bytes.size();++i) {
            text[i*2]=digits[bytes[i]>>4]; text[i*2+1]=digits[bytes[i]&15];
        }
        return text;
    }
};
void readable(const void* address,size_t size) {
    MEMORY_BASIC_INFORMATION region{};
    auto begin=reinterpret_cast<uintptr_t>(address);
    if(!address || !VirtualQuery(address,&region,sizeof(region)) || region.State!=MEM_COMMIT
       || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || size>UINTPTR_MAX-begin
       || begin+size>reinterpret_cast<uintptr_t>(region.BaseAddress)+region.RegionSize)
        throw std::runtime_error("Invalid native memory region");
}
template<class T> T field(const void* object,size_t offset) {
    auto address=static_cast<const char*>(object)+offset;
    readable(address,sizeof(T)); T result; std::memcpy(&result,address,sizeof(T)); return result;
}
void verify_module(HMODULE module,const char* expected) {
    wchar_t path[32768]{};
    auto length=module ? GetModuleFileNameW(module,path,32768) : 0;
    if(!length || length>=32768) throw std::runtime_error("Required native module absent");
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("Native module cannot be read");
    Hash hash; std::array<char,65536> buffer;
    while(input.read(buffer.data(),buffer.size()) || input.gcount()) hash.add(buffer.data(),static_cast<ULONG>(input.gcount()));
    if(!input.eof() || hash.finish()!=expected) throw std::runtime_error("Unsupported native module hash");
}
void* checked(bool is_runtime,uintptr_t rva) {
    auto module=is_runtime ? runtime : bedrock;
    const NativeSymbol* symbol=nullptr;
    for(const auto& entry:NATIVE_SYMBOLS) if(entry.runtime==is_runtime && entry.rva==rva) symbol=&entry;
    if(!symbol) throw std::runtime_error("Entry point is not in the compiled manifest");
    auto address=reinterpret_cast<char*>(module)+rva;
    readable(address,symbol->size);
    MEMORY_BASIC_INFORMATION region{}; VirtualQuery(address,&region,sizeof(region));
    if(region.AllocationBase!=module || !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))
        throw std::runtime_error("Entry point is outside verified executable memory");
    Hash hash; hash.add(address,symbol->size);
    if(hash.finish()!=symbol->sha256) throw std::runtime_error("Native entry point changed or hooked");
    return address;
}
void* player_handle(endstone::Player& player) {
    if(std::this_thread::get_id()!=owner) throw std::runtime_error("Game owner thread required");
    if(std::string(typeid(player).name())!="class endstone::core::EndstonePlayer")
        throw std::runtime_error("Unknown public player implementation");
    auto native=reinterpret_cast<void* (*)(const endstone::Player*)>(checked(true,748752))(&player);
    readable(native,3481);
    auto mode=field<void*>(native,2720);
    if(field<void*>(mode,8)!=native) throw std::runtime_error("Native player ownership mismatch");
    auto manager=field<void*>(native,2744);
    if(field<void*>(manager,16)!=native) throw std::runtime_error("Stack manager ownership mismatch");
    return native;
}
bool ready(void* native) { return reinterpret_cast<bool (*)(void*)>(checked(false,0x225ad0))(native); }
bool chemistry_enabled(void* native) {
    auto level=field<void*>(native,0x1d8);
    auto getter=checked(false,0x73ca60);
    if(field<void*>(field<void*>(level,0),0x560)!=getter)
        throw std::runtime_error("Unknown native level-data dispatch");
    auto reference=field<void*>(level,0x80);
    if(!reference || !field<bool>(reference,0)) throw std::runtime_error("Native level-data binding expired");
    auto data=reinterpret_cast<void* (*)(void*)>(getter)(level);
    if(!data || data!=field<void*>(level,0x90)) throw std::runtime_error("Native level-data identity changed");
    auto enabled=field<unsigned char>(data,0x4a8);
    if(enabled>1) throw std::runtime_error("Malformed native chemistry feature flag");
    return enabled==1;
}
py::dict education_state(endstone::Player& player) {
    py::dict result;
    result["education_features_enabled"]=chemistry_enabled(player_handle(player));
    return result;
}
struct LinkedTable { std::array<void*,23> storage{}; };
std::array<LinkedTable,std::size(NATIVE_TABLES)> linked_tables;

bool linked_usable(void* model,float) noexcept {
    try {
        if(std::this_thread::get_id()!=owner) return false;
        auto table=field<void*>(model,0);
        for(size_t i=0;i<linked_tables.size();++i) {
            if(table==linked_tables[i].storage.data()+1) {
                auto function=checked(false,NATIVE_TABLES[i].pointers[20]);
                return reinterpret_cast<bool (*)(void*,float)>(function)(model,1.0e9f);
            }
        }
    } catch(...) { }
    return false;
}

void adapt_linked(void* model,void* native,uintptr_t expected) {
    if(!model || field<void*>(model,0x30)!=native)
        throw std::runtime_error("Linked native manager owner mismatch");
    auto original=reinterpret_cast<char*>(bedrock)+expected;
    if(field<void*>(model,0)!=original)
        throw std::runtime_error("Unknown linked manager implementation");
    size_t index=0;
    while(index<std::size(NATIVE_TABLES) && NATIVE_TABLES[index].rva!=expected) ++index;
    if(index==std::size(NATIVE_TABLES)) throw std::runtime_error("Linked manager table is not admitted");
    const auto& description=NATIVE_TABLES[index];
    for(size_t slot=0;slot<description.pointers.size();++slot) {
        void* expected_pointer=description.pointers[slot] ? reinterpret_cast<char*>(bedrock)+description.pointers[slot] : nullptr;
        if(field<void*>(original-8,slot*8)!=expected_pointer)
            throw std::runtime_error("Linked manager table changed or hooked");
    }
    auto& storage=linked_tables[index].storage;
    if(!storage[1]) {
        HMODULE pinned{};
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
                              reinterpret_cast<LPCWSTR>(&linked_usable),&pinned))
            throw std::runtime_error("Cannot pin linked manager adapter lifetime");
        std::memcpy(storage.data(),original-8,storage.size()*sizeof(void*));
        storage[20]=reinterpret_cast<void*>(&linked_usable);
    }
    auto replacement=storage.data()+1;
    std::memcpy(model,&replacement,sizeof(replacement));
}

void append_signed(std::string& packet,int value);
void refresh_inventory(endstone::Player& player);

struct EntityBinding { void* player; void* actor; std::int64_t identity; uintptr_t table; uintptr_t opener; };
EntityBinding command_entity_binding(endstone::Player& player,endstone::Actor& source);
EntityBinding entity_binding(endstone::Player& player,endstone::Actor& source,const std::string& kind) {
    if(kind=="commandblockminecart") return command_entity_binding(player,source);
    auto native=player_handle(player);
    if(!source.isValid() || source.getDimension().getName()!=player.getDimension().getName())
        throw std::runtime_error("Entity must be live in the player dimension");
    const char* expected=nullptr;
    uintptr_t table=0xa825c50;
    uintptr_t opener=0xe1e2c0;
    if(kind=="chestminecart") expected="minecraft:chest_minecart";
    if(kind=="chestboat") expected="minecraft:chest_boat";
    if(kind=="hopperminecart") { expected="minecraft:hopper_minecart"; table=0xa802d30; }
    bool equipment=false,owned=false;
    if(kind=="agent") {
        expected="minecraft:agent"; owned=true;
        if(source.isDead()) throw std::runtime_error("The Agent is no longer alive");
        if(!chemistry_enabled(native)) throw std::runtime_error("World Education features are disabled");
    }
    if(kind=="villager") { expected="minecraft:villager_v2"; table=0xa733dc0; }
    if(kind=="wanderingtrader") { expected="minecraft:wandering_trader"; table=0xa733dc0; }
    if(kind=="horse") expected="minecraft:horse";
    if(kind=="donkey") expected="minecraft:donkey";
    if(kind=="mule") expected="minecraft:mule";
    if(kind=="zombiehorse") expected="minecraft:zombie_horse";
    if(kind=="horse" || kind=="donkey" || kind=="mule" || kind=="zombiehorse") {
        equipment=true; owned=true; opener=0x6326520;
    }
    if(kind=="llama") { expected="minecraft:llama"; equipment=true; owned=true; }
    if(kind=="traderllama") { expected="minecraft:trader_llama"; equipment=true; owned=true; }
    if(kind=="nautilus") { expected="minecraft:nautilus"; equipment=true; owned=true; }
    if(kind=="zombienautilus") { expected="minecraft:zombie_nautilus"; equipment=true; owned=true; }
    // Camels are naturally tamed shared mounts. Server source permission and
    // the real ContainerComponent canOpen policy still apply; never assign an owner.
    if(kind=="camel") { expected="minecraft:camel"; equipment=true; }
    if(kind=="camelhusk") { expected="minecraft:camel_husk"; equipment=true; }
    if(equipment) table=0xa802de0;
    if(!expected || source.getType()!=expected) throw std::runtime_error("Entity type is not admitted for this interface");
    const auto identity=source.getId();
    if(identity==-1 || identity==0 || source.getRuntimeId()==0) throw std::runtime_error("Invalid source entity identity");
    auto level=field<void*>(native,0x1d8);
    auto lookup=checked(false,0x739ab0);
    if(field<void*>(field<void*>(level,0),0x1f0)!=lookup)
        throw std::runtime_error("Native entity lookup dispatch changed");
    // Same ActorUniqueID resolver ABI as native ServerPlayer::openTrading.
    auto actor=reinterpret_cast<void* (*)(void*,std::int64_t,bool)>(lookup)(level,identity,false);
    if(!actor) throw std::runtime_error("Native source entity is not loaded");
    auto actual=reinterpret_cast<const std::int64_t* (*)(void*)>(checked(false,0xde8bf0))(actor);
    if(!actual || field<std::int64_t>(actual,0)!=identity)
        throw std::runtime_error("Native source entity identity mismatch");
    if(kind=="agent") {
        auto runtime_lookup=checked(false,0x739f60);
        if(field<void*>(field<void*>(level,0),0x200)!=runtime_lookup
           || reinterpret_cast<void* (*)(void*,std::uint64_t,bool)>(runtime_lookup)(level,source.getRuntimeId(),false)!=actor)
            throw std::runtime_error("Native Agent runtime identity mismatch");
    }
    if(field<void*>(field<void*>(actor,0),109*sizeof(void*))!=checked(false,opener))
        throw std::runtime_error("Unknown native entity container component dispatch");
    if(owned) {
        std::int64_t owner_id=-1;
        reinterpret_cast<void* (*)(void*,std::int64_t*)>(checked(false,0xdec0c0))(actor,&owner_id);
        auto player_id=reinterpret_cast<const std::int64_t* (*)(void*)>(checked(false,0xde8bf0))(native);
        if(owner_id==-1 || owner_id==0 || owner_id!=field<std::int64_t>(player_id,0))
            throw std::runtime_error("Only the actual owner may open this entity inventory remotely");
    }
    return {native,actor,identity,table,opener};
}
py::dict entity_state(endstone::Player& player,endstone::Actor& source,const std::string& kind) {
    const auto bound=entity_binding(player,source,kind);
    py::dict result; result["actor_id"]=bound.identity; return result;
}
struct RidingState {
    void* actor; void* vehicle; std::int64_t actor_id; std::int64_t vehicle_id;
    std::uint64_t actor_runtime; std::uint64_t vehicle_runtime;
};
RidingState read_riding(endstone::Player& player,endstone::Actor& source) {
    auto native=player_handle(player);
    // Read-only cleanup must remain available after an owner or editor-mode
    // revocation. This lookup grants no inventory or gameplay authority.
    if(!source.isValid() || source.getDimension().getName()!=player.getDimension().getName())
        throw std::runtime_error("Riding source is no longer live in this dimension");
    auto identity=source.getId();
    if(identity==0 || identity==-1 || source.getRuntimeId()==0)
        throw std::runtime_error("Invalid riding source identity");
    auto level=field<void*>(native,0x1d8);
    auto lookup=checked(false,0x739ab0);
    if(field<void*>(field<void*>(level,0),0x1f0)!=lookup)
        throw std::runtime_error("Riding lookup dispatch changed");
    auto actor=reinterpret_cast<void* (*)(void*,std::int64_t,bool)>(lookup)(level,identity,false);
    if(!actor) throw std::runtime_error("Native riding source is unavailable");
    auto unique=reinterpret_cast<const std::int64_t* (*)(void*)>(checked(false,0xde8bf0));
    auto runtime_id=reinterpret_cast<void* (*)(void*,std::uint64_t*)>(checked(false,0xde88f0));
    RidingState state{actor,nullptr,identity,-1,0,0};
    if(field<std::int64_t>(unique(actor),0)!=identity)
        throw std::runtime_error("Riding source unique identity changed");
    runtime_id(actor,&state.actor_runtime);
    if(state.actor_runtime!=source.getRuntimeId()) throw std::runtime_error("Riding source runtime identity changed");
    state.vehicle=reinterpret_cast<void* (*)(void*)>(checked(true,0x112a60))(actor);
    if(state.vehicle) {
        if(state.vehicle==actor) throw std::runtime_error("Self-referencing riding source");
        state.vehicle_id=field<std::int64_t>(unique(state.vehicle),0);
        runtime_id(state.vehicle,&state.vehicle_runtime);
        if(state.vehicle_id==0 || state.vehicle_id==-1 || !state.vehicle_runtime)
            throw std::runtime_error("Invalid source vehicle identity");
    }
    return state;
}
py::object entity_mount_state(endstone::Player& player,endstone::Actor& source) {
    const auto state=read_riding(player,source);
    if(!state.vehicle) return py::none();
    py::dict result;
    result["vehicle_id"]=state.vehicle_id; result["vehicle_runtime_id"]=state.vehicle_runtime;
    return std::move(result);
}
py::object entity_mount_link(endstone::Player& player,endstone::Actor& source) {
    const auto state=read_riding(player,source);
    if(!state.vehicle) return py::none();
    auto factory=checked(false,0xdef160),destroy=checked(false,0xaf5750);
    if(field<void*>(field<void*>(state.vehicle,0),23*sizeof(void*))!=factory)
        throw std::runtime_error("Unknown vehicle link serialization");
    // Exact AddActor payload, move constructor and native link producer.
    checked(false,0xab8fa0); checked(false,0xab2970); checked(false,0xdf2300);
    void* packet=nullptr;
    reinterpret_cast<void* (*)(void*,void**)>(factory)(state.vehicle,&packet);
    if(!packet || field<void*>(packet,0)!=reinterpret_cast<char*>(bedrock)+0xa6154b0
       || field<void*>(field<void*>(packet,0),0)!=destroy)
        throw std::runtime_error("Unknown native vehicle packet");
    auto deleter=[destroy](void* value) { reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(value,1); };
    std::unique_ptr<void,decltype(deleter)> owned(packet,deleter);
    auto first=field<uintptr_t>(packet,0xe8),last=field<uintptr_t>(packet,0xf0);
    if(last<first || (last-first)%32 || last-first>64*32)
        throw std::runtime_error("Native vehicle links exceed bounds");
    py::dict result;
    unsigned int matches=0;
    for(auto at=first;at<last;at+=32) {
        auto entry=reinterpret_cast<const void*>(at);
        if(field<std::int64_t>(entry,16)!=state.actor_id) continue;
        auto type=field<unsigned char>(entry,0),immediate=field<unsigned char>(entry,24),initiated=field<unsigned char>(entry,25);
        auto angular=field<float>(entry,28);
        if(++matches!=1 || field<std::int64_t>(entry,8)!=state.vehicle_id || (type!=1 && type!=2)
           || immediate>1 || initiated>1 || !std::isfinite(angular))
            throw std::runtime_error("Native passenger link identity or shape changed");
        result["type"]=type; result["vehicle_id"]=state.vehicle_id; result["passenger_id"]=state.actor_id;
        result["vehicle_runtime_id"]=state.vehicle_runtime; result["passenger_runtime_id"]=state.actor_runtime;
        result["immediate"]=bool(immediate); result["passenger_initiated"]=bool(initiated);
        result["angular_velocity"]=angular;
    }
    if(matches!=1) throw std::runtime_error("Native vehicle omitted its passenger link");
    return std::move(result);
}
int open_entity(endstone::Player& player,endstone::Actor& source,const std::string& kind) {
    if(kind=="commandblockminecart") throw std::runtime_error("Use the guarded command entity editor contract");
    const auto bound=entity_binding(player,source,kind);
    if(!ready(bound.player)) throw std::runtime_error("Close the current inventory before opening an entity");
    // Real ContainerComponent::canOpen preserves the native interaction,
    // lifecycle and configured owner-only checks before it creates a manager.
    if(bound.table==0xa733dc0) {
        auto mode=field<void*>(bound.player,2720);
        auto interact=field<void*>(field<void*>(mode,0),14*sizeof(void*));
        // Creative uses the base implementation; Survival may use the leaf
        // wrapper. Invoke the actual verified dispatch so its checks survive.
        if(field<void*>(mode,8)!=bound.player || (interact!=checked(false,0x1f36410)
            && interact!=checked(false,0x1f3d5d0)))
            throw std::runtime_error("Native merchant interaction dispatch changed");
        const std::array<float,3> hit{0.0f,1.0f,0.0f};
        // Full normal interaction sets the real customer and retains offer,
        // profession, stock, reputation and interaction event checks.
        if(!reinterpret_cast<bool (*)(void*,void*,const float*)>(interact)(mode,bound.actor,hit.data()))
            throw std::runtime_error("The merchant declined this interaction");
    } else {
        reinterpret_cast<void (*)(void*,void*)>(checked(false,bound.opener))(bound.actor,bound.player);
    }
    adapt_linked(field<void*>(bound.player,1440),bound.player,bound.table);
    return field<unsigned char>(bound.player,3480);
}

void send_entity_actor(endstone::Player& player,endstone::Actor& source,const std::string& kind) {
    const auto bound=entity_binding(player,source,kind);
    if(bound.table!=0xa802de0 && bound.table!=0xa733dc0 && kind!="commandblockminecart")
        throw std::runtime_error("Entity display is not admitted for this kind");
    auto factory=checked(false,0xdef160);
    if(field<void*>(field<void*>(bound.actor,0),23*sizeof(void*))!=factory)
        throw std::runtime_error("Unknown native AddActor factory dispatch");
    void* packet=nullptr;
    reinterpret_cast<void* (*)(void*,void**)>(factory)(bound.actor,&packet);
    auto destroy=checked(false,0xaf5750);
    if(!packet || field<void*>(packet,0)!=reinterpret_cast<char*>(bedrock)+0xa6154b0
       || field<void*>(field<void*>(packet,0),0)!=destroy)
        throw std::runtime_error("Unknown native AddActor packet");
    try {
        auto sender=checked(false,0x693260);
        if(field<void*>(field<void*>(bound.player,0),228*sizeof(void*))!=sender)
            throw std::runtime_error("Native packet sender changed");
        reinterpret_cast<void (*)(void*,const void*)>(sender)(bound.player,packet);
    } catch(...) {
        reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1);
        throw;
    }
    reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1);
}

void send_block_actor(endstone::Player& player,int x,int y,int z) {
    auto native=player_handle(player);
    auto block=player.getDimension().getBlockAt(x,y,z);
    if(!block) throw std::runtime_error("Linked block is unavailable");
    const auto type=block->getType();
    uintptr_t actor_table=0,update_rva=0;
    unsigned char actor_type=0;
    if(type=="minecraft:beacon") { actor_table=0xa6c4360; update_rva=0x29a4430; actor_type=21; }
    if(type=="minecraft:crafter") { actor_table=0xa6bc270; update_rva=0x29235c0; actor_type=55; }
    if(type=="minecraft:lectern") { actor_table=0xa77f210; update_rva=0x290b8c0; actor_type=37; }
    if(type=="minecraft:jigsaw") { actor_table=0xa77ec50; update_rva=0x290b8c0; actor_type=36; }
    if(type=="minecraft:structure_block") { actor_table=0xa72f380; update_rva=0x290b8c0; actor_type=32; }
    if(type=="minecraft:command_block" || type=="minecraft:repeating_command_block" || type=="minecraft:chain_command_block") {
        actor_table=0xa6bbe00; update_rva=0x291f3d0; actor_type=26;
    }
    if(type.ends_with("_sign") && !type.ends_with("_hanging_sign")) {
        actor_table=0xa72eed0; update_rva=0x4627ab0; actor_type=4;
    }
    if(!actor_table) throw std::runtime_error("Block actor projection is not admitted for this type");
    Position position{x,y,z};
    auto region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40))(native);
    auto actor=reinterpret_cast<void* (*)(void*,const Position*)>(checked(false,0x14fb650))(region,&position);
    if(!actor || field<unsigned char>(actor,20)!=actor_type
       || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+actor_table)
        throw std::runtime_error("Unknown native block actor implementation");
    auto actual=field<Position>(actor,8);
    if(actual.x!=x || actual.y!=y || actual.z!=z) throw std::runtime_error("Block actor source changed");
    auto update=checked(false,update_rva);
    if(field<void*>(field<void*>(actor,0),18*sizeof(void*))!=update)
        throw std::runtime_error("Block actor update dispatch changed");
    void* packet=nullptr;
    // Exact dispatcher ABI: RCX=BlockActor, RDX=unique_ptr result, R8=region.
    reinterpret_cast<void* (*)(void*,void**,void*)>(update)(actor,&packet,region);
    if(!packet) throw std::runtime_error("Block actor update is unavailable");
    auto destroy=checked(false,0x3829da0);
    if(field<void*>(packet,0)!=reinterpret_cast<char*>(bedrock)+0xa706460
       || field<void*>(field<void*>(packet,0),0)!=destroy)
        // Never apply a known-class destructor to an unexpected object.
        throw std::runtime_error("Unknown block actor update packet");
    try {
        auto sender=checked(false,0x693260);
        if(field<void*>(field<void*>(native,0),228*sizeof(void*))!=sender)
            throw std::runtime_error("Native packet sender changed");
        reinterpret_cast<void (*)(void*,const void*)>(sender)(native,packet);
    } catch(...) {
        reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1);
        throw;
    }
    reinterpret_cast<void (*)(void*,unsigned int)>(destroy)(packet,1);
}

struct Lectern { void* player; void* region; void* actor; int page; int total; };
Lectern lectern(endstone::Player& player,int x,int y,int z) {
    auto native=player_handle(player);
    auto block=player.getDimension().getBlockAt(x,y,z);
    if(!block || block->getType()!="minecraft:lectern") throw std::runtime_error("Real lectern unavailable");
    Position position{x,y,z};
    auto region=reinterpret_cast<void* (*)(void*)>(checked(false,0xddfc40))(native);
    auto actor=reinterpret_cast<void* (*)(void*,const Position*)>(checked(false,0x14fb650))(region,&position);
    if(!actor || field<unsigned char>(actor,20)!=37
       || field<void*>(actor,0)!=reinterpret_cast<char*>(bedrock)+0xa77f210)
        throw std::runtime_error("Unknown lectern block actor");
    auto actual=field<Position>(actor,8);
    if(actual.x!=x || actual.y!=y || actual.z!=z) throw std::runtime_error("Lectern source changed");
    // Preserve the original server handler's game-mode and real-book checks.
    if(!reinterpret_cast<bool (*)(void*,unsigned char)>(checked(false,0x21f0a0))(native,3)
       || reinterpret_cast<bool (*)(void*)>(checked(false,0x1bc7df0))(static_cast<char*>(actor)+0x288))
        throw std::runtime_error("The player cannot read this lectern");
    int page=field<int>(actor,0x280),total=field<int>(actor,0x284);
    if(total<1 || total>255 || page<0 || page>=total) throw std::runtime_error("Invalid native lectern book state");
    return {native,region,actor,page,total};
}

py::dict lectern_state(endstone::Player& player,int x,int y,int z) {
    const auto bound=lectern(player,x,y,z);
    py::dict result;
    result["page"]=bound.page; result["total_pages"]=bound.total;
    return result;
}

void lectern_page(endstone::Player& player,int x,int y,int z,int page,int total) {
    const auto bound=lectern(player,x,y,z);
    if(total!=bound.total || page<0 || page>=total) throw std::runtime_error("Lectern page changed or out of bounds");
    // Native setter also updates the actual comparator and dirty-save state.
    reinterpret_cast<void (*)(void*,int,void*)>(checked(false,0x5eeecd0))(bound.actor,page,bound.region);
}

#include "sign_bridge.h"
#include "command_bridge.h"
#include "structure_bridge.h"

int open_linked_station(endstone::Player& player,const std::string& kind,int x,int y,int z) {
    auto native=player_handle(player);
    if(!ready(native)) throw std::runtime_error("Close the current inventory before opening a linked station");
    auto block=player.getDimension().getBlockAt(x,y,z);
    if(!block) throw std::runtime_error("Linked block is unavailable");
    auto block_type=block->getType();
    Position position{x,y,z};
    uintptr_t factory=0,table=0;
    const char* expected_type=nullptr;
    if(kind=="lectern") {
        const auto bound=lectern(player,x,y,z);
        auto opener=checked(false,0x693c50);
        if(field<void*>(field<void*>(native,0),192*sizeof(void*))!=opener)
            throw std::runtime_error("Native book opener changed");
        reinterpret_cast<void (*)(void*,int,bool,int,void*)>(opener)(native,0,true,0,bound.actor);
        return field<unsigned char>(native,3480);
    }
    if(kind=="hopper") {
        if(block_type!="minecraft:hopper") throw std::runtime_error("Linked hopper block type changed");
        // Dispatcher 0x17e8906 invokes this factory, then sends the open and
        // native current-container contents. Player::sendInventory includes
        // that same current-container refresh (0x692c8e..0x692d4a).
        auto window=reinterpret_cast<int (*)(void*,const Position*)>(checked(false,0x1820dd0))(native,&position);
        adapt_linked(field<void*>(native,1440),native,0xa802d30);
        std::string packet;
        packet.push_back(static_cast<char>(window)); packet.push_back(8);
        append_signed(packet,x); append_signed(packet,y); append_signed(packet,z); packet.push_back(1);
        player.sendPacket(46,packet);
        refresh_inventory(player);
        return window;
    }
    if(kind=="chest" || kind=="doublechest" || kind=="trappedchest" || kind=="barrel") {
        expected_type=kind=="barrel" ? "minecraft:barrel" : kind=="trappedchest" ? "minecraft:trapped_chest" : "minecraft:chest";
        if(block_type!=expected_type) throw std::runtime_error("Linked storage block type changed");
        unsigned char actor_type=kind=="barrel" ? 42 : 2;
        reinterpret_cast<void (*)(void*,const Position*,int,std::int64_t,const unsigned char*)>(checked(false,0x17eb540))(
            native,&position,0,-1,&actor_type);
        table=0xa825c50;
    } else {
        if(kind=="furnace") { factory=0x17ed2a0; table=0xa802bd0; expected_type="minecraft:furnace"; }
        if(kind=="blastfurnace") { factory=0x17e9c90; table=0xa802bd0; expected_type="minecraft:blast_furnace"; }
        if(kind=="smoker") { factory=0x17eef30; table=0xa802bd0; expected_type="minecraft:smoker"; }
        if(kind=="brewing") { factory=0x17ea140; table=0xa825af0; expected_type="minecraft:brewing_stand"; }
        if(kind=="enchanting") { factory=0x17ece10; table=0xa6de070; expected_type="minecraft:enchanting_table"; }
        if(kind=="dispenser") { factory=0x17eba00; table=0xa825e60; expected_type="minecraft:dispenser"; }
        if(kind=="dropper") { factory=0x17ebe90; table=0xa802a70; expected_type="minecraft:dropper"; }
        if(kind=="beacon") { factory=0x17e97a0; table=0xa825a40; expected_type="minecraft:beacon"; }
        if(kind=="crafter") { factory=0x17ec320; table=0xa825db0; expected_type="minecraft:crafter"; }
        if(kind=="compoundcreator") { factory=0x17eab40; table=0xa825d00; expected_type="minecraft:compound_creator"; }
        if(kind=="elementconstructor") { factory=0x17ec8a0; table=0xa802b20; expected_type="minecraft:element_constructor"; }
        if(kind=="materialreducer") { factory=0x17ee9c0; table=0xa802ff0; expected_type="minecraft:material_reducer"; }
        if(kind=="labtable") { factory=0x17edfc0; table=0xa802e90; expected_type="minecraft:lab_table"; }
        if((kind=="compoundcreator" || kind=="elementconstructor" || kind=="materialreducer" || kind=="labtable")
           && !chemistry_enabled(native)) throw std::runtime_error("World chemistry features are disabled");
        if(!factory) throw std::runtime_error("Linked station is not admitted");
        auto lit=std::string("minecraft:lit_")+std::string(expected_type).substr(10);
        bool processing=kind=="furnace" || kind=="blastfurnace" || kind=="smoker";
        if(block_type!=expected_type && !(processing && block_type==lit))
            throw std::runtime_error("Linked workstation block type changed");
        reinterpret_cast<void (*)(void*,const Position*,std::int64_t)>(checked(false,factory))(native,&position,-1);
    }
    adapt_linked(field<void*>(native,1440),native,table);
    return field<unsigned char>(native,3480);
}
void append_signed(std::string& packet,int value) {
    auto encoded=(static_cast<std::uint32_t>(value)<<1)^static_cast<std::uint32_t>(value>>31);
    while(encoded>=128) { packet.push_back(static_cast<char>((encoded&127)|128)); encoded>>=7; }
    packet.push_back(static_cast<char>(encoded));
}
py::dict state(endstone::Player& player) {
    auto native=player_handle(player); py::dict result;
    result["ready"]=ready(native);
    result["manager_active"]=field<void*>(native,1440)!=nullptr;
    result["window"]=field<unsigned char>(native,3480);
    return result;
}
void refresh_inventory(endstone::Player& player) {
    auto native=player_handle(player);
    auto table=field<void*>(native,0);
    // Compiled pinned Mob::sendInventory(bool): virtual offset 1312, slot 164.
    // Both exact-build Player tables dispatch to the same verified function.
    auto function=checked(false,0x692b40);
    if(field<void*>(table,1312)!=function)
        throw std::runtime_error("Unknown native inventory refresh dispatch");
    reinterpret_cast<void (*)(void*,bool)>(function)(native,false);
}
int open_station(endstone::Player& player,const std::string& kind,int x,int y,int z) {
    auto native=player_handle(player);
    if(!ready(native)) throw std::runtime_error("Close the current inventory before opening a workstation");
    Position position{x,y,z};
    if(kind=="inventory2x2" || kind=="armor" || kind=="offhand" || kind=="recipebook") {
        auto function=checked(false,0x693aa0);
        if(field<void*>(field<void*>(native,0),196*sizeof(void*))!=function)
            throw std::runtime_error("Unknown player inventory opener dispatch");
        reinterpret_cast<void (*)(void*)>(function)(native);
        return field<unsigned char>(native,3480);
    }
    if(kind=="craft") {
        // Reproduce the verified workbench dispatch branch's native context.
        auto manager=field<void*>(native,2744);
        auto table=field<void*>(manager,0);
        auto callback=field<void*>(table,6*sizeof(void*));
        if(callback!=checked(false,0x274b550)) throw std::runtime_error("Unknown stack context callback");
        auto create=checked(false,0x693f80);
        alignas(8) std::array<unsigned char,24> source{};
        std::memcpy(source.data(),&position,12); // variant index 0, BlockPos
        auto window=reinterpret_cast<int (*)(void*,int,const void*)>(create)(native,1,source.data());
        alignas(8) std::array<unsigned char,40> context{};
        std::memcpy(context.data(),&native,8);
        context[8]=1; // ContainerType::WORKBENCH
        std::memcpy(context.data()+16,&position,12); context[32]=2;
        reinterpret_cast<void (*)(void*,const void*)>(callback)(manager,context.data());
        std::string packet;
        packet.push_back(static_cast<char>(window)); packet.push_back(1);
        append_signed(packet,x); append_signed(packet,y); append_signed(packet,z); packet.push_back(1);
        player.sendPacket(46,packet);
        return window;
    }
    uintptr_t factory=0;
    if(kind=="anvil") factory=0x17e9230;
    if(kind=="stonecutter") factory=0x17ef3e0;
    if(kind=="grindstone") factory=0x17ed740;
    if(kind=="smithing") factory=0x17e8cc0;
    if(kind=="loom") factory=0x17ee450;
    if(kind=="cartography") factory=0x17ea5d0;
    if(!factory) throw std::runtime_error("Workstation is not admitted by this native build");
    auto function=checked(false,factory);
    // BDS allocates, retains, validates and destroys its native container model.
    reinterpret_cast<void (*)(void*,const Position*,std::int64_t)>(function)(native,&position,-1);
    if(!field<void*>(native,1440)) throw std::runtime_error("BDS did not create a container manager");
    return field<unsigned char>(native,3480);
}
}
extern "C" unsigned int rw_sign_distance_allowed(void* player,void* packet,unsigned int stage,
                                                const std::shared_ptr<Packet>* shared) noexcept {
    return allow_sign_distance(player,packet,stage,shared);
}
extern "C" unsigned int rw_command_distance_allowed(void* player,const void* packet) noexcept {
    return allow_command_distance(player,packet);
}
extern "C" unsigned int rw_structure_distance_allowed(void* player,const void* packet) noexcept {
    return allow_structure_distance(player,packet);
}
PYBIND11_MODULE(_rw_native,module) {
    owner=std::this_thread::get_id(); runtime=GetModuleHandleW(L"endstone_runtime.dll"); bedrock=GetModuleHandleW(nullptr);
    verify_module(runtime,RUNTIME_SHA256); verify_module(bedrock,BDS_SHA256);
    for(const auto& symbol:NATIVE_SYMBOLS) checked(symbol.runtime,symbol.rva);
    module.def("state",&state);
    module.def("education_state",&education_state);
    module.def("refresh_inventory",&refresh_inventory);
    module.def("open_station",&open_station);
    module.def("open_linked_station",&open_linked_station);
    module.def("entity_state",&entity_state);
    module.def("entity_mount_state",&entity_mount_state);
    module.def("entity_mount_link",&entity_mount_link);
    module.def("open_entity",&open_entity);
    module.def("send_entity_actor",&send_entity_actor);
    module.def("send_block_actor",&send_block_actor);
    module.def("lectern_state",&lectern_state);
    module.def("lectern_page",&lectern_page);
    module.def("sign_state",&sign_state,py::arg("player"),py::arg("x"),py::arg("y"),py::arg("z"),py::arg("generation")=0);
    module.def("open_sign",&open_sign);
    module.def("jigsaw_state",&jigsaw_state,py::arg("player"),py::arg("x"),py::arg("y"),py::arg("z"),py::arg("generation")=0);
    module.def("open_jigsaw",&open_jigsaw);
    module.def("sign_touch",&sign_touch,py::arg("player"),py::arg("generation"),py::arg("accept_save")=false);
    module.def("close_sign",&close_sign);
    module.def("shutdown_sign_hooks",&shutdown_sign_hooks);
    module.def("abandon_sign",&abandon_sign);
    module.def("command_state",&command_state,py::arg("player"),py::arg("x"),py::arg("y"),py::arg("z"),py::arg("generation")=0);
    module.def("open_command",&open_command);
    module.def("command_entity_state",&command_entity_state,py::arg("player"),py::arg("source"),py::arg("generation")=0);
    module.def("open_command_entity",&open_command_entity);
    module.def("command_touch",&command_touch,py::arg("player"),py::arg("generation"),py::arg("accept_save")=false);
    module.def("close_command",&close_command);
    module.def("abandon_command",&abandon_command);
    module.def("shutdown_command_hooks",&shutdown_command_hooks);
    module.def("structure_state",&structure_state,py::arg("player"),py::arg("x"),py::arg("y"),py::arg("z"),py::arg("generation")=0);
    module.def("open_structure",&open_structure);
    module.def("structure_touch",&structure_touch,py::arg("player"),py::arg("generation"),py::arg("accept_save")=false);
    module.def("close_structure",&close_structure);
    module.def("abandon_structure",&abandon_structure);
    module.def("shutdown_structure_hooks",&shutdown_structure_hooks);
    module.def("runtime_info",[] {
        py::dict result; result["bridge_api"]=1; result["platform"]="windows-x86_64";
        result["bds_sha256"]=BDS_SHA256; result["runtime_sha256"]=RUNTIME_SHA256;
        result["workstations"]=py::make_tuple("craft","anvil","stonecutter","grindstone","smithing","loom","cartography",
                                            "inventory2x2","armor","offhand","recipebook");
        result["linked_workstations"]=py::make_tuple("chest","doublechest","trappedchest","barrel","dispenser","dropper","hopper",
            "furnace","blastfurnace","smoker","brewing","enchanting","beacon","crafter","lectern","sign","jigsaw","commandblock","structure",
            "compoundcreator","elementconstructor","materialreducer","labtable");
        result["entity_workstations"]=py::make_tuple("chestminecart","hopperminecart","chestboat","horse",
            "donkey","mule","llama","traderllama","camel","camelhusk","zombiehorse","nautilus","zombienautilus",
            "villager","wanderingtrader","commandblockminecart","agent");
        result["transaction_owner"]="BDS"; return result;
    });
}
