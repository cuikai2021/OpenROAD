include("openroad")

file(GLOB tcls *.tcl itdp/*.tcl)
swig_lib(NAME      linden
         NAMESPACE linden
         I_FILE    linden.i
         SCRIPTS   dp.tcl
)

file(GLOB cpps *.cpp util/*.cpp itdp/*.cpp gp/*.cpp mp/*.cpp)
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
    gpl
    cgr
    Banyan
    sc
    rsz
    odb
    utl
    linden_ext
    envflag
    fassert
    prettyprint
    tomlpp
    linden_pb
    linden_proto
)
