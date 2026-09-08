#include "runtime.hpp"
namespace oni::vcf::platform {
// No Windows addresses or layout assumptions are imported here.
// Docker execution and exact Linux runtime are required to admit this adapter.
Admission inspect_runtime(){return {false,{},{},"Linux runtime fingerprint and native ABI qualification are blocked pending the authorized Docker environment."};}
bool pin_provider(){return false;}
}
