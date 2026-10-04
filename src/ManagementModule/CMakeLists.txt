add_library(ManagementModule
    ManagementModule.cpp
    ManagementModule.h
)
target_include_directories(ManagementModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ManagementModule PUBLIC ${CMAKE_SOURCE_DIR}/src/common)
target_link_libraries(ManagementModule PUBLIC nlohmann_json::nlohmann_json)
