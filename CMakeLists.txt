cmake_minimum_required (VERSION 3.10)
set(CMAKE_CXX_STANDARD 17)

project(tessa)
set(EXECUTABLE_OUTPUT_PATH "../bin")

add_executable(tessa src/main.cpp src/logging.cpp)

if(MSVC)
  target_compile_options(tessa PRIVATE /W4)
else()
  target_compile_options(tessa PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

find_package(CGAL CONFIG REQUIRED)
target_link_libraries(tessa PRIVATE CGAL::CGAL)

find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(tessa PRIVATE CLI11::CLI11)

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(tessa PRIVATE spdlog::spdlog spdlog::spdlog_header_only)