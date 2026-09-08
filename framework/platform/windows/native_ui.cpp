#include "native_ui.hpp"
#include "bridge.hpp"
#include <endstone/server.h>
#include <endstone/player.h>
#include <endstone/block/block_data.h>
#include <endstone/level/dimension.h>
#include <endstone/event/server/packet_receive_event.h>
#include <endstone/event/server/packet_send_event.h>
#include <array>
#include <chrono>
#include <cmath>
#include <random>

namespace oni::vcf::platform::windows {
namespace {
using Clock = std::chrono::steady_clock;
struct Point { int32_t x=0,y=0,z=0; bool operator==(const Point&) const = default; };
struct Spec { std::string_view id,block; uint8_t type; };
constexpr Spec specs[] = {
    {"craft","minecraft:crafting_table",1}, {"anvil","minecraft:anvil",5},
    {"stonecutter","minecraft:stonecutter_block",29}, {"grindstone","minecraft:grindstone",26},
    {"smithing","minecraft:smithing_table",33}, {"loom","minecraft:loom",24},
    {"cartography","minecraft:cartography_table",30}, {"inventory2x2","",255},
    {"armor","",255}, {"offhand","",255}, {"recipebook","",255}
};
const Spec* spec(std::string_view id) {
    for(const auto& s:specs) if(s.id==id) return &s;
    return nullptr;
}
void var(std::string& out,uint64_t v) {
    do { auto b=static_cast<uint8_t>(v&127); v>>=7;out.push_back(static_cast<char>(b|(v?128:0))); } while(v);
}
void signed_var(std::string& out,int32_t n) {
    var(out,(uint32_t(n)<<1)^(n<0?UINT32_MAX:0));
}
struct Reader {
    std::string_view data; size_t pos=0;
    uint8_t byte(){require(pos<data.size());return static_cast<uint8_t>(data[pos++]);}
    uint64_t var(uint32_t bits=32) {
        uint64_t v=0;uint32_t n=0;
        for(uint32_t shift=0;shift<bits;shift+=7) {
            auto b=byte();++n;
            if(bits-shift<7) require((b&127)<(uint32_t(1)<<(bits-shift)));
            v|=uint64_t(b&127)<<shift;
            if(!(b&128)){require(n==1||(b&127)!=0);return v;}
        }
        throw Error{VCF_INVALID};
    }
    int32_t signed_var(){auto n=static_cast<uint32_t>(var());return static_cast<int32_t>((n>>1)^(uint32_t(0)-(n&1)));}
    Point point(){return {signed_var(),signed_var(),signed_var()};}
};
std::string block_update(Point p,uint32_t runtime) {
    std::string out;signed_var(out,p.x);signed_var(out,p.y);signed_var(out,p.z);
    var(out,runtime);var(out,2);var(out,0);return out;
}
std::string ping(uint32_t nonce) {
    std::string out(9,'\0');
    for(size_t i=0;i<8;i++)out[i]=static_cast<char>(uint64_t(nonce)>>(8*i));
    out[8]=1;return out;
}
bool ping_reply(std::string_view data,uint32_t nonce) {
    if(data.size()!=9||data[8]!=1)return false;
    uint64_t value=0;
    for(size_t i=0;i<8;i++)value|=uint64_t(static_cast<uint8_t>(data[i]))<<(8*i);
    return value==uint64_t(nonce)*1000000;
}
Point feet(endstone::Player& p) {
    auto l=p.getLocation();
    require(std::isfinite(l.getX())&&std::isfinite(l.getY())&&std::isfinite(l.getZ()));
    require(std::abs(l.getX())<=30000000&&std::abs(l.getZ())<=30000000&&l.getY()>=-64&&l.getY()<=319);
    return {static_cast<int32_t>(std::floor(l.getX())),static_cast<int32_t>(std::floor(l.getY())),static_cast<int32_t>(std::floor(l.getZ()))};
}
Point projection(endstone::Player& p) {
    auto base=feet(p);
    constexpr Point offsets[]={{1,1,0},{-1,1,0},{0,1,1},{0,1,-1},{0,2,1},{0,2,-1}};
    for(auto offset:offsets) {
        Point q{base.x+offset.x,base.y+offset.y,base.z+offset.z};
        if(q.y>=-63&&q.y<=318&&p.getDimension().getBlockAt(q.x,q.y,q.z)->getType()=="minecraft:air") return q;
    }
    throw Error{VCF_UNAVAILABLE};
}
}
struct NativeUi::Impl {
    struct View {
        vcf_handle owner,id;
        std::string player,kind,dimension,permission;
        Point position;
        uint32_t nonce;
        int window=-1;
        bool prepared=false,projected=false,ack=false,activating=false,observed=false,active=false;
        bool closing=false,restored=false,close_seen=false,superseded=false,projecting=false;
        Clock::time_point queued=Clock::now(),closed{},next_check{};
    };
    endstone::Server& server;
    Engine& engine;
    std::map<vcf_handle,View> views;
    vcf_handle cursor=0;
    uint32_t nonce=std::random_device{}()&0x7fffffff;
    explicit Impl(endstone::Server& s,Engine& e):server(s),engine(e){}
    endstone::Player* player(std::string_view id) {
        for(auto*p:server.getOnlinePlayers())if(p->getUniqueId().str()==id)return p;
        return nullptr;
    }
    bool permitted(endstone::Player& p,const View& v) {
        const auto* row=resolve(v.kind);
        return row&&p.isValid()&&!p.isDead()&&p.getGameVersion()=="1.26.45"&&p.getDeviceOS()=="Windows"
            &&p.getDimension().getName()==v.dimension
            &&p.hasPermission("remoteworkstations.use")&&p.hasPermission(std::string(row->permission))
            &&(v.permission.empty()||p.hasPermission(v.permission));
    }
    void restore(endstone::Player& p,View& v) {
        if(v.restored)return;
        if(v.projected&&!spec(v.kind)->block.empty()&&p.getDimension().getName()==v.dimension) {
            auto current=p.getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getData();
            v.projecting=true;
            try {p.sendPacket(21,block_update(v.position,current->getRuntimeId()));}
            catch(...) {v.projecting=false;throw;}
            v.projecting=false;
        }
        v.restored=true;
    }
    void retire(View& v,vcf_status result) {
        try {engine.retired(v.owner,v.id,result);}catch(const Error&) {}
    }
    void close(endstone::Player* p,View& v) {
        if(v.closing)return;
        v.closing=true;v.closed=Clock::now();
        if(p&&v.window>=0&&!v.superseded&&!spec(v.kind)->block.empty()) {
            std::string packet{static_cast<char>(v.window),static_cast<char>(247),1};
            p->sendPacket(47,packet);
        }
        if(p)restore(*p,v);
    }
    bool poll(View& v) {
        auto*p=player(v.player);auto now=Clock::now();
        if(!p||!p->isValid()){retire(v,VCF_CLOSED);return true;}
        if(v.superseded){restore(*p,v);retire(v,VCF_CLOSED);return true;}
        if(!permitted(*p,v))close(p,v);
        if(v.closing) {
            restore(*p,v);
            // The four shared inventory roles are client-owned. Relinquishing
            // our lease never writes the real cursor/grid or fabricates closure.
            if(spec(v.kind)->block.empty()||v.window<0||inspect(*p).ready) {
                retire(v,VCF_CLOSED);return true;
            }
            if(now-v.closed>std::chrono::seconds(5)){
                p->kick("Native inventory close was not acknowledged; rejoin to continue.");
                retire(v,VCF_QUARANTINED);return true;
            }
            return false;
        }
        if(!v.prepared) {
            require(inspect(*p).ready,VCF_CONFLICT);
            v.position=spec(v.kind)->block.empty()?feet(*p):projection(*p);
            if(!spec(v.kind)->block.empty()) {
                auto data=server.createBlockData(std::string(spec(v.kind)->block));
                v.projecting=true;v.projected=true;
                try{p->sendPacket(21,block_update(v.position,data->getRuntimeId()));}
                catch(...){v.projecting=false;throw;}
                v.projecting=false;
            } else v.restored=true;
            v.prepared=true;p->sendPacket(115,ping(v.nonce));return false;
        }
        if(!v.active) {
            require(now-v.queued<std::chrono::seconds(10),VCF_UNAVAILABLE);
            // This interval belongs only to the legacy Windows 1.26.45 profile.
            // The ordered projection ping is also mandatory. New artifact
            // qualification is still required before this experimental opt-in.
            if(!v.ack||now-v.queued<std::chrono::milliseconds(500))return false;
            require(inspect(*p).ready,VCF_CONFLICT);
            if(!spec(v.kind)->block.empty())require(p->getDimension().getBlockAt(v.position.x,v.position.y,v.position.z)->getType()=="minecraft:air",VCF_CONFLICT);
            v.activating=true;
            try {
                auto window=station(*p,v.kind,v.position.x,v.position.y,v.position.z);
                v.activating=false;
                auto observed_window=v.window;v.window=window;
                require(v.observed&&observed_window==window,VCF_CONFLICT);
            }catch(...){v.activating=false;throw;}
            v.active=true;engine.opened(v.owner,v.id,VCF_OK);
        }
        if(v.close_seen||now>=v.next_check) {
            v.next_check=now+std::chrono::milliseconds(250);
            if(inspect(*p).ready){restore(*p,v);retire(v,VCF_OK);return true;}
        }
        if(v.close_seen)close(p,v);
        if(now-v.queued>std::chrono::minutes(20))close(p,v);
        return false;
    }
};
NativeUi::NativeUi(endstone::Server&s,Engine&e):impl_(std::make_unique<Impl>(s,e)){}
NativeUi::~NativeUi()=default;
bool NativeUi::supports(std::string_view id){return spec(id)!=nullptr;}
vcf_status NativeUi::open(const Session&s) {
    auto*layout=spec(s.kind);if(!layout)return VCF_UNAVAILABLE;
    // Real player-state roles and a BDS-owned native workstation context are
    // distinct from plugin-authored virtual transactions.
    if(s.mode!=(layout->block.empty()?VCF_REAL_SOURCE:VCF_NATIVE_CONTEXT))return VCF_UNAVAILABLE;
    auto*p=impl_->player(s.player);if(!p)return VCF_CLOSED;
    require(impl_->views.size()<100,VCF_CAPACITY);
    for(const auto&[id,v]:impl_->views)require(v.player!=s.player,VCF_CONFLICT);
    Impl::View v;v.owner=s.owner;v.id=s.id;v.player=s.player;v.kind=s.kind;
    v.dimension=p->getDimension().getName();v.permission=s.permission;
    v.nonce=(++impl_->nonce&0x7fffffff)+1;
    require(impl_->permitted(*p,v),VCF_DENIED);require(inspect(*p).ready,VCF_CONFLICT);
    impl_->views.emplace(s.id,std::move(v));return VCF_PENDING;
}
vcf_status NativeUi::close(const Session&s) {
    auto it=impl_->views.find(s.id);if(it==impl_->views.end())return VCF_OK;
    impl_->close(impl_->player(it->second.player),it->second);return VCF_PENDING;
}
void NativeUi::tick() {
    auto deadline=Clock::now()+std::chrono::milliseconds(2);
    std::vector<vcf_handle> done;
    auto remaining=impl_->views.size();
    while(remaining--&&!impl_->views.empty()) {
        auto it=impl_->views.upper_bound(impl_->cursor);if(it==impl_->views.end())it=impl_->views.begin();
        auto&[id,v]=*it;impl_->cursor=id;
        try{if(impl_->poll(v))done.push_back(id);}
        catch(...){
            auto*p=impl_->player(v.player);
            try{impl_->close(p,v);}catch(...){}
            try{impl_->engine.opened(v.owner,v.id,VCF_UNAVAILABLE);}catch(const Error&){}
            if(v.window<0){impl_->retire(v,VCF_UNAVAILABLE);done.push_back(id);}
        }
        if(Clock::now()>=deadline)break;
    }
    for(auto id:done)impl_->views.erase(id);
}
void NativeUi::receive(endstone::PacketReceiveEvent&e) {
    if(!e.getPlayer()||e.isCancelled())return;
    auto player=e.getPlayer()->getUniqueId().str();
    for(auto&[id,v]:impl_->views)if(v.player==player) {
        if(e.getPacketId()==115&&v.prepared&&ping_reply(e.getPayload(),v.nonce))v.ack=true;
        if(e.getPacketId()==47&&e.getPayload().size()==3&&static_cast<uint8_t>(e.getPayload()[0])==v.window)v.close_seen=true;
        if(e.getPacketId()==147&&!v.superseded&&!impl_->permitted(*e.getPlayer(),v))e.setCancelled(true);
        return;
    }
}
void NativeUi::sent(endstone::PacketSendEvent&e) {
    if(!e.getPlayer()||e.isCancelled())return;
    auto player=e.getPlayer()->getUniqueId().str();
    for(auto&[id,v]:impl_->views)if(v.player==player) {
        try{
            if(e.getPacketId()==46) {
                Reader reader{e.getPayload()};auto window=reader.byte(),type=reader.byte();auto position=reader.point();reader.var(64);require(reader.pos==reader.data.size());
                if(v.activating&&type==spec(v.kind)->type&&(spec(v.kind)->block.empty()||position==v.position)&&!v.observed){v.window=window;v.observed=true;}
                else v.superseded=true;
            } else if(e.getPacketId()==47&&e.getPayload().size()==3&&static_cast<uint8_t>(e.getPayload()[0])==v.window)v.close_seen=true;
            else if(e.getPacketId()==21&&!v.projecting&&!spec(v.kind)->block.empty()&&v.prepared) {
                Reader reader{e.getPayload()};if(reader.point()==v.position)v.close_seen=true;
            } else if(e.getPacketId()==100)v.superseded=true;
        }catch(const Error&){v.close_seen=true;}
        return;
    }
}
void NativeUi::shutdown() {
    for(auto&[id,v]:impl_->views)try{impl_->close(impl_->player(v.player),v);}catch(...){}
    impl_->views.clear();
}
}
