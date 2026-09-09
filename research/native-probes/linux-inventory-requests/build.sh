#!/bin/sh
set -eu
set -- -I/headers/endstone-0.11.10/include -I/headers/endstone-0.11.10/src -I/headers/expected-lite/include -I/headers/entt/src -I/headers/glm -I/headers/ms-gsl/include -I/headers/raknet/Source -I/headers/nlohmann-json/include
for dependency in /headers/boost-*; do set -- "$@" "-I$dependency/include"; done
clang++-20 -std=c++20 -stdlib=libc++ -O2 -DNDEBUG -DENTT_SPARSE_PAGE=2048 -DENTT_PACKED_PAGE=128 -DENTT_NO_MIXIN -DRAKNET_SUPPORT_IPV6=1 -fms-extensions -fno-access-control -fPIC -shared -fvisibility=hidden -I/generated -I/source -I/repo/include -I/repo "$@" /source/probe.cpp /repo/framework/platform/linux.cpp /repo/framework/transactions.cpp -lcrypto -ldl -o /output/endstone_vcf_private_request_probe.so
clang++-20 -std=c++20 -stdlib=libc++ -DNDEBUG -DENTT_SPARSE_PAGE=2048 -DENTT_PACKED_PAGE=128 -DENTT_NO_MIXIN -DRAKNET_SUPPORT_IPV6=1 -fms-extensions -fno-access-control -I/generated -O2 -S -emit-llvm "$@" /source/layout.cpp -o /output/hook-layout.ll
