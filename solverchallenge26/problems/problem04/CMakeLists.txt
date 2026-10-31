# Problem 04-specific dependencies.
#
# Add only the libraries needed by this problem here. For example:
#
# find_package(SomeSolver CONFIG REQUIRED)
# target_link_libraries(sc26_problem_dependencies
#     INTERFACE SomeSolver::SomeSolver
# )
#
# Include directories and compile definitions can also be attached to this
# target with target_include_directories and target_compile_definitions.
add_library(sc26_problem_dependencies INTERFACE)
