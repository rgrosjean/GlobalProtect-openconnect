// Tests for the connection presenter: for each ConnectionState, what the window
// shows (status text, shield, main button, visible view, Tunnel timer), and the
// Gateway dropdown of the connected view.

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
        QVERIFY(presentConnection(ConnectionState::SwitchingGateway, 42).activeForText.isEmpty());
    }

    void gatewayDropdownListsPortalConfigGateways()
    {
        const GatewayChoice c = presentGateways({ "Berlin", "Paris", "Tokyo" }, "Paris", ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "Berlin", "Paris", "Tokyo" }));
        QCOMPARE(c.currentIndex, 1);
        QVERIFY(c.enabled);
    }

    // The Portal address used directly as a Gateway: the only Gateway is the Portal itself.
    void gatewayDropdownDisabledForPortalUsedAsGateway()
    {
        const GatewayChoice c = presentGateways({ "vpn.example.com" }, "vpn.example.com", ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "vpn.example.com" }));
        QCOMPARE(c.currentIndex, 0);
        QVERIFY(!c.enabled);
    }

    void gatewayDropdownShowsCurrentGatewayWhenNoneListed()
    {
        const GatewayChoice c = presentGateways({}, "vpn.example.com", ConnectionState::Connected);

        QCOMPARE(c.names, QStringList({ "vpn.example.com" }));
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
    }

    void gatewayDropdownEnabledOnlyWhileConnected()
    {
        QFETCH(ConnectionState, state);
        QFETCH(bool, enabled);

        const GatewayChoice c = presentGateways({ "Berlin", "Paris" }, "Paris", state);

        QCOMPARE(c.currentIndex, 1);
        QCOMPARE(c.enabled, enabled);
    }
};

QTEST_GUILESS_MAIN(TestConnectionView)
#include "tst_connectionview.moc"
