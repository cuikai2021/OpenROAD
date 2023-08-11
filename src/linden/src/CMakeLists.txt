include("openroad")

file(GLOB tcls *.tcl itdp/*.tcl)
swig_lib(NAME      linden
         NAMESPACE linden
         I_FILE    linden.i
         SCRIPTS   ${tcls}
)

file(GLOB cpps *.cpp itdp/*.cpp)
target_sources(linden
PRIVATE
    ${cpps}
)

target_include_directories(linden
PUBLIC
    ../include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
PUBLIC
    rsz_pb
    gpl
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
