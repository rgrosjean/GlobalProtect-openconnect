#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineUrlRequestJob>
#include <QtWebEngineCore/QWebEngineUrlScheme>
#include <plog/Log.h>

#include "gpcallbackhandler.h"
#include "gphelper.h"

GPCallbackHandler::GPCallbackHandler(QObject *parent) : QWebEngineUrlSchemeHandler(parent)
{
}

GPCallbackHandler *GPCallbackHandler::instance()
{
    static auto *handler = new GPCallbackHandler;
    return handler;
}

void GPCallbackHandler::registerScheme()
{
    QWebEngineUrlScheme scheme(SCHEME);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    scheme.setFlags(QWebEngineUrlScheme::LocalAccessAllowed | QWebEngineUrlScheme::CorsEnabled);
    QWebEngineUrlScheme::registerScheme(scheme);
}

void GPCallbackHandler::install()
{
    gpclient::helper::webProfile()->installUrlSchemeHandler(SCHEME, instance());
}

void GPCallbackHandler::requestStarted(QWebEngineUrlRequestJob *job)
{
    notify(job->requestUrl());
    job->fail(QWebEngineUrlRequestJob::RequestAborted);
}

void GPCallbackHandler::notify(const QUrl &url)
{
    LOGI << "Received the " << SCHEME << " URL";
    emit callbackReceived(url);
}
