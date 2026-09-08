#pragma once
#include <string>
namespace oni::vcf::platform {
struct Admission {bool accepted=false;std::string bds_sha256,runtime_sha256,reason;};
Admission inspect_runtime();
bool pin_provider();
}
