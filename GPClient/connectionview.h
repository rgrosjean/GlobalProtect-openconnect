#ifndef CONNECTIONVIEW_H
#define CONNECTIONVIEW_H

#include <QtCore/QString>
#include <QtCore/QStringList>

// Where the client is in its connection lifecycle. The window holds exactly one.
enum class ConnectionState {
    Disconnected,
    Authenticating,
    Connecting,
    Connected,
    Disconnecting,
    SwitchingGateway
};

enum class Shield {
    NotConnected,
    InProgress,
    Connected
};

// Which part of the window's central area is visible.
enum class VisibleView {
    Disconnected,
    Connected
};

// What the window shows for a ConnectionState.
struct ConnectionView {
    QString statusText;
    Shield shield;
    QString buttonText;
    bool buttonEnabled;
    VisibleView visibleView;
    QString activeForText;  // "Active for hh:mm:ss" while Connected, empty otherwise
};

// Pure mapping from a ConnectionState and the elapsed Tunnel time (in seconds)
// to what the window shows. No widget or DBus dependency, so it can be tested alone.
ConnectionView presentConnection(ConnectionState state, qint64 tunnelSeconds);

// What the connected view's Gateway dropdown shows.
struct GatewayChoice {
    QStringList names;  // one entry per Gateway, in Portal Config order
    int currentIndex;   // the current Gateway, -1 if none
    bool enabled;       // whether the user can pick another Gateway
};

// Pure mapping from the Gateways of the Portal Config, the current Gateway and the
// ConnectionState to the Gateway dropdown.
GatewayChoice presentGateways(const QStringList &gatewayNames, const QString &currentGateway, ConnectionState state);

// Qt resource path of the SVG icon drawn for a shield, used by the window and the tray.
QString shieldResource(Shield shield);

#endif // CONNECTIONVIEW_H
