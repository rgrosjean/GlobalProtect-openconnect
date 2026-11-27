#include <QtTest/QtTest>

#include "aboutinfo.h"

class TestAboutInfo : public QObject
{
    Q_OBJECT

private slots:
    void showsNameAndVersion()
    {
        const QString html = aboutHtml("1.5.5");
        QVERIFY(html.contains("GlobalProtect"));
        QVERIFY(html.contains("Version 1.5.5"));
    }

    void creditsBothUpstreamProjects()
    {
        const QString html = aboutHtml("1.5.5");
        QVERIFY(html.contains("kakwa/GlobalProtect-openconnect"));
        QVERIFY(html.contains("yuezk/GlobalProtect-openconnect"));
        QVERIFY(html.contains("1.x"));
    }

    void statesLicence()
    {
        QVERIFY(aboutHtml("1.5.5").contains("GPL-3.0"));
    }

    void linksToForkRepository()
    {
        QCOMPARE(aboutRepositoryUrl(), QString("https://github.com/rgrosjean/GlobalProtect-openconnect"));
        QVERIFY(aboutHtml("1.5.5").contains("<a href=\"https://github.com/rgrosjean/GlobalProtect-openconnect\">"));
    }
};

QTEST_APPLESS_MAIN(TestAboutInfo)
#include "tst_aboutinfo.moc"
