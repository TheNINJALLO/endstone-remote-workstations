# The SDK version tracks the public C ABI, separately from the provider release.
include(CMakePackageConfigHelpers)
add_library(vcf_sdk_abi INTERFACE)
add_library(OnistoneVCF::abi ALIAS vcf_sdk_abi)
set_target_properties(vcf_sdk_abi PROPERTIES EXPORT_NAME abi)
target_include_directories(vcf_sdk_abi INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
add_library(vcf_sdk INTERFACE)
add_library(OnistoneVCF::sdk ALIAS vcf_sdk)
set_target_properties(vcf_sdk PROPERTIES EXPORT_NAME sdk)
target_link_libraries(vcf_sdk INTERFACE vcf_sdk_abi "$<BUILD_INTERFACE:${CMAKE_DL_LIBS}>")
target_compile_features(vcf_sdk INTERFACE cxx_std_20)

# Configure relative to the standalone sdk/ root: users may copy and rename
# that directory without retaining the provider's artifact directory layout.
configure_package_config_file("${CMAKE_CURRENT_LIST_DIR}/OnistoneVCFConfig.cmake.in"
 "${PROJECT_BINARY_DIR}/OnistoneVCFConfig.cmake" INSTALL_DESTINATION lib/cmake/OnistoneVCF)
write_basic_package_version_file("${PROJECT_BINARY_DIR}/OnistoneVCFConfigVersion.cmake"
 VERSION 1.4.0 COMPATIBILITY SameMajorVersion ARCH_INDEPENDENT)
install(TARGETS vcf_sdk_abi vcf_sdk EXPORT OnistoneVCFTargets COMPONENT sdk)
install(EXPORT OnistoneVCFTargets NAMESPACE OnistoneVCF:: DESTINATION sdk/lib/cmake/OnistoneVCF COMPONENT sdk)
install(FILES "${PROJECT_BINARY_DIR}/OnistoneVCFConfig.cmake" "${PROJECT_BINARY_DIR}/OnistoneVCFConfigVersion.cmake"
 DESTINATION sdk/lib/cmake/OnistoneVCF COMPONENT sdk)
install(FILES include/oni/vcf/abi.h include/oni/vcf/sdk.hpp include/oni/vcf/loader.hpp DESTINATION sdk/include/oni/vcf COMPONENT sdk)
install(FILES LICENSE DESTINATION sdk COMPONENT sdk)
