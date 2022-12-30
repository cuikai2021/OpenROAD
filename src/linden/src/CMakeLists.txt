include("openroad")

add_library(linden)

target_sources(linden
  PRIVATE
    lindendp.cc
    lefdef_dumper.cc
)

set (LINDEN_LIBS_DIR "../gpl/lib")

message(STATUS "LINDEN_LIBS_DIR is set to " ${LINDEN_LIBS_DIR})

find_library(LINDENGP
    lindengp
    HINTS
      ${LINDEN_LIBS_DIR}
    NO_DEFAULT_PATH
)

message(STATUS "LINDENGP is set to " ${LINDENGP})

find_library(BABYDP
    baby_dp
    HINTS
      ${LINDEN_LIBS_DIR}
    NO_DEFAULT_PATH
)

find_library(BABYDP_PUBLIC
    baby_dp_public
    HINTS
      ${LINDEN_LIBS_DIR}
    NO_DEFAULT_PATH
)

find_library(FINUFFT
    finufft
    HINTS
      ${LINDEN_LIBS_DIR}
    NO_DEFAULT_PATH
)

find_library(LEGENDRE_RULE_FAST
    legendre_rule_fast
    HINTS
      ${LINDEN_LIBS_DIR}
    NO_DEFAULT_PATH
)

target_include_directories(linden
  PUBLIC
    ../include
    ../gpl/include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
  PRIVATE
    odb
    utl
    ${LINDENGP}
    ${BABYDP_PUBLIC}
    ${BABYDP}
    rsz
    tomlpp
    fassert
    prettyprint
    ${LEMON_LIBRARY}
    ${FINUFFT} ${LEGENDRE_RULE_FAST} fftw3 fftw3_omp fftw3f fftw3f_omp
)
