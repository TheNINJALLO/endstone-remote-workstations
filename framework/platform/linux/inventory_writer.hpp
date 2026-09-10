#pragma once
#include "../../inventory_commit.hpp"
namespace endstone {class Player;}
namespace oni::vcf::platform::linux_native {
class InventoryWriter {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 using Resolve=std::function<endstone::Player*()>;
 explicit InventoryWriter(Resolve,std::function<void()> initial_admission={});
 ~InventoryWriter();
 InventoryWriter(const InventoryWriter&)=delete;InventoryWriter&operator=(const InventoryWriter&)=delete;
 const inventory::SavedInventory& before()const;
 void validate()const;
 // A writer is single use, even when admission or preparation is refused.
 inventory::CommitResult commit(const inventory::SavedInventory&,std::function<void()> authorize,std::function<void(Boundary)> record);
};
}
