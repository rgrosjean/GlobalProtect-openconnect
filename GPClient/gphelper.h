#ifndef GPHELPER_H
#define GPHELPER_H

#include <QtCore/QObject>
#include <QtCore/QUrlQuery>
#include <QtCore/QSettings>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "samlloginwindow.h"

class QWebEngineProfile;
#include "gpgateway.h"


const QString UA = "PAN GlobalProtect";

namespace gpclient {
    namespace helper {
        extern QNetworkAccessManager *networkManager;

        QNetworkReply* createRequest(QString url, QByteArray params = nullptr);

        GPGateway filterPreferredGateway(QList<GPGateway> gateways, const QString ruleName);

        QUrlQuery parseGatewayResponse(const QByteArray& xml);

        void openMessageBox(const QString& message, const QString& informativeText = "");

        void moveCenter(QWidget *widget);

        // Client identity sent to the portal/gateway, aligned with the official client
        // (see yuezk/GlobalProtect-openconnect 2.x, crates/gpapi/src/os_profile).
        QString clientos();
        QString clientVersion();
        QString osVersion();
        QString hostId();
        QString userAgent();

        // Describe a secret for logging without revealing it
        QString redact(const QString &secret);

        // Whether a request made by createRequest failed because the server
        // certificate could not be verified
        bool isCertificateError(const QNetworkReply *reply);

        // User-facing message for a certificate failure: the role ("Portal" or "Gateway"),
        // the host, the TLS error reported by Qt, and the insecure-tls opt-out
        QString certificateErrorMessage(const QString &role, const QNetworkReply *reply);

        // Persistent web profile for the SAML/CAS login, so the identity provider
        // (e.g. Microsoft) remembers the account between runs.
        QWebEngineProfile* webProfile();

        // Send a prelogin request advertising CAS support (Cloud Authentication Service)
        // with the embedded browser. defaultBrowser is "-10" for the portal, "0" for a gateway.
        QNetworkReply* createPreloginRequest(const QString &url, const QString &defaultBrowser, const QString &clientos);

        namespace settings {

            extern QSettings *_settings;
            static const QStringList reservedKeys {"extraArgs", "clientos", "os-version", "client-version", "host-id", "insecure-tls"};

            QVariant get(const QString &key, const QVariant &defaultValue = QVariant());
            QStringList get_all(const QString &key, const QVariant &defaultValue = QVariant());
            void save(const QString &key, const QVariant &value);
            void clear();

            bool secureSave(const QString &key, const QString &value);
            bool secureGet(const QString &key, QString &value);
        }
    }
}

#endif // GPHELPER_H
