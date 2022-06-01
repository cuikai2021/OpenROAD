find_file(OCTAVE_HEADER octave/octave.h
    PATHS
        /usr/include/octave-5.2.0/ # distributed by Ubuntu 20.04
        /usr/include/octave-6.4.0/ # distributed by Ubuntu 22.04
)
string(REGEX MATCH "[0-9]+[.][0-9]+([.][0-9]+)?" OCTAVE_VERSION ${OCTAVE_HEADER})
message(STATUS "Octave version: ${OCTAVE_VERSION}")

set(OCTAVE_INCLUDE_DIR
    /usr/include/octave-${OCTAVE_VERSION}/
)
message(STATUS "Octave include dir: ${OCTAVE_INCLUDE_DIR}")

find_library(OCTINTERP_LIB
    octinterp
    PATHS
        /usr/lib/x86_64-linux-gnu/octave/${OCTAVE_VERSION}
        /usr/lib/x86_64-linux-gnu/
)
find_library(OCTAVE_LIB
    octave
    PATHS
        /usr/lib/x86_64-linux-gnu/octave/${OCTAVE_VERSION}
        /usr/lib/x86_64-linux-gnu/
)
set(OCTAVE_LIBRARY ${OCTAVE_LIB} ${OCTINTERP_LIB})
message(STATUS "Octave library: ${OCTAVE_LIBRARY}")


