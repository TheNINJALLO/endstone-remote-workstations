#include <oni/vcf/storage.hpp>
#include <bit>
#include <limits>
#include <utility>

namespace oni::vcf::storage {
namespace {
void utf8(std::string_view text){
    size_t i=0;
    while(i<text.size()){
        const auto first=static_cast<uint8_t>(text[i++]);if(first<128)continue;
        uint32_t code=0,minimum=0,n=0;
        if(first>=0xc2&&first<=0xdf){code=first&31;n=1;minimum=0x80;}
        else if(first>=0xe0&&first<=0xef){code=first&15;n=2;minimum=0x800;}
        else if(first>=0xf0&&first<=0xf4){code=first&7;n=3;minimum=0x10000;}
        else throw Error{VCF_INVALID};
        require(n<=text.size()-i);
        while(n--){auto c=static_cast<uint8_t>(text[i++]);require((c&0xc0)==0x80);code=(code<<6)|(c&63);}
        require(code>=minimum&&code<=0x10ffff&&!(code>=0xd800&&code<=0xdfff));
    }
}
struct Reader {
    std::span<const uint8_t> data;size_t offset=0;
    uint8_t byte(){require(offset<data.size());return data[offset++];}
    bool boolean(){auto b=byte();require(b<=1);return b!=0;}
    uint32_t fixed(){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(byte())<<(i*8);return n;}
    uint32_t var(){
        uint32_t value=0;
        for(unsigned shift=0;shift<32;shift+=7){
            auto b=byte();if(shift==28)require((b&127)<16);
            value|=uint32_t(b&127)<<shift;
            if(!(b&128)){require(shift==0||(b&127)!=0);return value;}
        }
        throw Error{VCF_INVALID};
    }
    int32_t signed_var(){auto v=var();return std::bit_cast<int32_t>((v>>1)^(uint32_t(0)-(v&1)));}
    std::string string(){
        auto size=var();require(size<=4096&&size<=data.size()-offset);
        std::string value(reinterpret_cast<const char*>(data.data()+offset),size);offset+=size;utf8(value);return value;
    }
    Reference reference(){
        Reference r;r.role=byte();if(boolean())r.dynamic_id=fixed();r.slot=byte();
        r.network_id=std::bit_cast<int32_t>(fixed());return r;
    }
};
struct Writer {
    std::vector<uint8_t> bytes;
    void byte(uint8_t b){require(bytes.size()<65536,VCF_CAPACITY);bytes.push_back(b);}
    void var(uint32_t n){do{auto b=static_cast<uint8_t>(n&127);n>>=7;byte(b|(n?128:0));}while(n);}
    void signed_var(int32_t n){auto value=std::bit_cast<uint32_t>(n);var((value<<1)^(uint32_t(0)-(value>>31)));}
    void fixed(uint32_t n){for(unsigned i=0;i<4;++i)byte(static_cast<uint8_t>(n>>(i*8)));}
    void string(const std::string& s){
        require(s.size()<=4096);utf8(s);var(static_cast<uint32_t>(s.size()));
        require(bytes.size()+s.size()<=65536,VCF_CAPACITY);bytes.insert(bytes.end(),s.begin(),s.end());
    }
    void count(size_t n){require(n<=100);var(static_cast<uint32_t>(n));}
};
void request_id(int32_t id){require(id<0&&(static_cast<uint32_t>(id)&1));}
const NetworkSlot& get(const State& s,Address a){
    if(a.area==Area::player)return s.player.at(a.slot);
    if(a.area==Area::storage)return s.storage.at(a.slot);
    require(a.slot==0);return s.cursor;
}
NetworkSlot& get(State& s,Address a){return const_cast<NetworkSlot&>(get(std::as_const(s),a));}
void permits(const NetworkSlot& slot,uint32_t operation){
    require((slot.value.policy&operation)&&!(slot.value.policy&(VCF_RESULT|VCF_PREVIEW)),VCF_DENIED);
}
void clear(NetworkSlot& slot){slot.value.item=Item{};slot.identity=0;}
}
std::vector<Request> decode(std::span<const uint8_t> payload){
    require(!payload.empty()&&payload.size()<=65536,VCF_CAPACITY);Reader reader{payload};
    auto count=reader.var();require(count>0&&count<=100);uint32_t total=0;
    std::vector<Request> requests;requests.reserve(count);
    for(uint32_t i=0;i<count;++i){
        Request request;request.id=reader.signed_var();request_id(request.id);
        auto actions=reader.var();require(actions>0&&actions<=100&&total<=100-actions,VCF_CAPACITY);total+=actions;
        for(uint32_t j=0;j<actions;++j){
            auto variant=reader.var();require(variant<=3&&reader.byte()==variant);
            Action a{};a.kind=static_cast<Kind>(variant);
            if(a.kind!=Kind::swap){a.count=reader.byte();require(a.count>0&&a.count<=64);}
            a.source=reader.reference();
            if(a.kind==Kind::drop)a.random_drop=reader.boolean();else a.destination=reader.reference();
            request.actions.push_back(std::move(a));
        }
        require(reader.var()==0);request.filter_origin=std::bit_cast<int32_t>(reader.fixed());
        requests.push_back(std::move(request));
    }
    require(reader.offset==payload.size());return requests;
}
std::vector<Response> decode_responses(std::span<const uint8_t> payload){
    require(!payload.empty()&&payload.size()<=65536,VCF_CAPACITY);Reader reader{payload};
    auto count=[&](){auto n=reader.var();require(n<=100);return n;};
    const auto size=count();require(size>0);std::vector<Response> result;
    for(uint32_t i=0;i<size;++i){
        Response response;response.result=reader.byte();response.request_id=reader.signed_var();request_id(response.request_id);
        require(reader.boolean());
        if(reader.boolean()){
            require(response.result==0);response.groups.emplace();const auto groups=count();
            for(uint32_t j=0;j<groups;++j){
                ResponseGroup group;group.role=reader.byte();if(reader.boolean())group.dynamic_id=reader.fixed();
                const auto slots=count();
                for(uint32_t k=0;k<slots;++k){
                    ResponseSlot slot;slot.slot=reader.byte();slot.hotbar_slot=reader.byte();slot.count=reader.byte();
                    require(reader.boolean());if(reader.boolean())slot.network_id=reader.signed_var();
                    require(slot.count?(slot.network_id&&*slot.network_id>0):!slot.network_id);
                    slot.name=reader.string();if(reader.boolean())slot.filtered_name=reader.string();slot.durability=reader.signed_var();
                    group.slots.push_back(std::move(slot));
                }
                response.groups->push_back(std::move(group));
            }
        }
        result.push_back(std::move(response));
    }
    require(reader.offset==payload.size());return result;
}
std::vector<uint8_t> encode_responses(std::span<const Response> responses){
    require(!responses.empty());Writer writer;writer.count(responses.size());
    for(const auto& response:responses){
        request_id(response.request_id);require(response.result==0||!response.groups);
        writer.byte(response.result);writer.signed_var(response.request_id);writer.byte(1);writer.byte(response.groups.has_value());
        if(!response.groups)continue;writer.count(response.groups->size());
        for(const auto& group:*response.groups){
            writer.byte(group.role);writer.byte(group.dynamic_id.has_value());if(group.dynamic_id)writer.fixed(*group.dynamic_id);writer.count(group.slots.size());
            for(const auto& slot:group.slots){
                require(slot.count?(slot.network_id&&*slot.network_id>0):!slot.network_id);
                writer.byte(slot.slot);writer.byte(slot.hotbar_slot);writer.byte(slot.count);writer.byte(1);writer.byte(slot.network_id.has_value());
                if(slot.network_id)writer.signed_var(*slot.network_id);
                writer.string(slot.name);writer.byte(slot.filtered_name.has_value());if(slot.filtered_name)writer.string(*slot.filtered_name);writer.signed_var(slot.durability);
            }
        }
    }
    return std::move(writer.bytes);
}
Layout::Layout(std::string_view kind){
    // Capacities and canonical IDs come from the retained catalog. The role
    // mapping is limited to storage; no workstation or bundle role is guessed.
    auto* row=oni::vcf::resolve(kind);require(row&&row->family=="storage",VCF_UNAVAILABLE);
    require(row->id=="chest"||row->id=="doublechest"||row->id=="hopper"||row->id=="dispenser"
        ||row->id=="barrel"||row->id=="dropper"||row->id=="trappedchest"||row->id=="enderchest"||row->id=="shulker",VCF_UNAVAILABLE);
    capacity_=row->capacity;role_=row->id=="shulker"?30:7;
}
Address Layout::resolve(const Reference& ref)const{
    require(!ref.dynamic_id,VCF_DENIED);
    if(ref.role==role_){require(ref.slot<capacity_);return {Area::storage,ref.slot};}
    if(ref.role==59){require(ref.slot==0);return {Area::cursor,0};}
    if(ref.role==28){require(ref.slot<9);return {Area::player,ref.slot};}
    if(ref.role==29){require(ref.slot>=9&&ref.slot<36);return {Area::player,ref.slot};}
    if(ref.role==12){require(ref.slot<36);return {Area::player,ref.slot};}
    throw Error{VCF_DENIED};
}
void validate(const State& state,const Layout& layout){
    require(state.generation&&state.storage.size()==layout.capacity()&&state.next_identity>0);
    std::set<int32_t> identities;size_t bytes=0;
    auto slot=[&](const NetworkSlot& s){
        const auto& item=s.value.item;require(s.value.policy<=15);
        require(!((s.value.policy&VCF_PREVIEW)&&(s.value.policy&(VCF_INSERT|VCF_EXTRACT|VCF_RESULT))));
        if(item.empty()){require(s.identity==0&&item.id.empty()&&item.nbt.empty());return;}
        require(!item.id.empty()&&item.limit>0&&item.limit<=255&&item.count<=item.limit);
        require(s.identity>0&&s.identity<state.next_identity&&identities.insert(s.identity).second);
        bytes+=item.nbt.size();require(bytes<=1024*1024,VCF_CAPACITY);
    };
    for(const auto& s:state.player)slot(s);for(const auto& s:state.storage)slot(s);slot(state.cursor);
}
bool same_inventory(const State& a,const State& b){
    if(a.generation!=b.generation||a.storage.size()!=b.storage.size())return false;
    for(size_t i=0;i<a.player.size();++i)if(a.player[i].value.item!=b.player[i].value.item)return false;
    for(size_t i=0;i<a.storage.size();++i)if(a.storage[i].value.item!=b.storage[i].value.item)return false;
    return true;
}
State plan(const State& before,const Layout& layout,const Request& request,uint64_t generation,uint64_t revision){
    validate(before,layout);request_id(request.id);
    require(before.generation==generation&&before.revision==revision,VCF_STALE);
    require(before.revision!=UINT64_MAX,VCF_CAPACITY);
    require(!request.actions.empty()&&request.actions.size()<=100);
    State after=before;
    auto reference=[&](const Reference& ref){
        auto address=layout.resolve(ref);require(get(before,address).identity==ref.network_id,VCF_STALE);return address;
    };
    for(const auto& action:request.actions){
        require(action.kind==Kind::take||action.kind==Kind::place||action.kind==Kind::swap,VCF_DENIED);
        require(action.destination.has_value());auto from=reference(action.source),to=reference(*action.destination);
        require(from!=to);auto& source=get(after,from);auto& dest=get(after,to);
        if(action.kind==Kind::swap){
            if(!source.value.item.empty()){permits(source,VCF_EXTRACT);permits(dest,VCF_INSERT);}
            if(!dest.value.item.empty()){permits(dest,VCF_EXTRACT);permits(source,VCF_INSERT);}
            std::swap(source.value.item,dest.value.item);std::swap(source.identity,dest.identity);continue;
        }
        permits(source,VCF_EXTRACT);permits(dest,VCF_INSERT);
        auto amount=action.count;require(amount>0&&amount<=64&&source.value.item.count>=amount,VCF_CONFLICT);
        require(dest.value.item.empty()||dest.value.item.matches(source.value.item),VCF_CONFLICT);
        require(amount<=source.value.item.limit&&dest.value.item.count<=source.value.item.limit-amount,VCF_CAPACITY);
        auto moved=source.value.item;moved.count=amount;auto identity=source.identity;
        if(amount<source.value.item.count){
            require(after.next_identity<INT32_MAX,VCF_CAPACITY);identity=after.next_identity++;
            source.value.item.count-=amount;
        }else clear(source);
        if(dest.value.item.empty()){dest.value.item=std::move(moved);dest.identity=identity;}
        else dest.value.item.count+=amount;
    }
    ++after.revision;validate(after,layout);return after;
}
Reservation::Reservation(State state,Layout layout,Reader reader,Writer writer,Guard guard)
    :state_(std::move(state)),baseline_(state_),layout_(layout),reader_(std::move(reader)),writer_(std::move(writer)),guard_(std::move(guard)){
    validate(state_,layout_);require(state_.cursor.value.item.empty()&&reader_&&writer_&&guard_);guard_();
}
Reservation::Result Reservation::apply(const Request& request,uint64_t generation,uint64_t revision){
    require(!closed_,quarantined_?VCF_QUARANTINED:VCF_CLOSED);
    require(generation==state_.generation,VCF_STALE);request_id(request.id);
    auto admitted=[&]{try{guard_();}catch(const Error&e){closed_=true;quarantined_=e.status==VCF_QUARANTINED;throw;}
        catch(...){closed_=true;throw Error{VCF_CONFLICT};}};
    admitted();
    bool matching=false;try{matching=same_inventory(baseline_,reader_());}catch(...){}
    admitted();
    if(!matching){closed_=true;throw Error{VCF_CONFLICT};}
    for(const auto& old:history_)if(old.id==request.id){require(old==request,VCF_STALE);return {true,false};}
    require(request.id<last_request_,VCF_STALE);last_request_=request.id;
    auto next=plan(state_,layout_,request,generation,revision);const bool commit=next.cursor.value.item.empty();
    // Allocate the replay record before publishing anything to a native writer.
    history_.push_back(request);if(history_.size()>128)history_.pop_front();
    if(commit){
        admitted();
        // Keep uncertainty latched through post-write reads AND allocations.
        // An exception after native mutation must never leave a retryable owner.
        closed_=true;quarantined_=true;
        vcf_status result=VCF_QUARANTINED;
        try{result=writer_(baseline_,next);}catch(...){}
        if(result!=VCF_OK){closed_=true;quarantined_=result!=VCF_CONFLICT;throw Error{quarantined_?VCF_QUARANTINED:VCF_CONFLICT};}
        bool verified=false;try{verified=same_inventory(next,reader_());}catch(...){}
        if(!verified)throw Error{VCF_QUARANTINED};
        baseline_=next;
    }
    state_=std::move(next);closed_=false;quarantined_=false;
    return {false,commit};
}
void Reservation::cancel(){
    closed_=true;history_.clear();
    // After uncertain native publication, retain both copies for reconciliation.
    // Never roll back by overwriting real inventory or issue replacement items.
    if(!quarantined_)state_=baseline_;
}
}
