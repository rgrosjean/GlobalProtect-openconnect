#include <QtCore/QUrlQuery>
#include <QtWidgets/QVBoxLayout>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineWidgets/QWebEngineView>
#include <QWebEngineCookieStore>
#include <plog/Log.h>

#include "samlloginwindow.h"
#include "gpcallbackhandler.h"
#include "gphelper.h"

SAMLLoginWindow::SAMLLoginWindow(QWidget *parent)
    : QDialog(parent)
    , webView(new EnhancedWebView(this))
{
    setWindowTitle("GlobalProtect Login");
    setModal(true);
    resize(700, 760);

    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    webView->setUrl(QUrl("about:blank"));
    webView->setAttribute(Qt::WA_DeleteOnClose);
    verticalLayout->addWidget(webView);

    webView->initialize();
    connect(webView, &EnhancedWebView::responseReceived, this, &SAMLLoginWindow::onResponseReceived);
    connect(webView, &EnhancedWebView::loadFinished, this, &SAMLLoginWindow::onLoadFinished);
    connect(GPCallbackHandler::instance(), &GPCallbackHandler::callbackReceived, this, &SAMLLoginWindow::onCallbackReceived);

    // Show the login window automatically when exceeds the MAX_WAIT_TIME
    QTimer::singleShot(MAX_WAIT_TIME, this, [this]() {
        if (failed) {
            return;
        }
        LOGI << "MAX_WAIT_TIME exceeded, display the login window.";
        this->show();
    });
}

SAMLLoginWindow::~SAMLLoginWindow()
{
    // Destroying the page runs pending toHtml() callbacks with an empty result.
    // Delete the view while this window is still whole and tell handleHtml() to
    // ignore them, otherwise it would show() a half-destroyed dialog.
    destroying = true;
    delete webView;
    webView = nullptr;
}

void SAMLLoginWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
    reject();
}

void SAMLLoginWindow::login(const QString samlMethod, const QString samlRequest, const QString preloginUrl)
{
    webView->page()->profile()->cookieStore()->deleteSessionCookies();

    if (samlMethod == "POST") {
        webView->setHtml(samlRequest, preloginUrl);
    } else if (samlMethod == "REDIRECT") {
        LOGI << "Redirect to " << samlRequest;
        webView->load(samlRequest);
    } else {
        LOGE << "Unknown saml-auth-method expected POST or REDIRECT, got " << samlMethod;
        failed = true;
        emit fail("ERR001", "Unknown saml-auth-method, got " + samlMethod);
    }
}

void SAMLLoginWindow::onResponseReceived(QJsonObject params)
{
    const auto type = params.value("type").toString();
    // Skip non-document response
    if (type != "Document") {
        return;
    }

    auto response = params.value("response").toObject();
    auto headers = response.value("headers").toObject();

    LOGI << "Trying to receive authentication cookie from " << response.value("url").toString();

    const auto username = headers.value("saml-username").toString();
    const auto preloginCookie = headers.value("prelogin-cookie").toString();
    const auto userAuthCookie = headers.value("portal-userauthcookie").toString();

    this->checkSamlResult(username, preloginCookie, userAuthCookie);
}

void SAMLLoginWindow::onCallbackReceived(const QUrl &url)
{
    if (done || failed) {
        return;
    }

    // Formats (see yuezk/GlobalProtect-openconnect 2.x, crates/gpapi/src/auth.rs):
    //   globalprotectcallback:cas-as=1&un=<user>&token=<token>      (CAS)
    //   globalprotectcallback:<base64 encoded SAML result HTML>
    QString data = QString::fromUtf8(url.toEncoded());
    data.remove(0, QString(GPCallbackHandler::SCHEME).length() + 1);
    while (data.startsWith('/')) {
        data.remove(0, 1);
    }

    if (data.startsWith("cas-as")) {
        LOGI << "Got the CAS authentication data";
        // Some clients encode the whole string ("cas-as%3D1%26un%3D..."): undo that
        // outer layer only, then let QUrlQuery decode each value exactly once.
        if (data.startsWith("cas-as%3D", Qt::CaseInsensitive)) {
            data = QUrl::fromPercentEncoding(data.toUtf8());
        }
        const QUrlQuery query(data);
        const auto username = query.queryItemValue("un", QUrl::FullyDecoded);
        const auto token = query.queryItemValue("token", QUrl::FullyDecoded);
        checkSamlResult(username, "", "", token);
        return;
    }

    const auto html = QString::fromUtf8(QByteArray::fromBase64(QUrl::fromPercentEncoding(data.toUtf8()).toUtf8()));
    if (parseTag("saml-auth-status", html) == "1") {
        checkSamlResult(parseTag("saml-username", html), parseTag("prelogin-cookie", html), parseTag("portal-userauthcookie", html));
    } else {
        LOGE << "Unexpected " << GPCallbackHandler::SCHEME << " data, authentication failed";
        failed = true;
        emit fail("ERR002", "Authentication failed, please try again.");
    }
}

void SAMLLoginWindow::checkSamlResult(QString username, QString preloginCookie, QString userAuthCookie, QString token)
{
    if (done) {
        return;
    }

    LOGI << "Checking the authentication result...";

    if (!username.isEmpty()) {
        samlResult.insert("username", username);
    }

    if (!preloginCookie.isEmpty()) {
        samlResult.insert("preloginCookie", preloginCookie);
    }

    if (!userAuthCookie.isEmpty()) {
        samlResult.insert("userAuthCookie", userAuthCookie);
    }

    if (!token.isEmpty()) {
        samlResult.insert("token", token);
    }

    // Check the SAML result
    if (samlResult.contains("username")
            && (samlResult.contains("preloginCookie") || samlResult.contains("userAuthCookie") || samlResult.contains("token"))) {
        LOGI << "Got the SAML authentication information successfully. "
             << "username: " << samlResult.value("username")
             << ", preloginCookie: " << gpclient::helper::redact(samlResult.value("preloginCookie"))
             << ", userAuthCookie: " << gpclient::helper::redact(samlResult.value("userAuthCookie"))
             << ", token: " << gpclient::helper::redact(samlResult.value("token"));

        done = true;
        emit success(samlResult);
        accept();
    }
}

void SAMLLoginWindow::onLoadFinished()
{
     LOGI << "Load finished " << webView->page()->url().toString();
     webView->page()->toHtml([this] (const QString &html) { this->handleHtml(html); });
}

void SAMLLoginWindow::handleHtml(const QString &html)
{
    if (destroying || done || failed) {
        return;
    }

    // try to check the html body and extract from there
    const auto samlAuthStatus = parseTag("saml-auth-status", html);

    if (samlAuthStatus == "1") {
        const auto preloginCookie = parseTag("prelogin-cookie", html);
        const auto username = parseTag("saml-username", html);
        const auto userAuthCookie = parseTag("portal-userauthcookie", html);

        checkSamlResult(username, preloginCookie, userAuthCookie);
    } else if (samlAuthStatus == "-1") {
        LOGI << "SAML authentication failed...";
        failed = true;
        emit fail("ERR002", "Authentication failed, please try again.");
    } else {
        show();
    }
}

QString SAMLLoginWindow::parseTag(const QString &tag, const QString &html) {
    const QRegularExpression expression(QString("<%1>(.*)</%1>").arg(tag));
    return expression.match(html).captured(1);
}
