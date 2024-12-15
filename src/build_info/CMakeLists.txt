configure_file(
    ${PROJECT_SOURCE_DIR}/src/build_info/build_info.cpp.in
    ${CMAKE_BINARY_DIR}/src/build_info/build_info.cpp
)

add_library(build_info STATIC
    ${CMAKE_BINARY_DIR}/src/build_info/build_info.cpp
)

target_include_directories(build_info
PUBLIC
    .
)

swig_lib(
    NAME      build_info_swig
    NAMESPACE eden
    I_FILE    build_info.i
)

file(GLOB cpps
    *.cpp
)

target_sources(build_info_swig
PRIVATE
    ${cpps}
)

target_link_libraries(build_info_swig
PRIVATE
    OpenSTA
    build_info
)

target_include_directories(build_info_swig
PRIVATE
    ./
    ../include
    ${OPENROAD_HOME}/include
)
