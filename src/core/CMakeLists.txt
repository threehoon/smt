# Domain model and program file I/O. Depends on QtCore only.
add_library(smtpro_core STATIC
    model/angle.h
    model/board.h
    model/boardinstance.h
    model/component.h
    model/feeder.h
    model/feedersetup.h
    model/fiducial.h
    model/package.h
    model/panel.h
    model/placement.h
    model/program.h
    io/formatversion.h
    io/programreader.h
    io/programreader.cpp
    io/programwriter.h
    io/programwriter.cpp
    io/rawxml.h
    io/rawxml.cpp
    io/xmlnames.h
    validation/diagnostic.h
    edit/programedits.h
    edit/programedits.cpp
    edit/commandstack.h
    edit/commandstack.cpp
    validation/programvalidator.h
    validation/programvalidator.cpp
    cadimport/cadcolumns.h
    cadimport/cadcolumns.cpp
    cadimport/cadtable.h
    cadimport/cadtable.cpp
    cadimport/cadplacements.h
    cadimport/cadplacements.cpp
    cadimport/cadimport.h
    cadimport/cadimport.cpp
)
target_include_directories(smtpro_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# smtpro_options stays PUBLIC: app and machine get our compile flags through core.
target_link_libraries(smtpro_core PUBLIC Qt5::Core smtpro_options)
