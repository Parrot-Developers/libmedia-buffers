cmake_minimum_required(VERSION 3.15)
project(libmedia-buffers VERSION 1.0)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(media-buffers_compiler_flags INTERFACE)
target_compile_features(media-buffers_compiler_flags INTERFACE cxx_std_11)

# media-buffers-memory
add_library(media-buffers-memory src/mbuf_mem.c)

target_include_directories(media-buffers-memory PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)
# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(media-buffers-memory PRIVATE "MBUF_API_EXPORTS")
target_compile_options(media-buffers-memory PRIVATE "-fvisibility=hidden")

# state that libpomp need PIC when the default is shared libraries
set_target_properties(media-buffers-memory PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_link_libraries(media-buffers-memory futils ulog pomp media-buffers_compiler_flags)

# media-buffers-memory-internal
add_library(media-buffers-memory-internal INTERFACE)
target_include_directories(media-buffers-memory-internal INTERFACE src/internal)


# media-buffers-generic
add_subdirectory(implem/generic)

# media-buffers
set(LOCAL_SRC_FILES
        src/mbuf_ancillary_data.c
        src/mbuf_base_frame.c
        src/mbuf_coded_video_frame.c
        src/mbuf_raw_video_frame.c
        src/mbuf_utils.c
)
add_library(media-buffers ${LOCAL_SRC_FILES})

target_include_directories(media-buffers PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)
# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(media-buffers PRIVATE "MBUF_API_EXPORTS")
target_compile_options(media-buffers PRIVATE "-fvisibility=hidden")

# state that libpomp need PIC when the default is shared libraries
set_target_properties(media-buffers PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_link_libraries(media-buffers PUBLIC futils media-buffers-memory media-buffers-memory-internal ulog pomp video-defs video-metadata media-buffers_compiler_flags)
