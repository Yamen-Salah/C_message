cmake_minimum_required(VERSION 4.1)
project(CMessage)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(CMessage
        CMessage.ino
        message_comp.cpp
        message_comp.h
        receive.cpp
        receive.h
        transmit.cpp
        transmit.h)
