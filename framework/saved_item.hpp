#pragma once
#include "inventory_commit.hpp"
#include <oni/vcf/storage.hpp>

namespace oni::vcf::inventory::saved {
using Bytes=std::vector<uint8_t>;
using Limit=std::function<uint32_t(std::string_view)>;
// Complete BDS SaveToDisk compounds, never public user-NBT or network items.
// The normalized Item keeps Count=1 inside its complete compound so the
// storage planner can compare metadata independently of stack quantities.
Item decode(std::span<const uint8_t>,const Limit&);
Bytes encode(const Item&);
bool shulker(std::string_view);
std::array<Bytes,27> contents(std::span<const uint8_t>);
Bytes with_contents(std::span<const uint8_t>,const std::array<Bytes,27>&);
std::string durable_id(std::span<const uint8_t>);
Bytes with_durable_id(std::span<const uint8_t>,std::string_view);

// Source-slot policy is locked in the presentation. Only project() changes
// that item's Items list, in the same 36-slot image as its input/output costs.
// Native reconstruction, mutation leases, permission and durable publication
// remain mandatory at the platform boundary; this model grants no authority.
storage::State read_shulker(const SavedInventory&,uint32_t source,uint64_t generation,
                           int32_t next_identity,const Limit&);
SavedInventory project_shulker(const SavedInventory&,uint32_t source,
                              const storage::State& before,const storage::State& after,const Limit&);
}
