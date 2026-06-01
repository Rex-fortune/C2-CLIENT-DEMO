cmake_minimum_required(VERSION 3.10)

project(CPPRemoteClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(src)

# Source files
file(GLOB SOURCES
    src/*.cpp
)

# Create the executable
add_executable(client ${SOURCES})

# Platform-specific link libraries
if(WIN32)
    target_link_libraries(client PRIVATE ws2_32 gdiplus gdi32 ole32 uuid strmiids)
elseif(APPLE)
    find_library(COREFOUNDATION CoreFoundation)
    target_link_libraries(client PRIVATE ${COREFOUNDATION})
else()
    # Linux: link pthread and rt if needed
    target_link_libraries(client PRIVATE pthread rt)
endif()

install(TARGETS client RUNTIME DESTINATION bin)