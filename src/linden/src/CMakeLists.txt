include("openroad")

add_library(linden)

target_sources(linden
  PRIVATE
    lindendp.cc
)

target_include_directories(linden
  PUBLIC
    ../include
    ${OPENROAD_HOME}/include
)

target_link_libraries(linden
  PRIVATE
    odb
    utl
)
