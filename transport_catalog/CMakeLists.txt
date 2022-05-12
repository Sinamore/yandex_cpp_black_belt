cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

find_package(Protobuf REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(include)

set(PROTO_DIR proto)
protobuf_generate_cpp(
    PROTO_SRCS
    PROTO_HDRS
    ${PROTO_DIR}/address.proto
    ${PROTO_DIR}/company.proto
    ${PROTO_DIR}/database.proto
    ${PROTO_DIR}/name.proto
    ${PROTO_DIR}/phone.proto
    ${PROTO_DIR}/rubric.proto
    ${PROTO_DIR}/sphere.proto
    ${PROTO_DIR}/transport_catalog.proto
    ${PROTO_DIR}/url.proto
    ${PROTO_DIR}/working_time.proto
)

set(CMAKE_CXX_FLAGS "-g -O2 -Wall -Wextra -fsanitize=address,undefined")

add_executable(main
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    src/bus.cpp
    src/company.cpp
    src/database.cpp
    src/database_render.cpp
    src/database_serializer.cpp
    src/database_yellow_pages.cpp
    src/json.cpp
    src/main.cpp)

target_link_libraries(main ${Protobuf_LIBRARIES})
