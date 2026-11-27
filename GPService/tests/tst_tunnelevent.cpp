// Tests for the matching of openconnect output lines to Tunnel events. The lines
// are the messages of openconnect 9.12 for the GlobalProtect protocol, taken from
// its source (gpst.c, esp.c, ssl.c, gnutls.c, main.c), with the values a real run
// prints in place of the format specifiers.

#include <QtTest/QTest>

#include "tunnelevent.h"

Q_DECLARE_METATYPE(TunnelEvent)

class TestTunnelEvent : public QObject
{
    Q_OBJECT

private slots:
    void matchesLine_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<TunnelEvent>("event");

        // gpst.c: the HTTPS Tunnel missed its keepalive replies.
        QTest::newRow("GPST dead peer")
            << "GPST Dead Peer Detection detected dead peer!" << TunnelEvent::Lost;
        // esp.c: the ESP Tunnel missed its probe replies.
        QTest::newRow("ESP dead peer")
            << "ESP detected dead peer" << TunnelEvent::Lost;
        // ssl.c, ssl_reconnect: an attempt to reach the Gateway again failed.
        QTest::newRow("reconnect retry")
            << "sleep 10s, remaining timeout 290s" << TunnelEvent::Lost;
        // ssl.c: the TCP connection to the known Gateway address failed.
        QTest::newRow("reconnect to host failed")
            << "Failed to reconnect to host gw.example.com: Network is unreachable" << TunnelEvent::Lost;
        QTest::newRow("reconnect to proxy failed")
            << "Failed to reconnect to proxy proxy.example.com: Connection timed out" << TunnelEvent::Lost;

        // gnutls.c: a new TLS connection to the Gateway succeeded. During a reconnect
        // in HTTPS mode it is the last message printed by default.
        QTest::newRow("HTTPS connected")
            << "Connected to HTTPS on gw.example.com with ciphersuite (TLS1.3)-(ECDHE-SECP256R1)-(RSA-PSS-RSAE-SHA256)-(AES-256-GCM)"
            << TunnelEvent::Recovered;
        // esp.c: the Gateway answered the ESP probes.
        QTest::newRow("ESP established")
            << "ESP session established with server" << TunnelEvent::Recovered;
        // gpst.c: traffic moved to the ESP Tunnel.
        QTest::newRow("ESP tunnel connected")
            << "ESP tunnel connected; exiting HTTPS mainloop." << TunnelEvent::Recovered;

        // main.c: the tun device is configured, printed once when the Tunnel first comes up.
        QTest::newRow("Configured as")
            << "Configured as 10.0.0.2, with SSL disconnected and ESP established" << TunnelEvent::Configured;
        // openconnect 8: the same moment.
        QTest::newRow("Connected as")
            << "Connected as 10.0.0.2, using SSL, with ESP in progress" << TunnelEvent::Configured;
        // A Spanish locale translates "Configured as".
        QTest::newRow("Configurado como")
            << "Configurado como 10.0.0.2, con SSL desconectado y ESP establecido" << TunnelEvent::Configured;

        // Lines that say nothing about a lost or recovered Tunnel.
        QTest::newRow("empty") << "" << TunnelEvent::None;
        QTest::newRow("first TCP connection")
            << "Connected to 203.0.113.10:443" << TunnelEvent::None;
        QTest::newRow("rekey interval")
            << "Tunnel timeout (rekey interval) is 180 minutes." << TunnelEvent::None;
        // A planned reconnect: only a failed attempt marks the Tunnel as lost.
        QTest::newRow("rekey due") << "GlobalProtect rekey due" << TunnelEvent::None;
        // Also printed when ESP does not come back after a rekey while HTTPS works;
        // a lost ESP Tunnel is already reported by "ESP detected dead peer".
        QTest::newRow("ESP fallback")
            << "Failed to connect ESP tunnel; using HTTPS instead." << TunnelEvent::None;
        // openconnect gives up and exits; the process exit reports the disconnect.
        QTest::newRow("reconnect failed") << "Reconnect failed" << TunnelEvent::None;
        QTest::newRow("GPST connect failed")
            << "GPST connect failed; exiting" << TunnelEvent::None;
        QTest::newRow("HIP check due") << "GlobalProtect HIP check due" << TunnelEvent::None;
    }

    void matchesLine()
    {
        QFETCH(QString, line);
        QFETCH(TunnelEvent, event);

        QCOMPARE(tunnelEventFor(line), event);
    }
};

QTEST_APPLESS_MAIN(TestTunnelEvent)
#include "tst_tunnelevent.moc"
