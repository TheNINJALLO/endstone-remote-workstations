# Docker execution has not been qualified on the Windows development host.
FROM --platform=linux/amd64 docker.io/silkeh/clang:20-bookworm@sha256:ae2f3deffd84470fbb2904cfb990db208a5f9880b4bcf9d3eae080a50a8900b4 AS toolchain
USER root
# Pin the package universe; never use the host Docker socket or private inputs.
RUN rm -f /etc/apt/sources.list.d/* && printf '%s\n' 'deb [check-valid-until=no] https://snapshot.debian.org/archive/debian/20260901T000000Z bookworm main' > /etc/apt/sources.list \
 && apt-get -o Acquire::Check-Valid-Until=false update \
 && apt-get install -y --no-install-recommends ninja-build git ca-certificates curl binutils file gdb python3 \
 && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz -o /tmp/cmake.tgz \
 && echo '5a1133ff103c71eb5120e2cc3de922733e7d8a26a98ae716397e8676adb367bf  /tmp/cmake.tgz' | sha256sum -c - \
 && tar xzf /tmp/cmake.tgz -C /opt && rm /tmp/cmake.tgz
ENV PATH="/opt/cmake-3.31.6-linux-x86_64/bin:${PATH}" CC=clang-20 CXX=clang++-20
WORKDIR /workspace
COPY cmake cmake
COPY tools/native/fetch-sdk.sh tools/native/fetch-sdk.sh
RUN sh tools/native/fetch-sdk.sh /opt/vcf-deps
COPY CMakeLists.txt CMakePresets.json ./
COPY include include
COPY framework framework
COPY third_party third_party
COPY tests/native tests/native
COPY examples/native examples/native
COPY tools/native tools/native
COPY research/original-ui-catalog.json research/original-ui-catalog.json
COPY research/original-ui-catalog.sha256 research/original-ui-catalog.sha256
RUN clang++-20 --version && cmake --version && ninja --version && ldd --version

FROM toolchain AS unit-test
RUN cmake -S . -B out/linux-release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
 -DVCF_ENDSTONE_ROOT=/opt/vcf-deps/endstone -DVCF_EXPECTED_ROOT=/opt/vcf-deps/expected-lite \
 && cmake --build out/linux-release --parallel 4 \
 && ctest --test-dir out/linux-release --output-on-failure \
 && cmake --install out/linux-release --prefix /artifacts
RUN sh tools/native/inspect-elf.sh /artifacts/plugins/endstone_onistone_vcf.so /artifacts/elf-report.txt

FROM unit-test AS abi-lab
# ptrace is granted only by compose's explicit research profile.
WORKDIR /lab
ENTRYPOINT ["/bin/bash"]

FROM toolchain AS sanitizer-test
RUN cmake -S . -B out/linux-sanitized -G Ninja -DCMAKE_BUILD_TYPE=Debug \
 -DVCF_BUILD_PLUGIN=OFF -DVCF_SANITIZERS=ON -DVCF_FUZZERS=ON \
 && cmake --build out/linux-sanitized --parallel 4 \
 && mkdir -p /sanitizer-results /fuzz-corpus \
 && printf '\012\000\000\000' > /fuzz-corpus/empty-compound \
 && printf '\012\000\000\001\001\000x\177\000' > /fuzz-corpus/byte-compound \
 && ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
 ctest --test-dir out/linux-sanitized --output-on-failure --output-junit /sanitizer-results/ctest.xml \
 && ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
 out/linux-sanitized/vcf_fuzz_nbt /fuzz-corpus -runs=100000 -max_len=65536 -timeout=5 -rss_limit_mb=1024 -seed=2169 \
 > /sanitizer-results/fuzz-nbt.txt 2>&1 \
 && cp out/linux-sanitized/Testing/Temporary/LastTest.log /sanitizer-results/

FROM scratch AS sanitizer-export
COPY --from=sanitizer-test /sanitizer-results/ /

FROM unit-test AS runtime-smoke
RUN useradd --uid 10001 --create-home vcf
USER 10001:10001
WORKDIR /data
ENTRYPOINT ["/bin/sh", "/workspace/tools/native/runtime-smoke.sh"]

FROM scratch AS artifact-export
COPY --from=unit-test /artifacts/ /
