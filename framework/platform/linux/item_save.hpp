#pragma once
#include <oni/vcf/core.hpp>
namespace endstone {class Player;}
namespace oni::vcf::platform::linux_native {
// A synchronous detached observation, never an inventory reservation or write.
InventoryItemSnapshot read_inventory_item(endstone::Player&,uint32_t slot);
}
