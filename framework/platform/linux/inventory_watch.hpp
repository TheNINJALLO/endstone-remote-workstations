#pragma once
#include "../../inventory_revision.hpp"
namespace endstone {class Player;}
namespace oni::vcf::platform::linux_native {
// Install only at startup, on the primary server thread with no online players.
// Code and trampolines remain pinned until exit; shutdown retires every lease.
void initialize_inventory_watches();
void retire_inventory_watches(std::string_view player);
void shutdown_inventory_watches();
class InventoryWatch {
 std::shared_ptr<inventory::Revision> revision_;
 uintptr_t container_=0,manager_=0;
 friend InventoryWatch watch_inventory(endstone::Player&);
public:
 inventory::Revision::Stamp capture()const;
 void validate(endstone::Player&,const inventory::Revision::Stamp&)const;
};
InventoryWatch watch_inventory(endstone::Player&);
}
