add_library(AlarmModule
    AlarmModule.cpp
    AlarmModule.h
)

target_include_directories(AlarmModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AlarmModule PUBLIC ${CMAKE_SOURCE_DIR}/src/common)
target_link_libraries(AlarmModule PUBLIC tgbot::tgbot)