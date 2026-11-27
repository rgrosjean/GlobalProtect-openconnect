// Tests for the connection presenter: for each ConnectionState, what the window
// shows (status text, shield, main button, visible view, Tunnel timer).

#include <QtCore/QDir>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtTest/QTest>

#include "connectionview.h"

Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(Shield)
Q_DECLARE_METATYPE(VisibleView)

class TestConnectionView : public QObject
{
    Q_OBJECT

private slots:
    void showsState_data()
    {
        QTest::addColumn<ConnectionState>("state");
        QTest::addColumn<QString>("statusText");
        QTest::addColumn<Shield>("shield");
        QTest::addColumn<QString>("buttonText");
        QTest::addColumn<bool>("buttonEnabled");
        QTest::addColumn<VisibleView>("view");

        QTest::newRow("Disconnected") << ConnectionState::Disconnected
            << "Not Connected" << Shield::NotConnected << "Connect" << true << VisibleView::Disconnected;
        QTest::newRow("Authenticating") << ConnectionState::Authenticating
            << "Authenticating..." << Shield::InProgress << "Authenticating..." << false << VisibleView::Disconnected;
        QTest::newRow("Connecting") << ConnectionState::Connecting
            << "Connecting..." << Shield::InProgress << "Connecting..." << false << VisibleView::Disconnected;
        QTest::newRow("Connected") << ConnectionState::Connected
            << "Connected" << Shield::Connected << "Disconnect" << true << VisibleView::Connected;
        QTest::newRow("Disconnecting") << ConnectionState::Disconnecting
            << "Disconnecting..." << Shield::InProgress << "Disconnecting..." << false << VisibleView::Connected;
        QTest::newRow("SwitchingGateway") << ConnectionState::SwitchingGateway
            << "Switching Gateway..." << Shield::InProgress << "Switching Gateway..." << false << VisibleView::Connected;
    }

    void showsState()
    {
        QFETCH(ConnectionState, state);
        QFETCH(QString, statusText);
        QFETCH(Shield, shield);
        QFETCH(QString, buttonText);
        QFETCH(bool, buttonEnabled);
        QFETCH(VisibleView, view);

        const ConnectionView v = presentConnection(state, 0);

        QCOMPARE(v.statusText, statusText);
        QCOMPARE(v.shield, shield);
        QCOMPARE(v.buttonText, buttonText);
        QCOMPARE(v.buttonEnabled, buttonEnabled);
        QCOMPARE(v.visibleView, view);
    }

    void formatsActiveFor_data()
    {
        QTest::addColumn<qint64>("seconds");
        QTest::addColumn<QString>("activeFor");

        QTest::newRow("zero") << qint64(0) << "Active for 00:00:00";
        QTest::newRow("under a minute") << qint64(42) << "Active for 00:00:42";
        QTest::newRow("over an hour") << qint64(3723) << "Active for 01:02:03";
        QTest::newRow("over 24 hours") << qint64(90061) << "Active for 25:01:01";
    }

    void formatsActiveFor()
    {
        QFETCH(qint64, seconds);
        QFETCH(QString, activeFor);

        QCOMPARE(presentConnection(ConnectionState::Connected, seconds).activeForText, activeFor);
    }

    void noActiveForUnlessConnected()
    {
        QVERIFY(presentConnection(ConnectionState::Disconnected, 42).activeForText.isEmpty());
        QVERIFY(presentConnection(ConnectionState::Disconnecting, 42).activeForText.isEmpty());
    }

    void mapsShieldToResource_data()
    {
        QTest::addColumn<Shield>("shield");
        QTest::addColumn<QString>("resource");

        QTest::newRow("not connected") << Shield::NotConnected << ":/images/shield_not_connected.svg";
        QTest::newRow("in progress") << Shield::InProgress << ":/images/shield_in_progress.svg";
        QTest::newRow("connected") << Shield::Connected << ":/images/shield_connected.svg";
    }

    void mapsShieldToResource()
    {
        QFETCH(Shield, shield);
        QFETCH(QString, resource);

        QCOMPARE(shieldResource(shield), resource);
    }

    // The resource must exist and render, and be the right colour: grey, amber, green.
    void shieldRendersInItsColour_data()
    {
        QTest::addColumn<Shield>("shield");
        QTest::addColumn<int>("minHue");
        QTest::addColumn<int>("maxHue");
        QTest::addColumn<bool>("grey");

        QTest::newRow("not connected") << Shield::NotConnected << 0 << 359 << true;
        QTest::newRow("in progress") << Shield::InProgress << 25 << 55 << false;
        QTest::newRow("connected") << Shield::Connected << 100 << 160 << false;
    }

    void shieldRendersInItsColour()
    {
        QFETCH(Shield, shield);
        QFETCH(int, minHue);
        QFETCH(int, maxHue);
        QFETCH(bool, grey);

        QImageReader reader(shieldResource(shield));
        QVERIFY2(reader.canRead(), qPrintable(reader.errorString()));
        reader.setScaledSize(QSize(128, 128));
        const QImage image = reader.read();
        QVERIFY(!image.isNull());

        // Count the opaque pixels that are the shield's body colour (most saturated or,
        // for grey, least saturated); the symbol inside is white and is skipped.
        int body = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor c = image.pixelColor(x, y);
                if (c.alpha() < 250 || c.lightness() > 230) {
                    continue;
                }
                if (grey ? c.saturation() < 40 : (c.saturation() > 120 && c.hue() >= minHue && c.hue() <= maxHue)) {
                    ++body;
                }
            }
        }
        QVERIFY2(body > 128 * 128 / 4, qPrintable(QString("only %1 body pixels").arg(body)));
    }
};

QTEST_GUILESS_MAIN(TestConnectionView)
#include "tst_connectionview.moc"
