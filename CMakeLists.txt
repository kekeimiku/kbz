cmake_minimum_required(VERSION 3.20) 

set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-s")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,-s")
set(CMAKE_C_COMPILER ${tools}/bin/arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/arm-linux-gnueabihf-g++)

# set the project name 
project(kbz) 

# add the executable 
add_executable(kk kk.cc)

find_package(Threads)
target_link_libraries(kk ${CMAKE_THREAD_LIBS_INIT})
