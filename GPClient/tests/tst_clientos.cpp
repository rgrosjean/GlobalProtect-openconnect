// Tests for the Client OS choices of the Settings dialog: which options exist,
// and which one a saved value selects.

#include <QtTest/QTest>

#include "clientos.h"

class TestClientOs : public QObject
{
    Q_OBJECT

private slots:
    void offersLinuxWindowsAndMac()
    {
        QCOMPARE(clientOsOptions(), (QStringList{"Linux", "Windows", "Mac"}));
    }

    void defaultsToLinux()
    {
        QCOMPARE(defaultClientOs(), QString("Linux"));
    }

    void selectsSavedValue_data()
    {
        QTest::addColumn<QString>("saved");
        QTest::addColumn<QString>("selected");

        QTest::newRow("Linux") << "Linux" << "Linux";
        QTest::newRow("Windows") << "Windows" << "Windows";
        QTest::newRow("Mac") << "Mac" << "Mac";
        QTest::newRow("unknown") << "Solaris" << "Linux";
        QTest::newRow("empty") << "" << "Linux";
        QTest::newRow("other case") << "windows" << "Linux";
    }

    void selectsSavedValue()
    {
        QFETCH(QString, saved);
        QFETCH(QString, selected);

        QCOMPARE(clientOsOptions().at(clientOsIndex(saved)), selected);
    }
};

QTEST_APPLESS_MAIN(TestClientOs)
#include "tst_clientos.moc"
