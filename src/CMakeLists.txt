# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(app_sources
	${CMAKE_SOURCE_DIR}/src/main.cpp
)

idf_component_register(
	SRCS ${app_sources}
	INCLUDE_DIRS
		"."
)
