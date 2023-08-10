include("openroad")

swig_lib(NAME      linden
         NAMESPACE linden
         I_FILE    linden.i
         SCRIPTS   linden.tcl
)

target_sources(linden
PRIVATE
    make_linden.cpp
    envflags.cpp
    lindendp.cc
    lefdef_dumper.cc
    scanchain.cpp
    ignored_nets.cpp
    relevance_verify.cpp
    itdp/linden_incr_tdp.cpp
    itdp/linden_lpbased_postdrv_itdp.cpp
    itdp/linden_lpbased_postdrv_itdp.db.cpp
)

target_include_directories(linden
PUBLIC
    ../include
    ../gpl/include
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
