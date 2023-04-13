include("openroad")

add_library(linden)

target_sources(linden
PRIVATE
    envflags.cpp
    lindendp.cc
    lefdef_dumper.cc
    scanchain.cpp
)

target_include_directories(linden
PUBLIC
    ../include
    ../gpl/include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
PUBLIC
    rsz
    odb
    utl
    linden_ext
    envflag
    fassert
    prettyprint
    tomlpp
    linden_pb
)
