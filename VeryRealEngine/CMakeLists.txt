cmake_minimum_required(VERSION 3.16)
project(VeryRealEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Vulkan REQUIRED)

if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
endif()

add_executable(very_real_engine
    src/main.cpp
    src/renderer/renderer.cpp
    src/assets/obj_loader.cpp
    src/assets/tga_loader.cpp
    src/assets/json_parser.cpp
    src/scene/scene.cpp
    src/physics/physics_world.cpp
)

if(UNIX AND NOT APPLE)
    target_sources(very_real_engine PRIVATE src/platform/linux/window_linux.cpp)
    target_link_libraries(very_real_engine PRIVATE ${X11_LIBRARIES})
    target_include_directories(very_real_engine PRIVATE ${X11_INCLUDE_DIR})
endif()

target_link_libraries(very_real_engine PRIVATE Vulkan::Vulkan)
target_include_directories(very_real_engine PRIVATE src)

# --- Shader compilation ---------------------------------------------------
# Prefer glslc (shaderc), fall back to glslangValidator. Neither ships with
# the base OS image; install one of:
#   sudo apt install glslc                 # or
#   sudo apt install glslang-tools
find_program(GLSLC_EXECUTABLE glslc)
find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if(NOT GLSLC_EXECUTABLE AND NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(WARNING
        "Neither glslc nor glslangValidator was found. Shaders will not be "
        "compiled to SPIR-V; the engine will fail to load shaders/*.spv at "
        "runtime. Install one with e.g. 'sudo apt install glslang-tools'.")
endif()

set(SHADER_SOURCES
    ${CMAKE_SOURCE_DIR}/shaders/mesh.vert
    ${CMAKE_SOURCE_DIR}/shaders/mesh.frag
    ${CMAKE_SOURCE_DIR}/shaders/shadow.vert
)
set(SHADER_OUTPUT_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)

foreach(SHADER_SOURCE ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
    set(SHADER_OUTPUT ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv)

    if(GLSLC_EXECUTABLE)
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
            COMMAND ${GLSLC_EXECUTABLE} ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling shader ${SHADER_NAME} with glslc"
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    elseif(GLSLANG_VALIDATOR_EXECUTABLE)
        add_custom_command(
            OUTPUT ${SHADER_OUTPUT}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
            COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling shader ${SHADER_NAME} with glslangValidator"
        )
        list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
    endif()
endforeach()

if(SHADER_OUTPUTS)
    add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
    add_dependencies(very_real_engine shaders)
endif()

# The renderer loads "shaders/*.spv" and "assets/..." relative to the
# working directory it's run from, so keep copies next to the binary too.
add_custom_command(TARGET very_real_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:very_real_engine>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders
        $<TARGET_FILE_DIR:very_real_engine>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:very_real_engine>/assets
)
