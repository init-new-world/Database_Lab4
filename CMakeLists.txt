cmake_minimum_required(VERSION 3.19)
project(code)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(code
        code.c
        extmem.c
        extmem.h
        )

add_executable(gen
        gen.c
        extmem.c
        extmem.h
)