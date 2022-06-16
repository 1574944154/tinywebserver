cmake_minimum_required(VERSION 3.0.0)
project(webserver VERSION 0.1.0)

include(CTest)
enable_testing()


add_subdirectory(src)
list(APPEND EXTRA_LIBS src)
list(APPEND EXTRA_INCLUDES "${PROJECT_SOURCE_DIR}/src")

add_executable(webserver main.cpp)
add_executable(client client.cpp)

target_link_libraries(webserver PUBLIC ${EXTRA_LIBS})
target_link_libraries(webserver PUBLIC pthread)


set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
target_include_directories(webserver PUBLIC
                            ${PROJECT_BINARY_DIR}
                            ${EXTRA_INCLUDES}
)
