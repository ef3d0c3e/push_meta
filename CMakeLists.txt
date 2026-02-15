# This is used to generate the `compile_commands.json` file required by ccls/clangd
# If bear is installed, run `make fclean && bear -- make` instead.
CMAKE_MINIMUM_REQUIRED(VERSION 3.22)
PROJECT(push_meta LANGUAGES C)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
SET(CMAKE_C_FLAGS "-Wall -Wextra -Wconversion -pedantic -std=c23 -ggdb")

FILE(GLOB_RECURSE SOURCES
	"src/*.c")

INCLUDE_DIRECTORIES(src)
ADD_EXECUTABLE(${CMAKE_PROJECT_NAME} ${SOURCES})
