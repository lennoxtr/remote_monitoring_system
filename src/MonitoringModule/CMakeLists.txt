add_library(MonitoringModule
    MonitoringModule.cpp
    MonitoringModule.h
)

target_include_directories(MonitoringModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(MonitoringModule PRIVATE nlohmann_json::nlohmann_json)
