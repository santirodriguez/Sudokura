cmake_minimum_required(VERSION 3.16)
project(Sudokura C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_library(sudokura_core STATIC src/core/game.c)
target_include_directories(sudokura_core PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(sudokura_platform STATIC src/platform/fonts.c)
target_link_libraries(sudokura_platform PRIVATE SDL2_ttf::SDL2_ttf)
target_include_directories(sudokura_platform PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_library(sudokura_ui STATIC src/ui/render.c)
target_include_directories(sudokura_ui PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(sudokura_ui PRIVATE sudokura_core sudokura_platform SDL2::SDL2 SDL2_ttf::SDL2_ttf m)

add_executable(sudokura src/main.c)
target_link_libraries(sudokura PRIVATE sudokura_ui sudokura_core sudokura_platform SDL2::SDL2 SDL2_ttf::SDL2_ttf m)

if(TARGET SDL2::SDL2main)
  target_link_libraries(sudokura PRIVATE SDL2::SDL2main)
endif

add_executable(test_game tests/test_game.c)
target_link_libraries(test_game PRIVATE sudokura_core)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME test_game COMMAND test_game)
endif
