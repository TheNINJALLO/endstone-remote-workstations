#include "runtime.hpp"
// Deliberately fake test library: a matching name/export must never admit it.
// This fixture is not installed with the plugin or examples.
extern "C" __attribute__((visibility("default"))) void* endstone_get_server(){return nullptr;}
extern "C" __attribute__((visibility("default"))) bool vcf_test_pin(){return oni::vcf::platform::pin_provider();}
