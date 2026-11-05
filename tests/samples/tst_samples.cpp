#include "io/programreader.h"

#include <QDir>
#include <QtTest>

// Every file in samples/ must load with the current ProgramReader, without
// errors or warnings.
class TstSamples : public QObject
{
    Q_OBJECT

private slots:
    void loads_data();
    void loads();
};

void TstSamples::loads_data()
{
    QTest::addColumn<QString>("path");

    const QDir dir(QStringLiteral(SMTPRO_SAMPLES_DIR));
    const QStringList files = dir.entryList({QStringLiteral("*.smt.xml")}, QDir::Files);
    QVERIFY2(!files.isEmpty(), qPrintable(QStringLiteral("no samples in ") + dir.path()));
    for (const QString &name : files)
        QTest::newRow(qPrintable(name)) << dir.filePath(name);
}

void TstSamples::loads()
{
    QFETCH(QString, path);

    const ReadResult r = ProgramReader::readFile(path);
    QStringList messages;
    for (const Diagnostic &d : r.diagnostics)
        messages << QStringLiteral("line %1: %2").arg(d.line).arg(d.message);
    QVERIFY2(r.diagnostics.isEmpty(), qPrintable(messages.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(TstSamples)
#include "tst_samples.moc"
