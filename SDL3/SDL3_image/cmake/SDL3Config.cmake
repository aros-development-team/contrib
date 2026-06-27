# Minimal SDL3Config.cmake shim for the AROS SDL3 port.
#
# The AROS SDL3 library is built as a native AROS module and therefore does
# not ship the SDL3Config.cmake that SDL's own CMake build would normally
# install.  CMake config-mode find_package(SDL3) consumers (such as
# SDL3_image) require it, so this shim recreates the imported targets they
# expect, using the SDL3 include directory and link library that are passed
# in from the AROS mmakefile via -DSDL3_INCLUDE_DIR / -DSDL3_LIBRARY.

if(NOT DEFINED SDL3_INCLUDE_DIR OR NOT DEFINED SDL3_LIBRARY)
    message(FATAL_ERROR
        "The AROS SDL3Config shim requires -DSDL3_INCLUDE_DIR and -DSDL3_LIBRARY")
endif()

set(SDL3_FOUND TRUE)
set(SDL3_VERSION "3.4.10")

# SDL3::Headers - include directories only.
if(NOT TARGET SDL3::Headers)
    add_library(SDL3::Headers INTERFACE IMPORTED)
    set_target_properties(SDL3::Headers PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}")
endif()

# SDL3::SDL3-static - the static link library.
if(NOT TARGET SDL3::SDL3-static)
    add_library(SDL3::SDL3-static STATIC IMPORTED)
    set_target_properties(SDL3::SDL3-static PROPERTIES
        IMPORTED_LOCATION "${SDL3_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_INCLUDE_DIR}")
endif()

# SDL3::SDL3 - the generic name a static consumer resolves; map it to the
# static library so the final executable links against libSDL3.a.
if(NOT TARGET SDL3::SDL3)
    add_library(SDL3::SDL3 INTERFACE IMPORTED)
    set_target_properties(SDL3::SDL3 PROPERTIES
        INTERFACE_LINK_LIBRARIES "SDL3::SDL3-static")
endif()

# Satisfy find_package(... COMPONENTS ...).
foreach(_comp ${SDL3_FIND_COMPONENTS})
    set(SDL3_${_comp}_FOUND TRUE)
endforeach()
