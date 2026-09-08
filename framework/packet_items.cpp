#include <oni/vcf/packet_items.hpp>
#include <bit>
#include <cmath>

namespace oni::vcf::wire {
namespace {
constexpr size_t inventory_bound=65536, registry_bound=8*1024*1024;
void utf8(std::string_view s){
    size_t i=0;
    while(i<s.size()){
        auto b=static_cast<uint8_t>(s[i++]);if(b<128)continue;
        uint32_t code,minimum,n;
        if(b>=0xc2&&b<=0xdf){code=b&31;minimum=128;n=1;}
        else if(b>=0xe0&&b<=0xef){code=b&15;minimum=2048;n=2;}
        else if(b>=0xf0&&b<=0xf4){code=b&7;minimum=65536;n=3;}
        else throw Error{VCF_INVALID};
        require(n<=s.size()-i);
        while(n--){auto next=static_cast<uint8_t>(s[i++]);require((next&192)==128);code=(code<<6)|(next&63);}
        require(code>=minimum&&code<=0x10ffff&&!(code>=0xd800&&code<=0xdfff));
    }
}
void identifier(std::string_view s){
    require(!s.empty()&&s.size()<=256);
    auto colon=s.find(':');require(colon!=std::string_view::npos&&colon>0&&colon+1<s.size());
    for(size_t i=0;i<s.size();++i){
        char c=s[i];require((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||(c==':'&&i==colon)||(c=='/'&&i>colon));
    }
}
struct Reader {
    std::span<const uint8_t> data;size_t offset=0;uint32_t nodes=0;
    uint8_t byte(){require(offset<data.size());return data[offset++];}
    bool boolean(){auto b=byte();require(b<=1);return b!=0;}
    uint64_t fixed(size_t n){require(n<=8&&n<=data.size()-offset);uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(byte())<<(8*i);return v;}
    uint64_t var(unsigned bits=32){
        uint64_t v=0;
        for(unsigned shift=0;shift<bits;shift+=7){
            auto b=byte();if(bits-shift<7)require((b&127)<(uint32_t(1)<<(bits-shift)));
            v|=uint64_t(b&127)<<shift;
            if(!(b&128)){require(shift==0||(b&127)!=0);return v;}
        }
        throw Error{VCF_INVALID};
    }
    int32_t signed_var(){auto v=static_cast<uint32_t>(var());return std::bit_cast<int32_t>((v>>1)^(uint32_t(0)-(v&1)));}
    std::span<const uint8_t> raw(size_t n){require(n<=data.size()-offset);auto out=data.subspan(offset,n);offset+=n;return out;}
    std::string string(size_t bound=65535){
        auto n=var();require(n<=bound,VCF_CAPACITY);auto bytes=raw(static_cast<size_t>(n));
        std::string value(reinterpret_cast<const char*>(bytes.data()),bytes.size());utf8(value);return value;
    }
    uint32_t length(){auto n=signed_var();require(n>=0&&n<=65536,VCF_CAPACITY);return static_cast<uint32_t>(n);}
    void tag(uint8_t type,uint32_t depth){
        require(depth<=32&&++nodes<=262144,VCF_CAPACITY);
        switch(type){
            case 1:raw(1);break;
            case 2:raw(2);break;
            case 3:var();break;
            case 4:var(64);break;
            case 5:require(std::isfinite(std::bit_cast<float>(static_cast<uint32_t>(fixed(4)))));break;
            case 6:require(std::isfinite(std::bit_cast<double>(fixed(8))));break;
            case 7:raw(length());break;
            case 8:string();break;
            case 9:{auto child=byte();auto n=length();require(child<=12&&(child||!n));for(uint32_t i=0;i<n;++i)tag(child,depth+1);break;}
            case 10:{
                std::set<std::string> names;
                for(;;){auto child=byte();if(!child)break;auto name=string();require(names.insert(name).second);tag(child,depth+1);}break;
            }
            case 11:case 12:{auto n=length();require(n<=262144-nodes,VCF_CAPACITY);nodes+=n;for(uint32_t i=0;i<n;++i)var(type==11?32:64);break;}
            default:throw Error{VCF_INVALID};
        }
    }
    std::vector<uint8_t> compound(){
        auto begin=offset;require(byte()==10);string();tag(10,0);
        require(offset-begin<=1024*1024,VCF_CAPACITY);
        auto bytes=data.subspan(begin,offset-begin);return {bytes.begin(),bytes.end()};
    }
    Descriptor descriptor(){
        Descriptor d;d.numeric_id=std::bit_cast<int16_t>(static_cast<uint16_t>(fixed(2)));
        d.count=static_cast<uint16_t>(fixed(2));require(d.count<=255);
        d.auxiliary=static_cast<uint32_t>(var());
        if(boolean())d.network_id=signed_var();d.block_runtime_id=static_cast<uint32_t>(var());
        auto bytes=raw(static_cast<size_t>(var()));d.user_data.assign(bytes.begin(),bytes.end());return d;
    }
    Container container(){Container c;c.role=byte();if(boolean())c.dynamic_id=static_cast<uint32_t>(fixed(4));return c;}
    void end(){require(offset==data.size());}
};
struct Writer {
    std::vector<uint8_t> data;size_t bound=inventory_bound;
    void byte(uint8_t b){require(data.size()<bound,VCF_CAPACITY);data.push_back(b);}
    void raw(std::span<const uint8_t> bytes){require(bytes.size()<=bound-data.size(),VCF_CAPACITY);data.insert(data.end(),bytes.begin(),bytes.end());}
    void fixed(uint64_t n,size_t count){for(size_t i=0;i<count;++i)byte(static_cast<uint8_t>(n>>(8*i)));}
    void var(uint32_t v){do{auto b=static_cast<uint8_t>(v&127);v>>=7;byte(b|(v?128:0));}while(v);}
    void signed_var(int32_t n){auto v=std::bit_cast<uint32_t>(n);var((v<<1)^(uint32_t(0)-(v>>31)));}
    void string(const std::string& s){require(s.size()<=65535);utf8(s);var(static_cast<uint32_t>(s.size()));raw({reinterpret_cast<const uint8_t*>(s.data()),s.size()});}
    void descriptor(const Descriptor& d){
        require(d.count<=255&&d.user_data.size()<=inventory_bound);
        fixed(std::bit_cast<uint16_t>(d.numeric_id),2);fixed(d.count,2);var(d.auxiliary);
        byte(d.network_id.has_value());if(d.network_id)signed_var(*d.network_id);var(d.block_runtime_id);
        var(static_cast<uint32_t>(d.user_data.size()));raw(d.user_data);
    }
    void container(const Container& c){byte(c.role);byte(c.dynamic_id.has_value());if(c.dynamic_id)fixed(*c.dynamic_id,4);}
};
void bounded(std::span<const uint8_t> bytes,size_t size){require(!bytes.empty()&&bytes.size()<=size,VCF_CAPACITY);}
}
Content decode_content(std::span<const uint8_t> bytes){
    bounded(bytes,inventory_bound);Reader r{bytes};Content c;c.window=static_cast<uint32_t>(r.var());require(c.window<=255);
    auto n=r.var();require(n<=54,VCF_CAPACITY);c.items.reserve(static_cast<size_t>(n));
    for(size_t i=0;i<n;++i)c.items.push_back(r.descriptor());
    c.container=r.container();c.storage=r.descriptor();r.end();return c;
}
Slot decode_slot(std::span<const uint8_t> bytes){
    bounded(bytes,inventory_bound);Reader r{bytes};Slot s;s.window=r.byte();s.slot=static_cast<uint32_t>(r.var());require(s.slot<=255);
    if(r.boolean())s.container=r.container();if(r.boolean())s.storage=r.descriptor();s.item=r.descriptor();r.end();return s;
}
std::vector<uint8_t> encode(const Content& c){
    require(c.window<=255&&c.items.size()<=54);Writer w;w.var(c.window);w.var(static_cast<uint32_t>(c.items.size()));
    for(const auto& d:c.items)w.descriptor(d);w.container(c.container);w.descriptor(c.storage);return std::move(w.data);
}
std::vector<uint8_t> encode(const Slot& s){
    require(s.slot<=255);Writer w;w.byte(s.window);w.var(s.slot);w.byte(s.container.has_value());
    if(s.container)w.container(*s.container);w.byte(s.storage.has_value());if(s.storage)w.descriptor(*s.storage);w.descriptor(s.item);return std::move(w.data);
}
std::vector<RegistryEntry> decode_registry(std::span<const uint8_t> bytes){
    bounded(bytes,registry_bound);Reader r{bytes};auto n=r.var();require(n>0&&n<=32768,VCF_CAPACITY);
    std::vector<RegistryEntry> entries;entries.reserve(static_cast<size_t>(n));std::set<std::string> names;std::set<int16_t> numbers;
    for(size_t i=0;i<n;++i){
        RegistryEntry e;e.identifier=r.string(256);identifier(e.identifier);require(names.insert(e.identifier).second);
        e.numeric_id=std::bit_cast<int16_t>(static_cast<uint16_t>(r.fixed(2)));require(numbers.insert(e.numeric_id).second);
        e.component_based=r.boolean();e.version=r.signed_var();e.components=r.compound();entries.push_back(std::move(e));
    }
    r.end();return entries;
}
std::vector<uint8_t> encode_registry(std::span<const RegistryEntry> entries){
    require(!entries.empty()&&entries.size()<=32768);Writer w;w.bound=registry_bound;w.var(static_cast<uint32_t>(entries.size()));
    std::set<std::string> names;std::set<int16_t> numbers;uint32_t nodes=0;
    for(const auto& e:entries){
        identifier(e.identifier);require(names.insert(e.identifier).second&&numbers.insert(e.numeric_id).second);
        require(!e.components.empty()&&e.components.size()<=1024*1024,VCF_CAPACITY);
        Reader r{e.components};r.nodes=nodes;r.compound();r.end();nodes=r.nodes;
        w.string(e.identifier);w.fixed(std::bit_cast<uint16_t>(e.numeric_id),2);w.byte(e.component_based);w.signed_var(e.version);w.raw(e.components);
    }
    return std::move(w.data);
}
void Observation::registry(std::vector<RegistryEntry> entries){
    // Validate even internal callers and stage every allocation before changing
    // the observed epoch. A failed parse must be invalidated by the packet owner.
    (void)encode_registry(entries);
    if(entries==registry_)return;
    require(epoch_!=UINT64_MAX,VCF_CAPACITY);std::map<std::string,size_t,std::less<>> names;
    for(size_t i=0;i<entries.size();++i)names.emplace(entries[i].identifier,i);
    registry_.swap(entries);identifiers_.swap(names);++epoch_;invalidate();
}
void Observation::content(Content c){
    if(c.window!=0)return;
    if(c.items.size()!=36||c.container.dynamic_id||c.container.role!=12){invalidate();throw Error{VCF_INVALID};}
    if(!registry_ready()){invalidate();return;}
    std::array<Descriptor,36> next;
    for(size_t i=0;i<next.size();++i)next[i]=std::move(c.items[i]);
    inventory_.swap(next);inventory_ready_=true;
}
void Observation::slot(Slot s){
    if(s.window!=0)return;
    if(s.slot>=36||(s.container&&(s.container->dynamic_id||s.container->role!=12))){invalidate();throw Error{VCF_INVALID};}
    if(inventory_ready_)inventory_[s.slot]=std::move(s.item);
}
void Observation::invalidate(){inventory_ready_=false;for(auto& d:inventory_)d=Descriptor{};}
const std::array<Descriptor,36>& Observation::inventory()const{require(inventory_ready_,VCF_UNAVAILABLE);return inventory_;}
const RegistryEntry* Observation::find(std::string_view name)const{
    auto it=identifiers_.find(name);return it==identifiers_.end()?nullptr:&registry_.at(it->second);
}
void Inbox::discard(std::string_view key){
    auto it=players_.find(key);if(it==players_.end())return;
    for(const auto& p:it->second.pending)queued_bytes_-=p.payload.size();
    resident_bytes_-=it->second.registry_bytes+it->second.inventory_bytes;
    registry_entries_-=it->second.observed.entries().size();players_.erase(it);
}
void Inbox::submit(std::string key,uint32_t id,std::span<const uint8_t> payload){
    if(id!=49&&id!=50&&id!=162)return;
    try{
        require(!key.empty()&&key.size()<=128);
        bounded(payload,id==162?registry_bound:inventory_bound);
        require(players_.contains(key)||players_.size()<16,VCF_CAPACITY);
        require(payload.size()<=16*1024*1024-queued_bytes_,VCF_CAPACITY);
        auto& p=players_[key];require(p.pending.size()<32,VCF_CAPACITY);
        Packet packet{id,{payload.begin(),payload.end()}};p.pending.push_back(std::move(packet));queued_bytes_+=payload.size();
        // A queued update makes the previous snapshot unsafe to expose, even
        // before the next scheduler tick has decoded the update. Only stats
        // are public from Inbox; observations cannot become write authority.
    }catch(...){discard(key);if(rejected_!=UINT64_MAX)++rejected_;}
}
uint32_t Inbox::poll(uint32_t budget){
    require(budget>0&&budget<=32);uint32_t done=0;size_t scanned=0;
    while(done<budget&&!players_.empty()&&scanned<players_.size()){
        auto it=players_.upper_bound(cursor_);if(it==players_.end())it=players_.begin();cursor_=it->first;
        auto& p=it->second;if(p.pending.empty()){++scanned;continue;}scanned=0;
        auto packet=std::move(p.pending.front());p.pending.pop_front();queued_bytes_-=packet.payload.size();++done;
        try{
            if(packet.id==162){
                auto entries=decode_registry(packet.payload);
                auto count=registry_entries_-p.observed.entries().size()+entries.size();require(count<=65536,VCF_CAPACITY);
                const bool unchanged=entries==p.observed.entries();
                auto resident=resident_bytes_-p.registry_bytes-p.inventory_bytes+packet.payload.size()+(unchanged?p.inventory_bytes:0);
                require(resident<=16*1024*1024,VCF_CAPACITY);
                p.observed.registry(std::move(entries));if(!unchanged)p.inventory_bytes=0;
                registry_entries_=count;resident_bytes_=resident;p.registry_bytes=packet.payload.size();
            }else{
                if(packet.id==49)p.observed.content(decode_content(packet.payload));else p.observed.slot(decode_slot(packet.payload));
                size_t inventory=0;if(p.observed.inventory_ready()){
                    // Retained wire payload only; objects and indices have
                    // separate fixed player/entry/slot bounds above.
                    for(const auto& d:p.observed.inventory())inventory+=d.user_data.size()+32;
                }
                auto resident=resident_bytes_-p.inventory_bytes+inventory;require(resident<=16*1024*1024,VCF_CAPACITY);
                resident_bytes_=resident;p.inventory_bytes=inventory;
            }
        }catch(...){discard(cursor_);if(rejected_!=UINT64_MAX)++rejected_;}
    }
    return done;
}
void Inbox::remove(std::string_view key){discard(key);}
void Inbox::clear(){players_.clear();queued_bytes_=resident_bytes_=registry_entries_=0;cursor_.clear();}
Inbox::Stats Inbox::stats()const{
    Stats s;s.players=static_cast<uint32_t>(players_.size());s.queued_bytes=queued_bytes_;s.resident_wire_bytes=resident_bytes_;s.rejected=rejected_;
    for(const auto& [_,p]:players_){
        s.pending+=static_cast<uint32_t>(p.pending.size());
        if(p.pending.empty()){s.registries+=p.observed.registry_ready();s.inventories+=p.observed.inventory_ready();}
    }
    return s;
}
}
