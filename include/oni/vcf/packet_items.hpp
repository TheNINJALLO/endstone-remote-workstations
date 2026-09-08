#pragma once
#include "core.hpp"
#include <array>

namespace oni::vcf::wire {
// Protocol 2169 payloads, without packet headers. Descriptor bytes are an
// observed presentation, not authority to create an Item or publish inventory.
struct Descriptor {
    int16_t numeric_id=0;
    uint16_t count=0;
    uint32_t auxiliary=0;
    std::optional<int32_t> network_id;
    uint32_t block_runtime_id=0;
    std::vector<uint8_t> user_data;
    bool operator==(const Descriptor&)const=default;
};
struct Container {
    uint8_t role=0;
    std::optional<uint32_t> dynamic_id;
    bool operator==(const Container&)const=default;
};
struct Content {
    uint32_t window=0;
    std::vector<Descriptor> items;
    Container container;
    Descriptor storage;
    bool operator==(const Content&)const=default;
};
struct Slot {
    uint8_t window=0;
    uint32_t slot=0;
    std::optional<Container> container;
    std::optional<Descriptor> storage;
    Descriptor item;
    bool operator==(const Slot&)const=default;
};
struct RegistryEntry {
    std::string identifier;
    int16_t numeric_id=0;
    bool component_based=false;
    int32_t version=0;
    // Complete named compound in network NBT, preserved without conversion.
    std::vector<uint8_t> components;
    bool operator==(const RegistryEntry&)const=default;
};
Content decode_content(std::span<const uint8_t>);
Slot decode_slot(std::span<const uint8_t>);
std::vector<RegistryEntry> decode_registry(std::span<const uint8_t>);
std::vector<uint8_t> encode(const Content&);
std::vector<uint8_t> encode(const Slot&);
std::vector<uint8_t> encode_registry(std::span<const RegistryEntry>);

// A full 36-slot server packet establishes the baseline. Slot deltas cannot
// synthesize an initial inventory. A registry change invalidates the baseline.
// This cache does not check BDS item equality, source permission or save state.
class Observation {
public:
    void registry(std::vector<RegistryEntry>);
    void content(Content);
    void slot(Slot);
    void invalidate();
    bool registry_ready()const{return !registry_.empty();}
    bool inventory_ready()const{return inventory_ready_;}
    uint64_t epoch()const{return epoch_;}
    const std::vector<RegistryEntry>& entries()const{return registry_;}
    const std::array<Descriptor,36>& inventory()const;
    const RegistryEntry* find(std::string_view)const;
private:
    std::vector<RegistryEntry> registry_;
    std::map<std::string,size_t,std::less<>> identifiers_;
    std::array<Descriptor,36> inventory_{};
    bool inventory_ready_=false;
    uint64_t epoch_=0;
};
// Bounded deferred observation of outgoing packets. Packet-event code only
// copies bytes; decoding runs from poll() on the server thread. Overflow or a
// malformed packet discards that player's entire cache rather than retaining a
// partial inventory. No callback, item write or replacement packet is emitted.
class Inbox {
public:
    struct Stats {uint32_t players=0,registries=0,inventories=0,pending=0;size_t queued_bytes=0,resident_wire_bytes=0;uint64_t rejected=0;};
    void submit(std::string,uint32_t,std::span<const uint8_t>);
    uint32_t poll(uint32_t budget=1);
    void remove(std::string_view);
    void clear();
    Stats stats()const;
private:
    struct Packet {uint32_t id;std::vector<uint8_t> payload;};
    struct Player {Observation observed;std::deque<Packet> pending;size_t registry_bytes=0,inventory_bytes=0;};
    std::map<std::string,Player,std::less<>> players_;
    size_t queued_bytes_=0,resident_bytes_=0,registry_entries_=0;
    uint64_t rejected_=0;
    std::string cursor_;
    void discard(std::string_view);
};
}
