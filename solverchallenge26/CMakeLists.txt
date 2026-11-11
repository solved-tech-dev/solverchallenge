cmake_minimum_required(VERSION 3.21)

project(SolverChallenge26 LANGUAGES C CXX)

include(CTest)
find_package(Python3 COMPONENTS Interpreter QUIET)
include(cmake/Dependencies.cmake)
include(cmake/ProblemProducts.cmake)

if(UNIX AND NOT APPLE)
    add_link_options("LINKER:--allow-shlib-undefined")
endif()

option(SC26_BUILD_MPI "Build MPI solver samples" ON)
option(SC26_BUILD_MPI_GPU "Build the MPI GPU direct and iterative solvers" OFF)
option(SC26_ENABLE_FLOAT128 "Enable float128 residual checks" ON)
option(SC26_SOLUTION_FP128 "Use float128 solution vectors" OFF)
set(SC26_PROBLEM "01" CACHE STRING "Problem slot to build: 01 through 10")
set_property(CACHE SC26_PROBLEM PROPERTY STRINGS 01 02 03 04 05 06 07 08 09 10)
set(SC26_TEST_MATRIX "" CACHE FILEPATH "Optional matrix for the MPI smoke test")
set(SC26_TEST_RHS "" CACHE FILEPATH "Optional right-hand side for the MPI smoke test")

if(SC26_PROBLEM MATCHES "^(01|02|03)$" AND NOT SC26_SOLUTION_FP128)
    message(FATAL_ERROR
        "Problems 01, 02, and 03 require -DSC26_SOLUTION_FP128=ON")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(SC26_BUILD_MPI_GPU AND NOT SC26_BUILD_MPI)
    message(FATAL_ERROR "SC26_BUILD_MPI_GPU requires SC26_BUILD_MPI=ON")
endif()

if(SC26_BUILD_MPI_GPU)
    include(CheckLanguage)
    check_language(CUDA)
    if(NOT CMAKE_CUDA_COMPILER)
        message(FATAL_ERROR
            "SC26_BUILD_MPI_GPU requires a CUDA compiler; configure with "
            "-DSC26_BUILD_MPI_GPU=OFF on CPU-only hosts")
    endif()
    enable_language(CUDA)
    set(CMAKE_CUDA_STANDARD 14)
    set(CMAKE_CUDA_STANDARD_REQUIRED ON)
    find_package(CUDAToolkit REQUIRED)
endif()

