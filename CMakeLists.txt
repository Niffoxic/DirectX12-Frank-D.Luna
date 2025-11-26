cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("DirectX12")

# Add source to this project's executable.
file(GLOB_RECURSE SRC_FILES
    "src/*.cpp"
    "src/*.h"
)

add_executable (application WIN32 ${SRC_FILES})

# move shaders
add_custom_command(
    TARGET application POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:application>/shaders
)

# Properties
set_property(TARGET application PROPERTY CXX_STANDARD 20)
set_property(TARGET application PROPERTY CXX_STANDARD_REQUIRED ON)

target_compile_definitions(application PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_libraries(application PRIVATE
    user32
    dxgi
    d3d12
    dxguid
    D3DCompiler
)

# structure
target_include_directories(
    application
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src 
)
