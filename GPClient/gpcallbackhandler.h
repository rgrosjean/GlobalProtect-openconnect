#ifndef GPCALLBACKHANDLER_H
#define GPCALLBACKHANDLER_H

#include <QtCore/QUrl>
#include <QtWebEngineCore/QWebEngineUrlSchemeHandler>

// Catches the "globalprotectcallback:" URL that ends a CAS (Cloud Authentication
// Service) login, so the embedded browser does not hand it to the desktop.
class GPCallbackHandler : public QWebEngineUrlSchemeHandler
{
    Q_OBJECT

public:
    static constexpr const char *SCHEME = "globalprotectcallback";

    static GPCallbackHandler *instance();

    // Must be called before the QApplication is created.
    static void registerScheme();
    // Must be called after the QApplication is created.
    static void install();

    void requestStarted(QWebEngineUrlRequestJob *job) override;

    void notify(const QUrl &url);

signals:
    void callbackReceived(const QUrl &url);

private:
    explicit GPCallbackHandler(QObject *parent = nullptr);
};

#endif // GPCALLBACKHANDLER_H
