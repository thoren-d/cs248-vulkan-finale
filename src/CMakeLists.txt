
add_custom_target(build-time-make-directory ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/")

function(add_shader shader)
    add_custom_target(${shader} COMMAND "$ENV{VULKAN_SDK}/Bin/glslc.exe" "-o" "${CMAKE_CURRENT_BINARY_DIR}/${shader}.spv" "${CMAKE_CURRENT_SOURCE_DIR}/${shader}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${shader}" build-time-make-directory)
endfunction()

function(add_shaders)
    foreach(item ${ARGV})
        add_shader(${item})
    endforeach()
    add_custom_target(all_shaders DEPENDS ${ARGV})
endfunction()

message("$ENV{VULKAN_SDK}")

add_shaders("basic.vert" "basic.frag")

add_executable(render
        main.cpp
        app.h
        app.cpp
        camera.h
        camera.cpp
        device.h
        device.cpp
        layouts.h
        layouts.cpp
        material.h
        material.cpp
        mesh.h
        mesh.cpp
        object.h
        object.cpp
        render_passes.h
        render_passes.cpp
        renderer.h
        renderer.cpp
        resource_manager.h
        resource_manager.cpp
        stb_image_impl.cpp
        structures.h
        structures.cpp
        texture.h
        texture.cpp
        vma_impl.cpp)

add_dependencies(render all_shaders)

target_link_libraries(render
        assimp
        glfw
        glm
        "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib")
target_include_directories(render
        PRIVATE
        "$ENV{VULKAN_SDK}/Include"
        "${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator"
        "${CMAKE_SOURCE_DIR}/deps/stb")
target_compile_definitions(render PRIVATE GLFW_INCLUDE_VULKAN)


target_compile_options(render PRIVATE "/std:c++17")
# target_compile_options(render PRIVATE "--std=c++17")
