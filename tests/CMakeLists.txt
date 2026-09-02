set(CF_TEST_PORT 6550)
add_executable(test_multiprocess test_multiprocess.cpp)
target_link_libraries(test_multiprocess PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_multiprocess)
add_test(NAME multiprocess
    COMMAND test_multiprocess
    --coord $<TARGET_FILE:collective-fabric-coordinator>
    --worker $<TARGET_FILE:collective-fabric-worker>
    --port ${CF_TEST_PORT}
    --state ${CMAKE_CURRENT_BINARY_DIR}/mp_state.bin
    --log ${CMAKE_CURRENT_BINARY_DIR}/mp_log.txt)

add_executable(test_identity test_identity.cpp)
target_link_libraries(test_identity PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_identity)
add_test(NAME test_identity COMMAND test_identity)

add_executable(test_collective_semantics test_collective_semantics.cpp)
target_link_libraries(test_collective_semantics PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_collective_semantics)
add_test(NAME test_collective_semantics COMMAND test_collective_semantics)

add_executable(test_group_membership test_group_membership.cpp)
target_link_libraries(test_group_membership PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_group_membership)
add_test(NAME test_group_membership COMMAND test_group_membership)

add_executable(test_planner test_planner.cpp)
target_link_libraries(test_planner PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_planner)
add_test(NAME test_planner COMMAND test_planner)

add_executable(test_topology test_topology.cpp)
target_link_libraries(test_topology PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_topology)
add_test(NAME test_topology COMMAND test_topology)

add_executable(test_measurement test_measurement.cpp)
target_link_libraries(test_measurement PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_measurement)
add_test(NAME test_measurement COMMAND test_measurement)

add_executable(test_health test_health.cpp)
target_link_libraries(test_health PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_health)
add_test(NAME test_health COMMAND test_health)

add_executable(test_persistence test_persistence.cpp)
target_link_libraries(test_persistence PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_persistence)
add_test(NAME test_persistence COMMAND test_persistence)

add_executable(test_property test_property.cpp)
target_link_libraries(test_property PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_property)
add_test(NAME test_property COMMAND test_property)

add_executable(test_concurrency test_concurrency.cpp)
target_link_libraries(test_concurrency PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_concurrency)
add_test(NAME test_concurrency COMMAND test_concurrency)

add_executable(test_adversarial test_adversarial.cpp)
target_link_libraries(test_adversarial PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_adversarial)
add_test(NAME test_adversarial COMMAND test_adversarial)

add_executable(test_protocol test_protocol.cpp)
target_link_libraries(test_protocol PRIVATE collectivefabric)
collectivefabric_enable_warnings(test_protocol)
add_test(NAME test_protocol COMMAND test_protocol)
if(COLLECTIVEFABRIC_ENABLE_CUDA)
  add_executable(test_cuda test_cuda.cu)
  set_target_properties(test_cuda PROPERTIES CUDA_ARCHITECTURES 120 CUDA_STANDARD 20)
  # Project CUDA code is warning-clean under /W4; suppress unavoidable vendor
  # host-stub warnings (project-controlled CUDA code remains clean).
  if(MSVC)
    set_target_properties(test_cuda PROPERTIES
        CUDA_FLAGS "-Xcompiler=/W4,/wd4204,/wd4100,/wd4324,/wd5033")
  endif()
  target_link_libraries(test_cuda PRIVATE collectivefabric cudart)
  add_test(NAME cuda COMMAND test_cuda)
endif()