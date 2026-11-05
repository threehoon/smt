# Install layout for the installer: everything flat next to SwiftPlace.exe.
#
#   cmake --install build/release --prefix build/release/stage
#
# installs the exe, runs windeployqt on it (Qt DLLs, plugins, QML modules),
# adds the MSVC runtime DLLs and the sample files. installer.iss packs that
# folder; package.bat runs the whole sequence.

# MSVC runtime (msvcp140.dll, vcruntime140*.dll) next to the exe, so the
# target PC needs no VC++ redistributable. The UCRT is part of Windows 10+.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
include(InstallRequiredSystemLibraries)

get_target_property(SMTPRO_QMAKE Qt5::qmake IMPORTED_LOCATION)
get_filename_component(SMTPRO_QT_BIN "${SMTPRO_QMAKE}" DIRECTORY)
find_program(SMTPRO_WINDEPLOYQT windeployqt HINTS "${SMTPRO_QT_BIN}" NO_DEFAULT_PATH REQUIRED)

# Qt 5.14's windeployqt has no --qmake option; Qt 5.14's bin goes first on PATH
# so it cannot pick up the Qt 6 tools that are also on this machine's PATH.
# opengl32sw.dll and the ANGLE DLLs stay in: they let Qt Quick start on
# machines without a working OpenGL driver (common on shop-floor PCs).
install(CODE "
    set(ENV{PATH} \"${SMTPRO_QT_BIN};\$ENV{PATH}\")
    set(exe \"\${CMAKE_INSTALL_PREFIX}/${SMTPRO_EXE_NAME}.exe\")
    message(STATUS \"windeployqt: \${exe}\")
    execute_process(
        COMMAND \"${SMTPRO_WINDEPLOYQT}\"
            $<IF:$<CONFIG:Debug>,--debug,--release>
            --qmldir \"${PROJECT_SOURCE_DIR}/src/qml\"
            --no-translations
            --no-compiler-runtime
            \"\${exe}\"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR \"windeployqt failed: \${result}\")
    endif()
")

install(FILES ${PROJECT_SOURCE_DIR}/samples/example.smt.xml DESTINATION samples)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/samples/cad/ DESTINATION samples/cad)

# Version and paths for installer.iss, which includes this file.
configure_file(installer-config.iss.in ${CMAKE_BINARY_DIR}/installer-config.iss @ONLY)
