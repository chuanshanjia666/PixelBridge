# Live555 CMake Wrapper

set(LIVE555_DIR ${CMAKE_CURRENT_SOURCE_DIR}/live555)

# High-level configuration
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compile flags matching config.linux-64bit
if(UNIX)
    add_definitions(
        -DSOCKLEN_T=socklen_t
        -D_LARGEFILE_SOURCE=1
        -D_FILE_OFFSET_BITS=64
        -DALLOW_RTSP_SERVER_PORT_REUSE=1
        -DBSD=1
    )
endif()

# 1. UsageEnvironment
file(GLOB USAGE_ENV_SRCS "${LIVE555_DIR}/UsageEnvironment/*.cpp")
add_library(UsageEnvironment STATIC ${USAGE_ENV_SRCS})
target_include_directories(UsageEnvironment PUBLIC 
    ${LIVE555_DIR}/UsageEnvironment/include
    ${LIVE555_DIR}/groupsock/include
)

# 2. BasicUsageEnvironment
file(GLOB BASIC_USAGE_ENV_SRCS "${LIVE555_DIR}/BasicUsageEnvironment/*.cpp")
add_library(BasicUsageEnvironment STATIC ${BASIC_USAGE_ENV_SRCS})
target_include_directories(BasicUsageEnvironment PUBLIC 
    ${LIVE555_DIR}/BasicUsageEnvironment/include
    ${LIVE555_DIR}/UsageEnvironment/include
    ${LIVE555_DIR}/groupsock/include
)

# 3. groupsock
file(GLOB GROUPSOCK_SRCS "${LIVE555_DIR}/groupsock/*.cpp" "${LIVE555_DIR}/groupsock/*.c")
add_library(groupsock STATIC ${GROUPSOCK_SRCS})
target_include_directories(groupsock PUBLIC 
    ${LIVE555_DIR}/groupsock/include
    ${LIVE555_DIR}/UsageEnvironment/include
)

# 4. liveMedia
file(GLOB LIVE_MEDIA_SRCS "${LIVE555_DIR}/liveMedia/*.cpp" "${LIVE555_DIR}/liveMedia/*.c")
add_library(liveMedia STATIC ${LIVE_MEDIA_SRCS})
target_include_directories(liveMedia PUBLIC 
    ${LIVE555_DIR}/liveMedia/include
    ${LIVE555_DIR}/groupsock/include
    ${LIVE555_DIR}/UsageEnvironment/include
)
target_link_libraries(liveMedia PUBLIC OpenSSL::SSL OpenSSL::Crypto)

set(LIVE555_LIBRARIES liveMedia groupsock BasicUsageEnvironment UsageEnvironment PARENT_SCOPE)
set(LIVE555_INCLUDE_DIRS 
    ${LIVE555_DIR}/UsageEnvironment/include
    ${LIVE555_DIR}/BasicUsageEnvironment/include
    ${LIVE555_DIR}/groupsock/include
    ${LIVE555_DIR}/liveMedia/include
    PARENT_SCOPE
)
