include(CheckCSourceCompiles)
include(CheckIncludeFile)

function(sc26_configure_float128 target)
    if(NOT SC26_ENABLE_FLOAT128)
        target_compile_definitions(${target} PUBLIC SC26_DISABLE_FLOAT128)
        return()
    endif()

    check_include_file(quadmath.h SC26_HAVE_QUADMATH_H)

    set(_sc26_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
    set(CMAKE_REQUIRED_LIBRARIES quadmath)
    check_c_source_compiles(
        "#include <quadmath.h>
         int main(void) {
             __float128 value = 1.0Q;
             char text[32];
             return quadmath_snprintf(text, sizeof(text), \"%Qe\", value) < 0;
         }"
        SC26_HAVE_QUADMATH)
    set(CMAKE_REQUIRED_LIBRARIES "${_sc26_saved_required_libraries}")
    unset(_sc26_saved_required_libraries)

    if(NOT SC26_HAVE_QUADMATH_H OR NOT SC26_HAVE_QUADMATH)
        message(FATAL_ERROR
            "SC26_ENABLE_FLOAT128 requires quadmath.h and libquadmath")
    endif()

    target_link_libraries(${target} PUBLIC quadmath)
endfunction()
