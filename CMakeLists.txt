cmake_minimum_required(VERSION 3.20)
project(clauncher)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)

set(LIB_DIR "${CMAKE_SOURCE_DIR}/libs")
set(LIB_BUILDER "${LIB_DIR}/lib_builder.sh")
execute_process(COMMAND /bin/bash "${LIB_BUILDER}" "${LIB_DIR}/c_tcp_lib/cpp" "${LIB_DIR}/built" ".a")

include_directories(include source)

include_directories(${LIB_DIR}/c_tcp_lib/cpp/include)

add_library(${PROJECT_NAME}
        STATIC
        source/clauncher-server.cpp source/clauncher-server-runner.cpp
        source/clauncher-client.cpp
        source/clauncher-supply.cpp)
add_executable(${PROJECT_NAME}_agent source/launch_agent.cpp)

set(CTCP_BUILT "${LIB_DIR}/built/lib_c_tcp.a")
target_link_libraries(${PROJECT_NAME} PRIVATE ${CTCP_BUILT})
target_link_libraries(${PROJECT_NAME}_agent PRIVATE ${CTCP_BUILT})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib_")