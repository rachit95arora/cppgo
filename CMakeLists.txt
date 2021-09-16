cmake_minimum_required(VERSION 3.10)

# set the project name
project(cppgo)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lrt")

find_library(LIBRT rt) 
# add the library
add_library(cppgolib 
"${PROJECT_SOURCE_DIR}/src/Context.cpp"
"${PROJECT_SOURCE_DIR}/src/Executor.cpp"
"${PROJECT_SOURCE_DIR}/src/Machines.cpp"
"${PROJECT_SOURCE_DIR}/src/Processor.cpp"
"${PROJECT_SOURCE_DIR}/src/Routine.cpp"
)

# lib include dirs
target_include_directories(cppgolib PUBLIC
                           "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(cppgolib ${LIBRT})

# add the executable
add_executable(Run Run.x.cpp)
target_link_libraries(Run PUBLIC cppgolib)

