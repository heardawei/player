cmake_minimum_required(VERSION 3.0.0)
project(player VERSION 0.1.0 LANGUAGES C CXX)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

file(GLOB SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
add_executable(player ${SRC_FILES} main.cpp)
target_include_directories(player PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/include")
# target_link_libraries(player PRIVATE
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/avformat.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/avcodec.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/avdevice.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/avfilter.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/avutil.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/postproc.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/swresample.lib"
#     "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib/swscale.lib")
target_link_libraries(player PRIVATE
    avformat
    avcodec
    avdevice
    avfilter
    avutil
    postproc
    swresample
    swscale
    fmt::fmt
    spdlog::spdlog)
target_link_directories(player PRIVATE 
    "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib")
target_compile_features(player PRIVATE cxx_std_23)
