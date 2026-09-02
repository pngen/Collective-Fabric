add_executable(collective-fabric-coordinator coordinator.cpp)
target_link_libraries(collective-fabric-coordinator PRIVATE collectivefabric ws2_32)
collectivefabric_enable_warnings(collective-fabric-coordinator)

add_executable(collective-fabric-worker worker.cpp)
target_link_libraries(collective-fabric-worker PRIVATE collectivefabric ws2_32)
collectivefabric_enable_warnings(collective-fabric-worker)

add_executable(collective-fabric-cli cli.cpp)
target_link_libraries(collective-fabric-cli PRIVATE collectivefabric ws2_32)
collectivefabric_enable_warnings(collective-fabric-cli)

install(TARGETS collective-fabric-coordinator collective-fabric-worker collective-fabric-cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
