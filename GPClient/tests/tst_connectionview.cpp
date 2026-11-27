// Tests for the connection presenter: for each ConnectionState, what the window
// shows (status text, shield, main button, visible view, Tunnel timer), and the
// Gateway dropdown of the connected view.

#include <QtCore/QDir>
#include <QtGui/QImage>
#include <QtGui/QImageReader>
#include <QtTest/QTest>

#include "connectionview.h"

Q_DECLARE_METATYPE(ConnectionState)
Q_DECLARE_METATYPE(Shield)
Q_DECLARE_METATYPE(VisibleView)
Q_DECLARE_METATYPE(GatewayPick)

namespace {

GPGateway gateway(const QString &name, const QString &address)
{
    GPGateway g;
    g.setName(name);
    g.setAddress(address);
    return g;
}

GPGateway berlin() { return gateway("Berlin", "berlin.example.com"); }
GPGateway paris() { return gateway("Paris", "paris.example.com"); }
GPGateway tokyo() { return gateway("Tokyo", "tokyo.example.com"); }

} // namespace

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
        // openconnect retries a lost Tunnel; the user can still stop it.
        QTest::newRow("Reconnecting") << ConnectionState::Reconnecting
            << "Reconnecting..." << Shield::InProgress << "Disconnect" << true << VisibleView::Connected;
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
        QVERIFY(presentConnection(ConnectionState::SwitchingGateway, 42).activeForText.isEmpty());
        // The timer only counts time with a working Tunnel; it restarts on recovery.
        QVERIFY(presentConnection(ConnectionState::Reconnecting, 42).activeForText.isEmpty());
    }

    void gatewayDropdownListsPortalConfigGateways()
    {
        const GatewayChoice c = presentGateways({ berlin(), paris(), tokyo() }, paris(), ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "Berlin", "Paris", "Tokyo" }));
        QCOMPARE(c.currentIndex, 1);
        QVERIFY(c.enabled);
    }

    // Each entry's address is shown with its name (as a tooltip).
    void gatewayDropdownGivesEachGatewayItsAddress()
    {
        const GatewayChoice c = presentGateways({ berlin(), paris(), tokyo() }, paris(), ConnectionState::Connected);

        QCOMPARE(c.addresses, QStringList({ "berlin.example.com", "paris.example.com", "tokyo.example.com" }));
    }

    // The Portal address used directly as a Gateway: the only Gateway is the Portal itself.
    void gatewayDropdownDisabledForPortalUsedAsGateway()
    {
        const GPGateway portal = gateway("vpn.example.com", "vpn.example.com");
        const GatewayChoice c = presentGateways({ portal }, portal, ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "vpn.example.com" }));
        QCOMPARE(c.currentIndex, 0);
        QVERIFY(!c.enabled);
    }

    void gatewayDropdownShowsCurrentGatewayWhenNoneListed()
    {
        const GatewayChoice c = presentGateways({}, gateway("vpn.example.com", "10.0.0.1"), ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "vpn.example.com" }));
        QCOMPARE(c.addresses, QStringList({ "10.0.0.1" }));
        QCOMPARE(c.currentIndex, 0);
        QVERIFY(!c.enabled);
    }

    // While the Tunnel goes down or moves, the dropdown shows the Gateway but cannot be used.
    void gatewayDropdownEnabledOnlyWhileConnected_data()
    {
        QTest::addColumn<ConnectionState>("state");
        QTest::addColumn<bool>("enabled");

        QTest::newRow("Connected") << ConnectionState::Connected << true;
        QTest::newRow("Disconnecting") << ConnectionState::Disconnecting << false;
        QTest::newRow("SwitchingGateway") << ConnectionState::SwitchingGateway << false;
        QTest::newRow("Reconnecting") << ConnectionState::Reconnecting << false;
    }

    void gatewayDropdownEnabledOnlyWhileConnected()
    {
        QFETCH(ConnectionState, state);
        QFETCH(bool, enabled);

        const GatewayChoice c = presentGateways({ berlin(), paris() }, paris(), state);

        QCOMPARE(c.currentIndex, 1);
        QCOMPARE(c.enabled, enabled);
    }

    // Picking a Gateway moves the Tunnel when Connected, only selects it when
    // Disconnected, and is ignored while a login or Tunnel change is in progress.
    void pickingGateway_data()
    {
        QTest::addColumn<ConnectionState>("state");
        QTest::addColumn<GatewayPick>("pick");

        QTest::newRow("Disconnected") << ConnectionState::Disconnected << GatewayPick::Select;
        QTest::newRow("Authenticating") << ConnectionState::Authenticating << GatewayPick::Ignore;
        QTest::newRow("Connecting") << ConnectionState::Connecting << GatewayPick::Ignore;
        QTest::newRow("Connected") << ConnectionState::Connected << GatewayPick::Switch;
        QTest::newRow("Disconnecting") << ConnectionState::Disconnecting << GatewayPick::Ignore;
        QTest::newRow("SwitchingGateway") << ConnectionState::SwitchingGateway << GatewayPick::Ignore;
        QTest::newRow("Reconnecting") << ConnectionState::Reconnecting << GatewayPick::Ignore;
    }

    void pickingGateway()
    {
        QFETCH(ConnectionState, state);
        QFETCH(GatewayPick, pick);

        QCOMPARE(gatewayPickFor(state), pick);
    }

    // After a Portal login the Gateway accepts the Portal Auth Cookie, so the switch is
    // silent; after a quick connect none is held and the Portal login runs first.
    void switchLogsInToPortalFirstWithoutPortalAuthCookie()
    {
        QCOMPARE(switchLoginFor(""), SwitchLogin::PortalThenGateway);
    }

    void switchLogsInToGatewayWithPortalAuthCookie()
    {
        QCOMPARE(switchLoginFor("c0ffee"), SwitchLogin::Gateway);
    }

    // The Portal login run for a switch ends on the Gateway the user picked, not on
    // the Portal's preferred one.
    void switchKeepsPickedGatewayAfterPortalLogin()
    {
        const GPGateway g = switchTargetAfterPortalLogin({ berlin(), paris(), tokyo() }, "Tokyo", paris());

        QCOMPARE(g.name(), QString("Tokyo"));
        QCOMPARE(g.address(), QString("tokyo.example.com"));
    }

    // The new Portal Config no longer lists the picked Gateway: the preferred one is used.
    void switchFallsBackToPreferredGatewayWhenPickedOneIsGone()
    {
        const GPGateway g = switchTargetAfterPortalLogin({ berlin(), paris() }, "Tokyo", paris());

        QCOMPARE(g.name(), QString("Paris"));
    }

    // The switch fallback timer read the VPN status: while the old Tunnel is still up
    // (or coming up) it runs on the previous Gateway, which becomes current again.
    void switchTimeoutWithOldTunnelUp_data()
    {
        QTest::addColumn<ConnectionState>("state");
        QTest::addColumn<bool>("restores");

        QTest::newRow("Connected") << ConnectionState::Connected << true;
        QTest::newRow("Connecting") << ConnectionState::Connecting << true;
        QTest::newRow("Reconnecting") << ConnectionState::Reconnecting << true;
        QTest::newRow("Disconnecting") << ConnectionState::Disconnecting << false;
        QTest::newRow("Disconnected") << ConnectionState::Disconnected << false;
    }

    void switchTimeoutWithOldTunnelUp()
    {
        QFETCH(ConnectionState, state);
        QFETCH(bool, restores);

        QCOMPARE(switchTimeoutRestoresPreviousGateway(state), restores);
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
