cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(peripheral_nus)

target_sources(app PRIVATE 
    src/main.c

    src/resources/img_88112_261123_youwe.c
    src/resources/img_88112_261123_youwe2.c
    src/resources/img_88112_261123_tel.c
    src/resources/img_88112_261123_carolie.c
    src/resources/img_88112_261123_hr.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})