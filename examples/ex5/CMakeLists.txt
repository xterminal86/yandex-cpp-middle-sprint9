cmake_minimum_required(VERSION 3.30)
project(ex5 VERSION 1.0.0 LANGUAGES CXX)

cmake_policy(SET CMP0169 OLD)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wfatal-errors)
add_compile_options(-Werror=return-type)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Не больше одной ошибки за раз
    add_compile_options(-fmax-errors=1)
endif()

include(FetchContent)
FetchContent_Declare(
  CPM
  GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake
  GIT_TAG origin/master
)
FetchContent_MakeAvailable(CPM)

CPMAddPackage(
    NAME stdexec
    GITHUB_REPOSITORY NVIDIA/stdexec
    GIT_TAG main # This will always pull the latest code from the `main` branch. You may also use a specific release version or tag
    OPTIONS
        "STDEXEC_BUILD_EXAMPLES OFF"
        "STDEXEC_BUILD_TESTS OFF"
)

# Создаём исполняемый таргет и линкуем к нему статическую библиотеку
add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/ex5.cpp")

# Добавляем в проект используемые сторонние PRIVATE
target_include_directories(
  ${PROJECT_NAME} PUBLIC
  ${stdexec_SOURCE_DIR}/include
)

target_link_libraries(
  ${PROJECT_NAME} PUBLIC
  STDEXEC::stdexec
)

