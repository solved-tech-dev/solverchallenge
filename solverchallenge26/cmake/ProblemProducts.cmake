function(sc26_add_xianzhi_dependency target_name problem_config_dir)
    set(_root "")
    if(DEFINED SC26_XIANZHIKRYLOV_ROOT)
        set(_root "${SC26_XIANZHIKRYLOV_ROOT}")
    elseif(DEFINED ENV{XIANZHIKRYLOV_ROOT})
        set(_root "$ENV{XIANZHIKRYLOV_ROOT}")
    endif()

    find_path(
        SC26_XIANZHIKRYLOV_INCLUDE_DIR
        NAMES XianzhiKrylovAPI.h
        HINTS "${_root}/include"
        REQUIRED
    )
    find_library(
        SC26_XIANZHIKRYLOV_LIBRARY
        NAMES XianzhiKrylov
        HINTS "${_root}/lib"
        REQUIRED
    )
    get_filename_component(
        SC26_XIANZHIKRYLOV_LIBRARY_DIR "${SC26_XIANZHIKRYLOV_LIBRARY}" DIRECTORY)

    add_library(sc26_xianzhi_common STATIC
        "${PROJECT_SOURCE_DIR}/src/product/xianzhi_solver.c"
    )
    target_include_directories(sc26_xianzhi_common
        PUBLIC
            "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src"
            "${SC26_XIANZHIKRYLOV_INCLUDE_DIR}"
    )
    target_link_libraries(sc26_xianzhi_common
        PUBLIC
            sc26_headers
            "${SC26_XIANZHIKRYLOV_LIBRARY}"
            ${CMAKE_DL_LIBS}
            m
    )

    target_link_libraries(${target_name} INTERFACE sc26_xianzhi_common)
    target_link_options(${target_name}
        INTERFACE "LINKER:-rpath,${SC26_XIANZHIKRYLOV_LIBRARY_DIR}")
    target_compile_definitions(${target_name}
        INTERFACE
            SC26_PROBLEM_CONFIG_DIR="${problem_config_dir}"
    )
endfunction()

function(sc26_add_pangulu_direct_dependency target_name problem_config_dir)
    set(_root "")
    if(DEFINED SC26_PANGULU_ROOT)
        set(_root "${SC26_PANGULU_ROOT}")
    elseif(DEFINED ENV{PANGULU_ROOT})
        set(_root "$ENV{PANGULU_ROOT}")
    endif()

    find_path(
        SC26_PANGULU_INCLUDE_DIR
        NAMES pangulu.h
        HINTS "${_root}/include"
        REQUIRED
    )
    find_library(
        SC26_PANGULU_LIBRARY
        NAMES pangulu
        HINTS "${_root}/lib"
        REQUIRED
    )
    find_library(
        SC26_PANGULU_PLATFORM_LIBRARY
        NAMES pangulu_platform
        HINTS "${_root}/lib"
    )
    get_filename_component(
        SC26_PANGULU_LIBRARY_DIR "${SC26_PANGULU_LIBRARY}" DIRECTORY)

    add_library(sc26_pangulu_direct_common STATIC
        "${PROJECT_SOURCE_DIR}/src/product/pangulu_direct_solver.cpp"
    )
    set_target_properties(sc26_pangulu_direct_common PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
    target_include_directories(sc26_pangulu_direct_common
        PUBLIC
            "${PROJECT_SOURCE_DIR}/include"
            "${PROJECT_SOURCE_DIR}/src"
            "${SC26_PANGULU_INCLUDE_DIR}"
    )
    target_link_libraries(sc26_pangulu_direct_common
        PUBLIC
            sc26_headers
            "${SC26_PANGULU_LIBRARY}"
            ${CMAKE_DL_LIBS}
            m
            pthread
            atomic
    )
    if(SC26_PANGULU_PLATFORM_LIBRARY)
        target_link_libraries(sc26_pangulu_direct_common
            PUBLIC "${SC26_PANGULU_PLATFORM_LIBRARY}")
    endif()
    target_compile_definitions(sc26_pangulu_direct_common PRIVATE
        SC26_PROBLEM_CONFIG_DIR="${problem_config_dir}"
    )
    target_link_libraries(${target_name} INTERFACE sc26_pangulu_direct_common)
    target_link_options(${target_name}
        INTERFACE "LINKER:-rpath,${SC26_PANGULU_LIBRARY_DIR}")
    target_compile_definitions(${target_name}
        INTERFACE
            SC26_PROBLEM_CONFIG_DIR="${problem_config_dir}"
    )
endfunction()
