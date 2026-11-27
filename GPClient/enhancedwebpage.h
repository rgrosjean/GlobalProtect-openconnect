#ifndef ENHANCEDWEBPAGE_H
#define ENHANCEDWEBPAGE_H

#include <QtWebEngineCore/qwebenginepage.h>

class EnhancedWebPage : public QWebEnginePage
{
public:
    explicit EnhancedWebPage(QWebEngineProfile *profile, QObject *parent = nullptr) : QWebEnginePage(profile, parent) {}

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override;
};

#endif // ENHANCEDWEBPAGE_H
