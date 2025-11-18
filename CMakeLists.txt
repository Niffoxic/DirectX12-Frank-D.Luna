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

add_executable (DirectX12 WIN32 ${SRC_FILES})

# Properties
set_property(TARGET DirectX12 PROPERTY CXX_STANDARD 20)
set_property(TARGET DirectX12 PROPERTY CXX_STANDARD_REQUIRED ON)

target_compile_definitions(DirectX12 PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_libraries(DirectX12 PRIVATE
    user32
    dxgi
    d3d12
    dxguid
)
