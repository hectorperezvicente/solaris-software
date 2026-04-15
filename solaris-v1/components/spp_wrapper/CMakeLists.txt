# ----------------------------------------------------------------
# spp_wrapper — ESP-IDF component for SPP core + services
#
# Compiles everything in spp/src/ and spp/services/**/src/ and
# exposes spp/include/ as the single public include root.
# Platform-specific ports come from spp_port_wrapper.
# ----------------------------------------------------------------

# Locate external/spp
set(_bases "${CMAKE_CURRENT_LIST_DIR};${PROJECT_SOURCE_DIR};${CMAKE_SOURCE_DIR}")
set(_candidates "")
if(SPP_ROOT)
    list(APPEND _candidates "${SPP_ROOT}")
endif()
if(DEFINED ENV{SPP_ROOT})
    list(APPEND _candidates "$ENV{SPP_ROOT}")
endif()
foreach(b ${_bases})
    get_filename_component(babs "${b}" ABSOLUTE)
    set(cur "${babs}")
    foreach(_n 0 1 2 3 4 5)
        if(EXISTS "${cur}/external/spp")
            list(APPEND _candidates "${cur}/external/spp")
        endif()
        get_filename_component(cur "${cur}/.." ABSOLUTE)
    endforeach()
endforeach()
list(REMOVE_DUPLICATES _candidates)
set(SPP_ROOT "")
foreach(p ${_candidates})
    if(EXISTS "${p}/include/spp/core/core.h")
        set(SPP_ROOT "${p}")
        break()
    endif()
endforeach()
if(SPP_ROOT STREQUAL "")
    message(FATAL_ERROR "spp_wrapper: could not find external/spp. Set SPP_ROOT.")
endif()
message(STATUS "spp_wrapper: SPP_ROOT = ${SPP_ROOT}")

# All .c files: core library + sensor services
file(GLOB_RECURSE SPP_SRC_FILES     "${SPP_ROOT}/src/*.c")
file(GLOB_RECURSE SPP_SERVICE_FILES "${SPP_ROOT}/services/*.c")

# Single include root covers everything: spp/core/, spp/hal/, spp/services/, etc.
idf_component_register(
    SRCS         ${SPP_SRC_FILES} ${SPP_SERVICE_FILES}
    INCLUDE_DIRS "${SPP_ROOT}/include"
)
