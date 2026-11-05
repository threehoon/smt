# smtpro_add_test(<name> SOURCES <files...> LIBS <targets...>)
function(smtpro_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt5::Test smtpro_options ${ARG_LIBS})
    target_compile_definitions(${name} PRIVATE
        SMTPRO_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")
    add_test(NAME ${name} COMMAND ${name})
    # Qt DLLs must be found when ctest launches the test outside dev-env.bat.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.22)
        set_tests_properties(${name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${Qt5_DIR}/../../../bin")
    endif()
endfunction()

add_subdirectory(core)
add_subdirectory(samples)

add_subdirectory(app)
