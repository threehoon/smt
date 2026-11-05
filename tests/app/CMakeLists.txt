smtpro_add_test(tst_programdocument SOURCES tst_programdocument.cpp LIBS smtpro_app)
smtpro_add_test(tst_mainwindow
    SOURCES tst_mainwindow.cpp ${PROJECT_SOURCE_DIR}/src/qml/qml.qrc
    LIBS smtpro_app Qt5::Gui Qt5::Qml Qt5::Quick)
smtpro_add_test(tst_programedit SOURCES tst_programedit.cpp LIBS smtpro_app)
smtpro_add_test(tst_cadimporter SOURCES tst_cadimporter.cpp LIBS smtpro_app)

# Timings on a large program; no pass / fail, so not a ctest test.
# Configure with -DSMTPRO_BENCHMARKS=ON (preferably Release) and run it by hand.
option(SMTPRO_BENCHMARKS "Build bench_largeprogram" OFF)
if(SMTPRO_BENCHMARKS)
    add_executable(bench_largeprogram bench_largeprogram.cpp)
    target_link_libraries(bench_largeprogram PRIVATE Qt5::Test smtpro_options smtpro_app)
    target_compile_definitions(bench_largeprogram PRIVATE
        SMTPRO_SAMPLES_DIR="${PROJECT_SOURCE_DIR}/samples")
endif()
