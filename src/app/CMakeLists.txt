# ViewModels and list models exposed to QML. QtCore only, so tests can link
# them without a GUI; the executable adds the QML engine.
#
#   document/    ProgramDocument: the open program file, open / save
#   listmodels/  QAbstractListModel per table, read from ProgramDocument
#   display/     user-visible texts for model values
#   cadimport/   the "import placements" dialog: CadImporter and its preview models
#
# Includes are written from this directory, e.g. "listmodels/boardlistmodel.h".
add_library(smtpro_app STATIC
    document/programdocument.h
    document/programdocument.cpp

    listmodels/programlistmodel.h
    listmodels/programlistmodel.cpp
    listmodels/boardcontentmodel.h
    listmodels/boardcontentmodel.cpp
    listmodels/packagelistmodel.h
    listmodels/packagelistmodel.cpp
    listmodels/componentlistmodel.h
    listmodels/componentlistmodel.cpp
    listmodels/boardlistmodel.h
    listmodels/boardlistmodel.cpp
    listmodels/placementlistmodel.h
    listmodels/placementlistmodel.cpp
    listmodels/fiduciallistmodel.h
    listmodels/fiduciallistmodel.cpp
    listmodels/badmarklistmodel.h
    listmodels/badmarklistmodel.cpp
    listmodels/boardinstancelistmodel.h
    listmodels/boardinstancelistmodel.cpp
    listmodels/jobplacementlistmodel.h
    listmodels/jobplacementlistmodel.cpp
    listmodels/feederlistmodel.h
    listmodels/feederlistmodel.cpp

    display/displaytext.h
    display/displaytext.cpp

    cadimport/cadimporter.h
    cadimport/cadimporter.cpp
    cadimport/cadpreviewmodel.h
    cadimport/cadpreviewmodel.cpp
    cadimport/cadrowlistmodel.h
    cadimport/cadrowlistmodel.cpp
    cadimport/cadpartlistmodel.h
    cadimport/cadpartlistmodel.cpp
)
target_include_directories(smtpro_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smtpro_app PUBLIC smtpro_machine Qt5::Core)

# Icon and version resource, filled in from the project version and names.
set(SMTPRO_ICON_FILE "${PROJECT_SOURCE_DIR}/packaging/icon/swiftplace.ico")
configure_file(${PROJECT_SOURCE_DIR}/packaging/swiftplace.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/swiftplace.rc @ONLY)

# WIN32: a GUI program, no console window. qDebug output still reaches the debugger.
add_executable(smtpro WIN32
    main.cpp
    ${PROJECT_SOURCE_DIR}/src/qml/qml.qrc
    ${CMAKE_CURRENT_BINARY_DIR}/swiftplace.rc
)
set_target_properties(smtpro PROPERTIES OUTPUT_NAME ${SMTPRO_EXE_NAME})
target_compile_definitions(smtpro PRIVATE
    SMTPRO_VERSION="${PROJECT_VERSION}"
    SMTPRO_EXE_NAME="${SMTPRO_EXE_NAME}")
target_link_libraries(smtpro PRIVATE smtpro_app Qt5::Gui Qt5::Qml Qt5::Quick)

install(TARGETS smtpro RUNTIME DESTINATION .)
