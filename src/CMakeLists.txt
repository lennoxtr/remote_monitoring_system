find_package(tgbot CONFIG REQUIRED)
find_package(nlohmann_json REQUIRED)

add_subdirectory(ManagementModule)
add_subdirectory(MonitoringModule)
add_subdirectory(AlarmModule)

add_executable (Remote_Monitoring_System "main.cpp")

target_link_libraries(Remote_Monitoring_System
    PRIVATE
        ManagementModule
        MonitoringModule
        AlarmModule
)

install(TARGETS Remote_Monitoring_System DESTINATION bin)
