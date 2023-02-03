add_executable(banyan_unittest
        MergeWireSegment_test.cpp
        )

target_include_directories(banyan_unittest PUBLIC ../src)

target_link_libraries(banyan_unittest
        Banyan
        testa
        )
