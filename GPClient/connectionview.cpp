#include "connectionview.h"

namespace {

// While work is in progress the button shows the state and cannot be clicked,
// so a second click cannot start a second login.
ConnectionView inProgress(const QString &text, VisibleView view)
{
    return { text, Shield::InProgress, text, false, view, {} };
}

// "Active for hh:mm:ss"; hours keep counting past 24.
QString activeFor(qint64 seconds)
{
    return QString("Active for %1:%2:%3")
        .arg(seconds / 3600, 2, 10, QChar('0'))
        .arg(seconds / 60 % 60, 2, 10, QChar('0'))
        .arg(seconds % 60, 2, 10, QChar('0'));
}

} // namespace

ConnectionView presentConnection(ConnectionState state, qint64 tunnelSeconds)
{
    switch (state) {
    case ConnectionState::Authenticating:
        return inProgress("Authenticating...", VisibleView::Disconnected);
    case ConnectionState::Connecting:
        return inProgress("Connecting...", VisibleView::Disconnected);
    case ConnectionState::Connected:
        return { "Connected", Shield::Connected, "Disconnect", true, VisibleView::Connected, activeFor(tunnelSeconds) };
    case ConnectionState::Disconnecting:
        return inProgress("Disconnecting...", VisibleView::Connected);
    case ConnectionState::SwitchingGateway:
        return inProgress("Switching Gateway...", VisibleView::Connected);
    case ConnectionState::Disconnected:
        break;
    }
    return { "Not Connected", Shield::NotConnected, "Connect", true, VisibleView::Disconnected, {} };
}
