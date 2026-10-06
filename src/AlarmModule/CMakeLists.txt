add_library(AlarmModule
    AlarmModule.cpp
    AlarmModule.h
)

target_include_directories(AlarmModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(AlarmModule PUBLIC ${CMAKE_SOURCE_DIR}/src/common)
target_link_libraries(AlarmModule PUBLIC tgbot::tgbot)
target_compile_definitions(AlarmModule PUBLIC HAVE_CURL)
target_link_libraries(AlarmModule PUBLIC CURL::libcurl)