# components/flip_clock/CMakeLists.txt

idf_component_register(SRCS "slave_clock.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio
)