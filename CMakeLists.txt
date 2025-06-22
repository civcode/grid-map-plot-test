cmake_minimum_required(VERSION 3.10)

project(GridMapPlot)

set(CMAKE_CXX_STANDARD 17)
add_compile_options(-fmax-errors=3)

find_package(RenderModule REQUIRED)

include(FetchContent)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)


add_executable(GridMapPlot src/main.cpp)

target_include_directories(GridMapPlot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_link_libraries(GridMapPlot PRIVATE RenderModule::RenderModule)
