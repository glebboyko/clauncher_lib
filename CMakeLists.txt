cmake_minimum_required(VERSION 3.20)
project(clauncher)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS -pthread)

get_filename_component(DIR_INCLUDE include ABSOLUTE DIRECTORY)
set(${PROJECT_NAME}_DIR ${DIR_INCLUDE} PARENT_SCOPE)

set(CTCP_LIB "c_tcp_lib")
file(GLOB LIB_CONT "source/${CTCP_LIB}/*")
list(LENGTH LIB_CONT LIB_LENGTH)

set(MY_GIT_LINK "https://github.com/glebboyko")
if (LIB_LENGTH EQUAL 0)
    execute_process(COMMAND git clone "${MY_GIT_LINK}/${CTCP_LIB}.git" "source/${CTCP_LIB}")
else ()
    execute_process(COMMAND git -C "source/${CTCP_LIB}" pull)
endif ()

add_subdirectory("source/${CTCP_LIB}/cpp")
include_directories(${c_tcp_DIR})

include_directories(include)
include_directories(source)

add_library(${PROJECT_NAME}
        SHARED
        source/clauncher-server.cpp source/clauncher-client.cpp
        source/clauncher-supply.cpp)
add_library(${PROJECT_NAME}_lib ALIAS ${PROJECT_NAME})

add_executable(${PROJECT_NAME}_agent source/launch_agent.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${CTCP_LIB})
target_link_libraries(${PROJECT_NAME}_agent PRIVATE ${CTCP_LIB})