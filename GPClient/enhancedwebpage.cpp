#include "enhancedwebpage.h"
#include "gpcallbackhandler.h"

bool EnhancedWebPage::acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame)
{
    // The CAS login ends with a redirect to "globalprotectcallback:...", catch it here
    if (url.scheme() == GPCallbackHandler::SCHEME) {
        GPCallbackHandler::instance()->notify(url);
        return false;
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}
