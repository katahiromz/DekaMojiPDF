# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and languages
project(DMPDF CXX RC)

# statically link
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # using Clang
    set(CMAKE_CXX_FLAGS "-static -lstdc++ -lgcc")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # using GCC
    set(CMAKE_CXX_FLAGS "-static -lstdc++ -lgcc")
elseif (MSVC)
    # replace "/MD" with "/MT" (building without runtime DLLs)
    set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    foreach(CompilerFlags ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
    endforeach()
endif()

##############################################################################

# DekaMoji.exe
add_executable(DekaMoji WIN32 DekaMoji.cpp DekaMoji_res.rc)
target_compile_definitions(DekaMoji PRIVATE -DUNICODE -D_UNICODE)
target_link_libraries(DekaMoji comctl32 shlwapi gdiplus)

##############################################################################
