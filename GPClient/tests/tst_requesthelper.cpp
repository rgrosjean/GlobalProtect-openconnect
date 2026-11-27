// Tests for the request helper, which every Portal and Gateway request goes through.
// Each test starts a TLS server on a loopback port, sends a request through the
// helper, and checks only whether the request completes or fails with an SSL error.

#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslServer>
#include <QtNetwork/QSslSocket>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include "gphelper.h"

using namespace gpclient::helper;

namespace {

QByteArray readFile(const QString &name)
{
    QFile file(QStringLiteral(TEST_CERT_DIR "/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Cannot read test certificate %s", qPrintable(file.fileName()));
    }
    return file.readAll();
}

QSslCertificate certificate(const QString &name)
{
    return QSslCertificate(readFile(name), QSsl::Pem);
}

// A minimal HTTPS server on 127.0.0.1 that answers every request with "200 OK".
// It records the bytes it receives, so a test can check that nothing was sent.
class TlsServer : public QSslServer
{
public:
    QByteArray received;

    explicit TlsServer(const QString &certName)
    {
        QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
        conf.setLocalCertificate(certificate(certName + ".pem"));
        conf.setPrivateKey(QSslKey(readFile(certName + ".key"), QSsl::Rsa, QSsl::Pem));
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        setSslConfiguration(conf);

        connect(this, &QTcpServer::pendingConnectionAvailable, this, [this] {
            while (auto *socket = nextPendingConnection()) {
                connect(socket, &QIODevice::readyRead, socket, [this, socket] {
                    received += socket->readAll();
                    if (received.contains("\r\n\r\n")) {
                        socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nok");
                        socket->disconnectFromHost();
                    }
                });
            }
        });

        if (!listen(QHostAddress::LocalHost)) {
            qFatal("Cannot listen on 127.0.0.1: %s", qPrintable(errorString()));
        }
    }

    QString url() const
    {
        return QString("https://127.0.0.1:%1/ssl-vpn/prelogin.esp").arg(serverPort());
    }
};

// Send a request with a body through the helper and wait for it to finish.
// The reply is deleted when control returns to the event loop.
QNetworkReply *send(const QString &url)
{
    auto *reply = createRequest(url, "user=test&passwd=secret");
    QSignalSpy finished(reply, &QNetworkReply::finished);
    if (!reply->isFinished() && !finished.wait(10000)) {
        qWarning("Request timed out");
    }
    if (reply->error() != QNetworkReply::NoError) {
        qInfo("Request failed: %s", qPrintable(reply->errorString()));
    }
    reply->deleteLater();
    return reply;
}

}

class TestRequestHelper : public QObject
{
    Q_OBJECT

    QTemporaryDir settingsDir;
    QSslConfiguration originalSslConfiguration;

private slots:
    void initTestCase()
    {
        QVERIFY(settingsDir.isValid());
        // Never touch the user's real client settings
        settings::_settings = new QSettings(settingsDir.filePath("GPClient.conf"), QSettings::IniFormat);
        originalSslConfiguration = QSslConfiguration::defaultConfiguration();
    }

    void cleanup()
    {
        settings::_settings->clear();
        QSslConfiguration::setDefaultConfiguration(originalSslConfiguration);
        // Do not reuse a connection or TLS session from a previous test
        networkManager->clearConnectionCache();
        networkManager->clearAccessCache();
    }

    void certificateSignedByTrustedTestCaIsAccepted()
    {
        // Trust the test CA for this test only
        auto conf = QSslConfiguration::defaultConfiguration();
        conf.addCaCertificate(certificate("ca.pem"));
        QSslConfiguration::setDefaultConfiguration(conf);

        TlsServer server("server");

        QCOMPARE(send(server.url())->error(), QNetworkReply::NoError);
    }

    void selfSignedCertificateIsRejectedByDefault()
    {
        TlsServer server("selfsigned");

        auto *reply = send(server.url());

        QCOMPARE(reply->error(), QNetworkReply::SslHandshakeFailedError);
        QVERIFY(isCertificateError(reply));
        // The handshake failed, so no credential reached the server
        QVERIFY(server.received.isEmpty());
    }

    void selfSignedCertificateIsAcceptedWithInsecureTls()
    {
        settings::save("insecure-tls", true);
        TlsServer server("selfsigned");

        QCOMPARE(send(server.url())->error(), QNetworkReply::NoError);
    }
};

QTEST_GUILESS_MAIN(TestRequestHelper)
#include "tst_requesthelper.moc"
