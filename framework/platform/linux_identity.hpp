#pragma once
#include <string>
namespace oni::vcf::platform::linux_detail {
// Reads an already-open regular file from offset zero, with a 1 GiB bound.
// Throws for unreadable, non-regular or concurrently changed input.
std::string fingerprint_fd(int fd);
}
