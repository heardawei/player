cmake_minimum_required(VERSION 3.10.0)
project(player VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH true)

find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(sdl2 CONFIG REQUIRED)

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
    SDL2::SDL2
    fmt::fmt
    spdlog::spdlog)
target_link_directories(player PRIVATE 
    "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/lib")
target_compile_features(player PRIVATE cxx_std_23)
target_compile_definitions(player PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
if(CMAKE_HOST_WIN32)
    add_custom_command(TARGET player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:player> $<TARGET_RUNTIME_DLLS:player>
    COMMAND_EXPAND_LISTS
    )
    add_custom_command(TARGET player POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:player> 
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/avformat-60.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/avcodec-60.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/avdevice-60.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/avfilter-9.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/avutil-58.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/postproc-57.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/swresample-4.dll"
        "D:/code/3rd-party/ffmpeg-6.1-full_build-shared/bin/swscale-7.dll"
        COMMAND_EXPAND_LISTS
    )
endif()