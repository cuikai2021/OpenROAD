include("openroad")

file(GLOB tcls
    *.tcl
    itdp/*.tcl
    timing_data/*.tcl
    dp/*.tcl
    gp/*.tcl
    remove_fillers/*.tcl
)
swig_lib(NAME      linden
         NAMESPACE linden
         I_FILE    linden.i
         SCRIPTS   ${tcls}
)

file(GLOB cpps
    *.cpp
    util/*.cpp
    itdp/*.cpp
    gp/*.cpp
    mp/*.cpp
    cgr/*.cpp
    timing_data/*.cpp
    dp/*.cpp
    remove_fillers/*.cpp
)
target_sources(linden
PRIVATE
    ${cpps}
)

target_include_directories(linden
PUBLIC
    ./
    ../include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
PUBLIC
    gpl
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

