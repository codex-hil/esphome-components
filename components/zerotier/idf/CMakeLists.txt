set(ZT_ROOT "${CMAKE_CURRENT_LIST_DIR}/upstream")
file(GLOB ZT_CORE "${ZT_ROOT}/node/*.cpp")
idf_component_register(SRCS "zt_port.c" "zt_nvs.c" ${ZT_CORE} "zt_osutils.cpp" "zt_rng.c" "zt_crypto_selftest.cpp"
 INCLUDE_DIRS "${ZT_ROOT}/include" "include"
 PRIV_INCLUDE_DIRS "${ZT_ROOT}/node" "${ZT_ROOT}/ext/prometheus-cpp-lite-1.0/core/include"
 "${ZT_ROOT}/ext/prometheus-cpp-lite-1.0/simpleapi/include" "${ZT_ROOT}/ext" "${ZT_ROOT}/osdep"
 REQUIRES lwip nvs_flash esp_timer esp_netif esp_hw_support pthread bootloader_support mbedtls)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE ZT_NO_TYPE_PUNNING ZT_NO_UNALIGNED_ACCESS)

target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
