include("openroad")

swig_lib(NAME      cgr
         NAMESPACE cgr
         I_FILE    src/cgr.i
         SCRIPTS   src/cgr.tcl
)

target_sources(cgr
  PRIVATE
    src/MakeCongestionRepair.cpp
    src/congestionRepair.cpp
)

messages(TARGET cgr)

target_include_directories(cgr
  PUBLIC
    include
)

target_link_libraries(cgr
  PRIVATE
    gpl
    odb
    Banyan
    linden
    fassert
)
