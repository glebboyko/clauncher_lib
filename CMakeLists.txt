cmake_minimum_required(VERSION 3.20)
project(clauncher)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)

set(LIBS_DIR "${CMAKE_SOURCE_DIR}/external_libs")

execute_process(COMMAND chmod u+x "${LIBS_DIR}/build_ctcp.sh")
execute_process(COMMAND "${LIBS_DIR}/build_ctcp.sh" "${LIBS_DIR}/c_tcp_lib" "${LIBS_DIR}/built_libs")

set(CTCP_LIB "lib_c_tcp.a")

include_directories(include source)
include_directories(external_libs/c_tcp_lib/cpp/include)

link_directories(external_libs/built_libs)

add_library(${PROJECT_NAME}
        STATIC
        source/clauncher-server.cpp source/clauncher-server-runner.cpp
        source/clauncher-client.cpp
        source/clauncher-supply.cpp)
add_executable(${PROJECT_NAME}_agent source/launch_agent.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CTCP_LIB})
target_link_libraries(${PROJECT_NAME}_agent PRIVATE ${CTCP_LIB})

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "lib_")