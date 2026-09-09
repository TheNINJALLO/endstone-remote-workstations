#pragma once
#include <oni/vcf/core.hpp>
#include <memory>
namespace endstone {class Server;class PacketReceiveEvent;class PacketSendEvent;}
namespace oni::vcf::platform::linux_native {
class NativeUi {
 struct Impl;std::unique_ptr<Impl> impl_;
public:
 NativeUi(endstone::Server&,Engine&);
 ~NativeUi();
 static bool supports(std::string_view);
 vcf_status open(const Session&);
 vcf_status close(const Session&);
 void tick();
 void receive(endstone::PacketReceiveEvent&);
 void sent(endstone::PacketSendEvent&);
 void shutdown();
};
}
