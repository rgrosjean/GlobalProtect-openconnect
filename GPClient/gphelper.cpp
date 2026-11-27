#include <QtCore/QXmlStreamReader>
#include <QtCore/QSysInfo>
#include <QtCore/QUuid>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslSocket>
#include <plog/Log.h>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <keychain.h>

#include "gphelper.h"
#include "clientos.h"

using namespace QKeychain;

QNetworkAccessManager* gpclient::helper::networkManager = new QNetworkAccessManager;

static const char *certificateErrorProperty = "gpclientCertificateError";

QNetworkReply* gpclient::helper::createRequest(QString url, QByteArray params)
{
    QNetworkRequest request(url);

    // Verify the server certificate against the system trust store, unless the
    // user opted out for a Portal with a self-signed certificate
    QSslConfiguration conf = request.sslConfiguration();
    if (settings::get("insecure-tls", false).toBool()) {
        LOGW << "TLS certificate verification is disabled (insecure-tls=true) for " << request.url().host();
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    }
    conf.setSslOption(QSsl::SslOptionDisableLegacyRenegotiation, false);
    request.setSslConfiguration(conf);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());

    auto *reply = networkManager->post(request, params == nullptr ? QByteArray(nullptr) : params);

    // Remember that the certificate was rejected, to tell it apart from other
    // handshake failures (no shared protocol or cipher, ...)
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &errors) {
        if (!errors.isEmpty()) {
            reply->setProperty(certificateErrorProperty, true);
        }
    });
    return reply;
}

QString gpclient::helper::clientos()
{
    return settings::get("clientos", defaultClientOs()).toString();
}

QString gpclient::helper::clientVersion()
{
    auto version = settings::get("client-version", "").toString();
    if (!version.isEmpty()) {
        return version;
    }

    const auto os = clientos();
    if (os == "Windows") {
        return "6.3.3-650";
    }
    if (os == "Mac") {
        return "6.3.3-915";
    }
    return "6.3.3-619";
}

QString gpclient::helper::osVersion()
{
    auto version = settings::get("os-version", "").toString();
    if (!version.isEmpty()) {
        return version;
    }

    const auto os = clientos();
    if (os == "Windows") {
        return "Microsoft Windows 11 Pro , 64-bit";
    }
    if (os == "Mac") {
        return "Apple Mac OS X 13.4.0";
    }
    return "Linux " + QSysInfo::prettyProductName();
}

QString gpclient::helper::hostId()
{
    auto id = settings::get("host-id", "").toString();
    if (!id.isEmpty()) {
        return id;
    }

    // Stable UUID derived from the machine id (product_uuid is only readable by root)
    auto seed = QSysInfo::machineUniqueId();
    if (seed.isEmpty()) {
        seed = QSysInfo::machineHostName().toUtf8();
    }
    id = QUuid::createUuidV5(QUuid(), seed).toString(QUuid::WithoutBraces);

    // Persist it, so the portal keeps seeing the same device even if the hostname changes
    settings::save("host-id", id);
    return id;
}

QString gpclient::helper::redact(const QString &secret)
{
    return secret.isEmpty() ? "<empty>" : QString("<redacted, %1 chars>").arg(secret.length());
}

bool gpclient::helper::isCertificateError(const QNetworkReply *reply)
{
    return reply->error() == QNetworkReply::SslHandshakeFailedError
        && reply->property(certificateErrorProperty).toBool();
}

QString gpclient::helper::certificateErrorMessage(const QString &role, const QNetworkReply *reply)
{
    const auto url = reply->url();
    const auto address = url.port() > 0 ? QString("%1:%2").arg(url.host()).arg(url.port()) : url.host();
    return QString("The TLS certificate of the %1 %2 could not be verified: %3.\n\n"
                   "If you trust this %1, add insecure-tls=true under [General] in %4 and connect again.")
        .arg(role, address, reply->errorString(), settings::_settings->fileName());
}

QString gpclient::helper::userAgent()
{
    return QString("%1/%2 (%3)").arg(UA, clientVersion(), osVersion());
}

QNetworkReply* gpclient::helper::createPreloginRequest(const QString &url, const QString &defaultBrowser, const QString &os)
{
    QUrlQuery params;
    params.addQueryItem("tmp", "tmp");
    params.addQueryItem("clientVer", "4100");
    if (!os.isEmpty()) {
        params.addQueryItem("clientos", os);
    }
    params.addQueryItem("os-version", QUrl::toPercentEncoding(osVersion()));
    params.addQueryItem("host-id", hostId());
    params.addQueryItem("ipv6-support", "yes");
    params.addQueryItem("default-browser", defaultBrowser);
    params.addQueryItem("cas-support", "yes");
    // base64 of {"cas_embedded_browser":"yes"}
    params.addQueryItem("data", "eyJjYXNfZW1iZWRkZWRfYnJvd3NlciI6InllcyJ9");

    // Always advertise Kerberos SSO in the query, as earlier versions did
    const QString kerberosUrl = url + "?kerberos-support=yes";

    // No secrets in the prelogin parameters
    LOGI << "Prelogin request to " << kerberosUrl << " with " << params.toString(QUrl::FullyDecoded);

    // Linux and macOS clients send the parameters in the body, Windows in the query
    if (os == "Windows") {
        return createRequest(kerberosUrl + "&" + params.toString(QUrl::FullyEncoded));
    }
    return createRequest(kerberosUrl, params.toString(QUrl::FullyEncoded).toUtf8());
}

