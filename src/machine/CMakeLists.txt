# Device abstractions (Machine, Head, Nozzle, Feeder, Camera, Driver) and the
# Simulation implementation. Must not depend on QML.
# INTERFACE until the first sources land; then switch to STATIC.
add_library(smtpro_machine INTERFACE)
target_include_directories(smtpro_machine INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smtpro_machine INTERFACE smtpro_core)
