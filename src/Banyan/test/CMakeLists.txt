add_executable(banyan_unittest
        MergeWireSegment_test.cpp
        M1GridlessMaze_test.cpp
        )

target_include_directories(banyan_unittest PUBLIC
../src
../include
)

target_link_libraries(banyan_unittest
        Banyan
        testa
        )
