#pragma once
#include "../../inventory_revision.hpp"
namespace endstone {class Player;}
namespace oni::vcf::platform::linux_native {
// Install only at startup, on the primary server thread with no online players.
// Code and trampolines remain pinned until exit; shutdown retires every lease.
void initialize_inventory_watches();
void retire_inventory_watches(std::string_view player);
void shutdown_inventory_watches();
// Item serialization is hooked for detached save-view construction. Validate
// its installed patch instead of expecting the original function bytes.
void verify_inventory_item_save_hook();
class InventoryWatch {
 std::shared_ptr<inventory::Revision> revision_;
 uintptr_t container_=0,manager_=0;
 friend InventoryWatch watch_inventory(endstone::Player&);
public:
 inventory::Revision::Stamp capture()const;
 void validate(endstone::Player&,const inventory::Revision::Stamp&)const;
 // Internal writer admission: own setters stale the old slot epochs, but a
 // native request, retirement or different owner must still abort the batch.
 void validate_writer(endstone::Player&,uint64_t request_revision)const;
};
InventoryWatch watch_inventory(endstone::Player&);
}
