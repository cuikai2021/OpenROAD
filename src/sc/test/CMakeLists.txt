add_executable(sc_unittest
        SpanningGraphTest.cpp
        CompleteGraphStrategy.cpp
        )

target_include_directories(sc_unittest PUBLIC ../src)

target_link_libraries(sc_unittest
        sc
        testa
        )
