#pragma once
#include <oni/vcf/core.hpp>
namespace endstone {class Server;class Player;class PacketReceiveEvent;class PacketSendEvent;}
namespace oni::vcf::inventory {class InventoryJournal;}
namespace oni::vcf::platform::linux_native {
// Reuses the independently admitted original-UI player/context check.
bool native_player_ready(endstone::Player&);
bool native_stale_close(endstone::Player&,uint8_t incoming);
class HeldStorage {
 struct Impl;std::shared_ptr<Impl> impl_;
public:
 HeldStorage(endstone::Server&,std::shared_ptr<Engine>,std::shared_ptr<inventory::InventoryJournal>);
 ~HeldStorage();
 vcf_status open(const Session&);
 vcf_status close(const Session&);
 void tick();void receive(endstone::PacketReceiveEvent&);void sent(endstone::PacketSendEvent&);void shutdown();
};
}
