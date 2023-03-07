include("openroad")

add_library(linden)

target_sources(linden
PRIVATE
    lindendp.cc
    lefdef_dumper.cc
)

target_include_directories(linden
PUBLIC
    ../include
    ../gpl/include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
PUBLIC
    odb
    utl
    linden_ext
    fassert
    prettyprint
    rsz
    tomlpp
)
