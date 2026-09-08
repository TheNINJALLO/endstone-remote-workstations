#include "runtime.hpp"
namespace oni::vcf::platform {
// No Windows addresses or layout assumptions are imported here.
// Docker execution and exact Linux runtime are required to admit this adapter.
Admission inspect_runtime(){return {false,{},{},"Linux BDS and loader fingerprints have not been qualified against an authorized private runtime; native operations remain disabled despite successful Docker builds."};}
bool pin_provider(){return false;}
}
