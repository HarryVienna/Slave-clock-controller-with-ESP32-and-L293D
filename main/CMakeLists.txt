# main/CMakeLists.txt

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/main/*.*)

idf_component_register(SRCS ${app_sources} 
                       INCLUDE_DIRS "."
                       REQUIRES slave_clock wifi_provisioner esp_lcd)