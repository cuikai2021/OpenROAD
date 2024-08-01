include("openroad")
find_package(Eigen3 REQUIRED)

file(GLOB tcls
    *.tcl
    cgr/*.tcl
    dp/*.tcl
    gp/*.tcl
    itdp/*.tcl
    mp/*.tcl
    remove_fillers/*.tcl
    timing_data/*.tcl
)
swig_lib(NAME      linden
         NAMESPACE linden
         I_FILE    linden.i
         SCRIPTS   ${tcls}
)

file(GLOB cpps
    *.cpp
    cgr/*.cpp
    dp/*.cpp
    gp/*.cpp
    initial_place/*.cpp
    initial_place/ev/*.cpp
    itdp/*.cpp
    mp/*.cpp
    remove_fillers/*.cpp
    timing_data/*.cpp
    util/*.cpp
    eco_placer/*.cpp
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
PRIVATE
    ${OCTAVE_INCLUDE_DIR}
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
    Eigen3::Eigen
    ${OCTAVE_LIBRARY}
    envflag
    fassert
    prettyprint
    tomlpp
    linden_pb
    linden_proto
)

