#pragma once
#include <string>
namespace endstone {class Player;}
namespace oni::vcf::platform::windows {
struct NativeState {bool ready,manager_active;int window;};
void initialize_bridge();
NativeState inspect(endstone::Player&);
int station(endstone::Player&,const std::string&,int,int,int);
int linked(endstone::Player&,const std::string&,int,int,int);
void refresh(endstone::Player&);
void shutdown_editors();
}
