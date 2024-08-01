file(GLOB cpps *.cpp)
add_executable(main ${cpps})
target_link_libraries(main 
PUBLIC
    linden
) 