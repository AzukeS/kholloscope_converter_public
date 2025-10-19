cmake_minimum_required(VERSION 4.0)
project(kholloscope_converter C)

set(CMAKE_C_STANDARD 23)

add_executable(kholloscope_converter main.c
        converter.c
        converter.h)
