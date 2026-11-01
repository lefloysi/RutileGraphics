cmake_minimum_required(VERSION 3.28)
project(rt-opengl LANGUAGES C CXX)

find_package(Threads REQUIRED)
find_package(SPIRV-Tools CONFIG REQUIRED)

add_library(rt-opengl SHARED)

target_sources(rt-opengl PRIVATE
    "src/config.h"
    "src/context.c"
    "src/context.h"
    "src/core.c"
    "src/core.h"
    "src/error.c"
    "src/error.h"
    "src/execution/commands.cpp"
    "src/execution/execution.cpp"
    "src/execution/execution.h"
    "src/execution/internal.hpp"
    "src/glad/gl.c"
    "src/glad/gl.h"
    "src/glfw.c"
    "src/glfw.h"
    "src/KHR/khrplatform.h"
    "src/platform/context.h"
    "src/platform/proc.h"
    "src/resource/buffer.c"
    "src/resource/buffer.h"
    "src/resource/command_buffer.cpp"
    "src/resource/command_buffer.h"
    "src/resource/framebuffer.c"
    "src/resource/framebuffer.h"
    "src/resource/program.c"
    "src/resource/program.h"
    "src/resource/queue.c"
    "src/resource/queue.h"
    "src/resource/resource.c"
    "src/resource/resource.h"
    "src/resource/sampler.c"
    "src/resource/sampler.h"
    "src/resource/swapchain.c"
    "src/resource/swapchain.h"
    "src/resource/texture.c"
    "src/resource/texture.h"
    "src/rutile.h"
    "src/system/atomic.h"
    "src/system/sync.h"
    "src/transpiler/transpiler.cpp"
    "src/transpiler/transpiler.h"
)

if(WIN32)
    target_sources(rt-opengl PRIVATE
        "src/platform/wgl/context.c"
        "src/system/windows/atomic.cpp"
        "src/system/windows/sync.cpp"
    )
else()
    target_sources(rt-opengl PRIVATE
        "src/system/posix/atomic.cpp"
        "src/system/posix/sync.cpp"
        "src/platform/posix/proc.c"
    )
    if(APPLE)
        target_sources(rt-opengl PRIVATE "src/platform/cgl/context.c")
        find_library(RT_OPENGL_FRAMEWORK "OpenGL" REQUIRED)
        target_link_libraries(rt-opengl PRIVATE "${RT_OPENGL_FRAMEWORK}")
    else()
        target_sources(rt-opengl PRIVATE "src/platform/egl/context.c")
        find_path(RT_EGL_INCLUDE_DIRECTORY "EGL/egl.h" REQUIRED)
        target_include_directories(rt-opengl PRIVATE "${RT_EGL_INCLUDE_DIRECTORY}")
        target_link_libraries(rt-opengl PRIVATE "${CMAKE_DL_LIBS}")
    endif()
endif()

target_compile_features(rt-opengl PRIVATE c_std_11 cxx_std_23)
target_include_directories(rt-opengl PRIVATE "src")
target_link_libraries(rt-opengl PRIVATE
    Threads::Threads
    RTSL::rtsl
    SPIRV-Tools-static
)

if(WIN32)
    target_link_libraries(rt-opengl PRIVATE opengl32 gdi32 user32)
endif()

set_target_properties(rt-opengl PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
