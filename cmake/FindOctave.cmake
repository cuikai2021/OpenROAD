# For ubuntu 21.10
# set(OCTAVE_VERSION 6.4.0)
# For ubuntu 20.04
# set(OCTAVE_VERSION 5.2.0)

set(OCTAVE_LIB_PATHS
    /usr/lib/x86_64-linux-gnu/octave/5.2.0 # distributed by Ubuntu 20.04
    /usr/lib/x86_64-linux-gnu/octave/6.4.0 # distributed by Ubuntu 22.04
)

find_library(OCTAVE_LIB
    octave
    PATHS ${OCTAVE_LIB_PATHS}
)

string(REGEX MATCH "[0-9]+[.][0-9]+([.][0-9]+)?" OCTAVE_VERSION ${OCTAVE_LIB})
message(STATUS "Octave version: ${OCTAVE_VERSION}")

find_library(OCTINTERP_LIB
    octinterp
    PATHS /usr/lib/x86_64-linux-gnu/octave/${OCTAVE_VERSION}
)

set(OCTAVE_LIBRARY
    ${OCTAVE_LIB}
    ${OCTINTERP_LIB}
)
message(STATUS "Octave library: ${OCTAVE_LIBRARY}")

set(OCTAVE_INCLUDE_DIR
    /usr/include/octave-${OCTAVE_VERSION}/
)
message(STATUS "Octave include dir: ${OCTAVE_INCLUDE_DIR}")