add_library(sc26_headers INTERFACE)
target_include_directories(sc26_headers INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
target_compile_options(sc26_headers INTERFACE -Wall -Wextra)
target_compile_definitions(sc26_headers INTERFACE SC26_PROBLEM_ID="${SC26_PROBLEM}")
if(SC26_SOLUTION_FP128)
    if(NOT SC26_ENABLE_FLOAT128)
        message(FATAL_ERROR "SC26_SOLUTION_FP128 requires SC26_ENABLE_FLOAT128=ON")
    endif()
    target_compile_definitions(sc26_headers INTERFACE SC26_SOLUTION_FP128)
endif()

if(SC26_PROBLEM MATCHES "^(0[1-9]|10)$")
    set(SC26_PROBLEM_DIR "${PROJECT_SOURCE_DIR}/problems/problem${SC26_PROBLEM}")
else()
    message(FATAL_ERROR "SC26_PROBLEM must be a value from 01 to 10")
endif()
if(NOT EXISTS "${SC26_PROBLEM_DIR}/solver.h")
    message(FATAL_ERROR "Missing solver interface: ${SC26_PROBLEM_DIR}/solver.h")
endif()
if(NOT EXISTS "${SC26_PROBLEM_DIR}/solver.c")
    message(FATAL_ERROR "Missing solver implementation: ${SC26_PROBLEM_DIR}/solver.c")
endif()
if(NOT EXISTS "${SC26_PROBLEM_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing problem build configuration: ${SC26_PROBLEM_DIR}/CMakeLists.txt")
endif()
if(SC26_BUILD_MPI_GPU)
    if(NOT EXISTS "${SC26_PROBLEM_DIR}/gpu_direct_solver.cu")
        message(FATAL_ERROR "Missing MPI GPU direct adapter: ${SC26_PROBLEM_DIR}/gpu_direct_solver.cu")
    endif()
    if(NOT EXISTS "${SC26_PROBLEM_DIR}/gpu_iterative_solver.cu")
        message(FATAL_ERROR "Missing MPI GPU iterative adapter: ${SC26_PROBLEM_DIR}/gpu_iterative_solver.cu")
    endif()
endif()

add_subdirectory("${SC26_PROBLEM_DIR}" "selected_problem")

add_library(sc26_solver_config INTERFACE)
target_include_directories(sc26_solver_config INTERFACE "${SC26_PROBLEM_DIR}")

if(SC26_BUILD_MPI)
    find_package(MPI REQUIRED COMPONENTS C)
endif()

add_library(sc26_io STATIC
    src/matrix_market/highlevel.c
    src/matrix_market/mmio.c
    src/petsc_binary_reader.c
    src/system_loader.c
    src/csr_sort.c
)
target_link_libraries(sc26_io PUBLIC sc26_headers m)

add_library(sc26_diagnostics STATIC src/diagnostics.c)
target_link_libraries(sc26_diagnostics PUBLIC sc26_headers m)

sc26_configure_float128(sc26_diagnostics)

add_executable(direct_solver apps/direct_solver.c "${SC26_PROBLEM_DIR}/solver.c")
target_link_libraries(direct_solver PRIVATE sc26_io sc26_diagnostics sc26_solver_config
    sc26_problem_dependencies)
target_compile_definitions(direct_solver PRIVATE DIRECT_SOLVER)

add_executable(iterative_solver apps/iterative_solver.c "${SC26_PROBLEM_DIR}/solver.c")
target_link_libraries(iterative_solver PRIVATE sc26_io sc26_diagnostics sc26_solver_config
    sc26_problem_dependencies)
target_compile_definitions(iterative_solver PRIVATE ITERATIVE_SOLVER)

add_executable(iterative_solver_multi
    apps/iterative_solver_multi.c "${SC26_PROBLEM_DIR}/solver.c")
target_link_libraries(iterative_solver_multi PRIVATE
    sc26_io sc26_diagnostics sc26_solver_config sc26_problem_dependencies)
target_compile_definitions(iterative_solver_multi PRIVATE ITERATIVE_SOLVER)

add_executable(direct_solver_multi
    apps/direct_solver_multi.c "${SC26_PROBLEM_DIR}/solver.c")
target_link_libraries(direct_solver_multi PRIVATE
    sc26_io sc26_diagnostics sc26_solver_config sc26_problem_dependencies)
target_compile_definitions(direct_solver_multi PRIVATE DIRECT_SOLVER)

if(SC26_BUILD_MPI)
    add_library(sc26_mpi_io STATIC src/mpi_system_loader.c)
    target_link_libraries(sc26_mpi_io PUBLIC sc26_io MPI::MPI_C)
    target_compile_definitions(sc26_mpi_io PRIVATE MPI_USE)

    add_library(sc26_mpi_diagnostics STATIC src/diagnostics.c)
    target_link_libraries(sc26_mpi_diagnostics PUBLIC sc26_headers m MPI::MPI_C)
    target_compile_definitions(sc26_mpi_diagnostics PRIVATE MPI_USE)
    sc26_configure_float128(sc26_mpi_diagnostics)

    add_executable(MPI_direct_solver apps/MPI_direct_solver.c "${SC26_PROBLEM_DIR}/solver.c")
    target_link_libraries(MPI_direct_solver PRIVATE sc26_mpi_io sc26_mpi_diagnostics
        sc26_solver_config sc26_problem_dependencies)
    target_compile_definitions(MPI_direct_solver PRIVATE DIRECT_SOLVER MPI_USE)

    add_executable(MPI_iterative_solver apps/MPI_iterative_solver.c "${SC26_PROBLEM_DIR}/solver.c")
    target_link_libraries(MPI_iterative_solver PRIVATE sc26_mpi_io sc26_mpi_diagnostics
        sc26_solver_config sc26_problem_dependencies)
    target_compile_definitions(MPI_iterative_solver PRIVATE ITERATIVE_SOLVER MPI_USE)

    if(SC26_BUILD_MPI_GPU)
        add_library(sc26_problem_gpu_direct STATIC
            "${SC26_PROBLEM_DIR}/gpu_direct_solver.cu")
        target_include_directories(sc26_problem_gpu_direct PRIVATE
            "${PROJECT_SOURCE_DIR}/include")
        target_compile_definitions(sc26_problem_gpu_direct PRIVATE MPI_USE)
        target_link_libraries(sc26_problem_gpu_direct PRIVATE
            sc26_headers sc26_problem_dependencies MPI::MPI_C CUDA::cudart)

        add_library(sc26_problem_gpu_iterative STATIC
            "${SC26_PROBLEM_DIR}/gpu_iterative_solver.cu")
        target_include_directories(sc26_problem_gpu_iterative PRIVATE
            "${PROJECT_SOURCE_DIR}/include")
        target_compile_definitions(sc26_problem_gpu_iterative PRIVATE MPI_USE)
        target_link_libraries(sc26_problem_gpu_iterative PRIVATE
            sc26_headers sc26_problem_dependencies MPI::MPI_C CUDA::cudart)

        add_executable(MPI_GPU_direct_solver apps/MPI_GPU_direct_solver.c)
        target_link_libraries(MPI_GPU_direct_solver PRIVATE
            sc26_mpi_io sc26_mpi_diagnostics sc26_problem_gpu_direct
            sc26_problem_dependencies CUDA::cudart)
        target_compile_definitions(MPI_GPU_direct_solver PRIVATE MPI_USE)

        add_executable(MPI_GPU_iterative_solver apps/MPI_GPU_iterative_solver.c)
        target_link_libraries(MPI_GPU_iterative_solver PRIVATE
            sc26_mpi_io sc26_mpi_diagnostics sc26_problem_gpu_iterative
            sc26_problem_dependencies CUDA::cudart)
        target_compile_definitions(MPI_GPU_iterative_solver PRIVATE MPI_USE)
    endif()

    if(BUILD_TESTING)
        add_test(
            NAME header_include_order_mpi_first
            COMMAND "${MPI_C_COMPILER}" "-I${PROJECT_SOURCE_DIR}/include"
                    "-I${PROJECT_SOURCE_DIR}" -std=gnu99 -Werror
                    -Wno-unused-function -fsyntax-only
                    "${PROJECT_SOURCE_DIR}/tests/header_include_order_mpi_first.c")
        add_test(
            NAME header_include_order_reader_first
            COMMAND "${MPI_C_COMPILER}" "-I${PROJECT_SOURCE_DIR}/include"
                    "-I${PROJECT_SOURCE_DIR}" -std=gnu99 -Werror
                    -Wno-unused-function -fsyntax-only
                    "${PROJECT_SOURCE_DIR}/tests/header_include_order_reader_first.c")
    endif()

    if(BUILD_TESTING AND SC26_TEST_MATRIX AND SC26_TEST_RHS)
        add_test(
            NAME mpi_smoke
            COMMAND "${MPIEXEC_EXECUTABLE}" ${MPIEXEC_PREFLAGS}
                    ${MPIEXEC_NUMPROC_FLAG} 2
                    "$<TARGET_FILE:MPI_iterative_solver>"
                    "${SC26_TEST_MATRIX}" "${SC26_TEST_RHS}"
                    ${MPIEXEC_POSTFLAGS})
    endif()
endif()

if(BUILD_TESTING)
    add_executable(test_csr_sort tests/csr_sort.c src/csr_sort.c)
    target_link_libraries(test_csr_sort PRIVATE sc26_headers)
    target_compile_options(test_csr_sort PRIVATE -UNDEBUG)
    add_test(NAME csr_sort COMMAND test_csr_sort)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME round_isolation
            COMMAND "${Python3_EXECUTABLE}"
                    "${PROJECT_SOURCE_DIR}/tests/test_round_isolation.py")
    endif()
endif()
