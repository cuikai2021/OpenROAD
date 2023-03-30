include("openroad")

swig_lib(NAME      sc
         NAMESPACE sc
         I_FILE    ScanChain.i
         SCRIPTS   ScanChain.tcl
)

target_sources(sc
  PRIVATE
    Graph.cpp
    ZhouSpanningGraphStrategy.cpp
    DisjointSet.cpp
    Kruskal.cpp
    ScanChain.cpp
    MakeScanChain.cpp
    ChristofidesSpanningGraph.cpp
)

target_include_directories(sc
  PUBLIC
    ../include
  PRIVATE
    .
)

target_link_libraries(sc
  PRIVATE
    odb
    OpenSTA
    utl
 )