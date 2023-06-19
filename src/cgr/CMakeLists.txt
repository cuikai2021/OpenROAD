include("openroad")

swig_lib(NAME      cgr
         NAMESPACE cgr
         I_FILE    src/cgr.i
         SCRIPTS   src/cgr.tcl src/analyze_banyan_cmap_file.tcl
)

target_sources(cgr
  PRIVATE
    src/MakeCongestionRepair.cpp
    src/congestionRepair.cpp
    src/analyze_banyan_cmap_file.cpp
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
