target_link_libraries(MonitoringModule PUBLIC nlohmann_json::nlohmann_json)

add_library(MonitoringModule
    MonitoringModule.cpp
    MonitoringModule.h
)