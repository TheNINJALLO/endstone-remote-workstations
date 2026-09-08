#pragma once
#include "core.hpp"
#include <array>

namespace oni::vcf::storage {
// This is the bounded protocol-2169 storage contract retained from the frozen
// Python implementation. Decoding is not permission or runtime admission.
struct Reference {
    uint8_t role=0,slot=0;
    std::optional<uint32_t> dynamic_id;
    int32_t network_id=0;
    bool operator==(const Reference&)const=default;
};
enum class Kind : uint8_t { take=0,place=1,swap=2,drop=3 };
struct Action {
    Kind kind;
    Reference source;
    std::optional<Reference> destination;
    uint8_t count=0;
    bool random_drop=false;
    bool operator==(const Action&)const=default;
};
struct Request {
    int32_t id=0,filter_origin=0;
    std::vector<Action> actions;
    bool operator==(const Request&)const=default;
};
std::vector<Request> decode(std::span<const uint8_t> payload);
struct ResponseSlot {
    uint8_t slot=0,hotbar_slot=0,count=0;
    std::optional<int32_t> network_id;
    std::string name;
    std::optional<std::string> filtered_name;
    int32_t durability=0;
    bool operator==(const ResponseSlot&)const=default;
};
struct ResponseGroup {
    uint8_t role=0;std::optional<uint32_t> dynamic_id;
    std::vector<ResponseSlot> slots;
    bool operator==(const ResponseGroup&)const=default;
};
struct Response {
    uint8_t result=0;int32_t request_id=0;
    std::optional<std::vector<ResponseGroup>> groups;
    bool operator==(const Response&)const=default;
};
std::vector<Response> decode_responses(std::span<const uint8_t>);
std::vector<uint8_t> encode_responses(std::span<const Response>);
struct NetworkSlot {
    Slot value;
    int32_t identity=0;
    bool operator==(const NetworkSlot&)const=default;
};
struct State {
    uint64_t generation=0,revision=0;
    int32_t next_identity=1;
    std::array<NetworkSlot,36> player{};
    std::vector<NetworkSlot> storage;
    NetworkSlot cursor{};
    bool operator==(const State&)const=default;
};
enum class Area { player,storage,cursor };
struct Address { Area area;uint32_t slot;bool operator==(const Address&)const=default; };
class Layout {
    uint32_t capacity_;uint8_t role_;
public:
    explicit Layout(std::string_view kind);
    Address resolve(const Reference&)const;
    uint32_t capacity()const{return capacity_;}
};
void validate(const State&,const Layout&);
bool same_inventory(const State&,const State&);
State plan(const State&,const Layout&,const Request&,uint64_t generation,uint64_t revision);

// A cursor gesture reserves changes in our presentation while real inventory
// remains at baseline. Only a completed gesture can reach the native writer.
// The writer must report uncertainty, never claim BDS and plugin saves atomic.
class Reservation {
public:
    using Reader=std::function<State()>;
    using Writer=std::function<vcf_status(const State&,const State&)>;
    struct Result { bool replayed,committed; };
    Reservation(State,Layout,Reader,Writer);
    Result apply(const Request&,uint64_t generation,uint64_t revision);
    void cancel();
    const State& state()const{return state_;}
    bool closed()const{return closed_;}
    bool quarantined()const{return quarantined_;}
private:
    State state_,baseline_;
    Layout layout_;
    Reader reader_;Writer writer_;
    bool closed_=false,quarantined_=false;
    int32_t last_request_=0;
    std::deque<Request> history_;
};
}
