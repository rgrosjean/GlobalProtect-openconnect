#ifndef CONNECTIONVIEW_H
#define CONNECTIONVIEW_H

#include <QtCore/QString>
#include <QtCore/QStringList>

#include "gpgateway.h"

// Where the client is in its connection lifecycle. The window holds exactly one.
enum class ConnectionState {
    Disconnected,
    Authenticating,
    Connecting,
    Connected,
    Disconnecting,
    SwitchingGateway,
    Reconnecting  // the Tunnel was lost and openconnect is trying to get it back
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
    QStringList names;      // one entry per Gateway, in Portal Config order
    QStringList addresses;  // the address of each entry in names
    int currentIndex;       // the current Gateway, -1 if none
    bool enabled;           // whether the user can pick another Gateway
};

// Pure mapping from the Gateways of the Portal Config, the current Gateway and the
// ConnectionState to the Gateway dropdown.
GatewayChoice presentGateways(const QList<GPGateway> &gateways, const GPGateway &currentGateway, ConnectionState state);

// What picking another Gateway does.
enum class GatewayPick {
    Ignore,  // a login or Tunnel change is in progress
    Select,  // no Tunnel: the Gateway becomes the current one for the next connection
    Switch   // the Tunnel moves to the Gateway
};

GatewayPick gatewayPickFor(ConnectionState state);

// Which login a Gateway switch runs once the old Tunnel is down.
enum class SwitchLogin {
    Gateway,           // straight to the picked Gateway, with the Portal Auth Cookie held
    PortalThenGateway  // no Portal Auth Cookie held (after a quick connect): Portal login first
};

// A Gateway accepts the Portal Auth Cookie of the last Portal login, so the switch is
// silent; without one it answers with a new SAML Login, so the Portal login runs first.
SwitchLogin switchLoginFor(const QString &portalAuthCookie);

// The Gateway a switch logs in to after its Portal login: the picked one if the new
// Portal Config still lists it, otherwise the preferred one, as a normal Portal login.
GPGateway switchTargetAfterPortalLogin(const QList<GPGateway> &portalConfigGateways, const QString &pickedName, const GPGateway &preferred);

// Whether a Gateway switch whose old Tunnel never went down gives the current Gateway
// back to the previous one, given the state read from the VPN when the fallback timer
// fires: a Tunnel still up, coming up or reconnecting runs on the previous Gateway.
bool switchTimeoutRestoresPreviousGateway(ConnectionState stateAfterTimeout);

// Qt resource path of the SVG icon drawn for a shield, used by the window and the tray.
QString shieldResource(Shield shield);

#endif // CONNECTIONVIEW_H
