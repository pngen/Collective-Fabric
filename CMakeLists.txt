cmake_minimum_required(VERSION 3.20)
project(CollectiveFabric VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COLLECTIVEFABRIC_ENABLE_CUDA "Build and exercise the CUDA sm_120 proof" OFF)
option(COLLECTIVEFABRIC_BUILD_TESTS "Build test executables" ON)
option(COLLECTIVEFABRIC_BUILD_EXAMPLES "Build runnable examples" ON)
option(COLLECTIVEFABRIC_BUILD_BENCHMARKS "Build benchmarks" ON)
option(COLLECTIVEFABRIC_BUILD_TOOLS "Build CLI and coordinator/worker tools" ON)

include(GNUInstallDirs)

set(COLLECTIVEFABRIC_SOURCES
    src/enums.cpp
    src/digest.cpp
    src/planner.cpp
    src/runtime.cpp
    src/store.cpp
    src/protocol.cpp
)

add_library(collectivefabric STATIC ${COLLECTIVEFABRIC_SOURCES})
add_library(CollectiveFabric::collectivefabric ALIAS collectivefabric)

target_include_directories(collectivefabric PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
set_target_properties(collectivefabric PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Warning policy: zero project-source warnings.
function(collectivefabric_enable_warnings tgt)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /WX)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
collectivefabric_enable_warnings(collectivefabric)

# --- Install / package -----------------------------------------------------
install(TARGETS collectivefabric
    EXPORT CollectiveFabricTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/collectivefabric DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
set(COLLECTIVEFABRIC_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/CollectiveFabric")
configure_package_config_file(
    cmake/CollectiveFabricConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/CollectiveFabricConfig.cmake
    INSTALL_DESTINATION "${COLLECTIVEFABRIC_INSTALL_CMAKEDIR}")
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/CollectiveFabricConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT CollectiveFabricTargets
    FILE CollectiveFabricTargets.cmake
    NAMESPACE CollectiveFabric::
    DESTINATION "${COLLECTIVEFABRIC_INSTALL_CMAKEDIR}")
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/CollectiveFabricConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/CollectiveFabricConfigVersion.cmake
    DESTINATION "${COLLECTIVEFABRIC_INSTALL_CMAKEDIR}")

if(COLLECTIVEFABRIC_BUILD_TOOLS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools)
  add_subdirectory(tools)
endif()
if(COLLECTIVEFABRIC_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  enable_testing()
  add_subdirectory(tests)
endif()
if(COLLECTIVEFABRIC_BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
  add_subdirectory(examples)
endif()
if(COLLECTIVEFABRIC_BUILD_BENCHMARKS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks)
  add_subdirectory(benchmarks)
endif()

include(CPack)