QWebEngineProfile* gpclient::helper::webProfile()
{
    // Parented to the application, so it outlives every login page
    static auto *profile = [] {
        auto *p = new QWebEngineProfile("gpclient", QCoreApplication::instance());
        p->setPersistentCookiesPolicy(QWebEngineProfile::AllowPersistentCookies);
        return p;
    }();
    return profile;
}

GPGateway gpclient::helper::filterPreferredGateway(QList<GPGateway> gateways, const QString ruleName)
{
    LOGI << gateways.size() << " gateway(s) available, filter the gateways with rule: " << ruleName;

    GPGateway gateway = gateways.first();

    for (GPGateway g : gateways) {
        if (g.priorityOf(ruleName) > gateway.priorityOf(ruleName)) {
            LOGI << "Find a preferred gateway: " << g.name();
            gateway = g;
        }
    }

    return gateway;
}

QUrlQuery gpclient::helper::parseGatewayResponse(const QByteArray &xml)
{
    LOGI << "Start parsing the gateway response...";

    QXmlStreamReader xmlReader{xml};
    QList<QString> args;

    while (!xmlReader.atEnd()) {
        xmlReader.readNextStartElement();
        if (xmlReader.name() == QString("argument")) {
            args.append(QUrl::toPercentEncoding(xmlReader.readElementText()));
        }
    }

    QUrlQuery params{};
    params.addQueryItem("authcookie", args.at(1));
    params.addQueryItem("portal", args.at(3));
    params.addQueryItem("user", args.at(4));
    params.addQueryItem("domain", args.at(7));
    params.addQueryItem("preferred-ip", args.at(15));
    params.addQueryItem("computer", QUrl::toPercentEncoding(QSysInfo::machineHostName()));

    LOGI << "The gateway response has " << args.size() << " arguments, authcookie: " << redact(args.at(1))
         << ", portal: " << args.at(3) << ", user: " << args.at(4) << ", domain: " << args.at(7)
         << ", preferred-ip: " << args.at(15);

    return params;
}

void gpclient::helper::openMessageBox(const QString &message, const QString& informativeText)
{
    QMessageBox msgBox;
    msgBox.setWindowTitle("Notice");
    msgBox.setText(message);
    msgBox.setFixedWidth(500);
    msgBox.setStyleSheet("QLabel{min-width: 250px}");
    msgBox.setInformativeText(informativeText);
    msgBox.exec();
}

void gpclient::helper::moveCenter(QWidget *widget)
{
    if (!widget)
        return;

    // Get the screen associated with the widget
    QScreen *screen = widget->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    QRect screenGeometry = screen->availableGeometry();
    QSize windowSize = widget->size();

    int x = (screenGeometry.width() - windowSize.width()) / 2 + screenGeometry.x();
    int y = (screenGeometry.height() - windowSize.height()) / 2 + screenGeometry.y() - 50;

    widget->move(x, y);
}

QSettings *gpclient::helper::settings::_settings = new QSettings("com.yuezk.qt", "GPClient");

QVariant gpclient::helper::settings::get(const QString &key, const QVariant &defaultValue)
{
    return _settings->value(key, defaultValue);
}

QStringList gpclient::helper::settings::get_all(const QString &key, const QVariant &defaultValue)
{
	QRegularExpression re(key);
	return 	_settings->allKeys().filter(re);
}

void gpclient::helper::settings::save(const QString &key, const QVariant &value)
{
    _settings->setValue(key, value);
}


void gpclient::helper::settings::clear()
{
    const QStringList keys = _settings->allKeys();
    for (const auto &key : keys) {
        if (!reservedKeys.contains(key)) {
            _settings->remove(key);
        }
    }

    webProfile()->cookieStore()->deleteAllCookies();
}


bool gpclient::helper::settings::secureSave(const QString &key, const QString &value) {
    WritePasswordJob job( QLatin1String("gpclient") );
    job.setAutoDelete( false );
    job.setKey( key );
    job.setTextData( value );
    QEventLoop loop;
    job.connect( &job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()) );
    job.start();
    loop.exec();
    if ( job.error() ) {
        return false;
    }

    return true;
}

bool gpclient::helper::settings::secureGet(const QString &key, QString &value) {
    ReadPasswordJob job( QLatin1String("gpclient") );
    job.setAutoDelete( false );
    job.setKey( key );
    QEventLoop loop;
    job.connect( &job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()) );
    job.start();
    loop.exec();

    const QString pw = job.textData();
    if ( job.error() ) {
        return false;
    }

    value = pw;
    return true;
}
