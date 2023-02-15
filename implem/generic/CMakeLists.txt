add_library(media-buffers-generic_compiler_flags INTERFACE)
target_compile_features(media-buffers-generic_compiler_flags INTERFACE cxx_std_11)

# media-buffers-memory-generic
add_library(media-buffers-memory-generic src/mbuf_mem_generic.c)

target_include_directories(media-buffers-memory-generic PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>
)
target_include_directories(media-buffers-memory-generic PUBLIC
                            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/internal>
                            $<INSTALL_INTERFACE:include>
)
# define the symbol stating we are using the declspec(dllexport) when
# building on windows
target_compile_definitions(media-buffers-memory-generic PRIVATE "MBUF_API_EXPORTS")
target_compile_options(media-buffers-memory-generic PRIVATE "-fvisibility=hidden")

# state that libpomp need PIC when the default is shared libraries
set_target_properties(media-buffers-memory-generic PROPERTIES
                    POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
)

target_link_libraries(media-buffers-memory-generic PUBLIC media-buffers-memory media-buffers-memory-internal ulog media-buffers-generic_compiler_flags)
