cmake_minimum_required(VERSION 3.10.2)

project("sample")

include_directories(${CMAKE_SOURCE_DIR}/include)

add_library(jsc SHARED IMPORTED)
set_target_properties(
        jsc
        PROPERTIES
        IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/${ANDROID_ABI}/libjsc.so)

add_library(
        sample
        SHARED
        native-lib.cpp)

find_library(
        log-lib
        log)

target_link_libraries(
        sample
        jsc
        ${log-lib})