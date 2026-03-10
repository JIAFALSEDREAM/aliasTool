cmake_minimum_required(VERSION 4.0)
project(aliasTool)

set(CMAKE_CXX_STANDARD 14)

add_executable(aliasTool
        app.rc
        set_folder_alias.c)
