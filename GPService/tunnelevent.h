#ifndef TUNNELEVENT_H
#define TUNNELEVENT_H

#include <QtCore/QString>

// What a line of openconnect output says about the Tunnel.
enum class TunnelEvent {
    None,        // nothing about the Tunnel state
    Configured,  // the Tunnel came up for the first time
    Lost,        // the Tunnel died; openconnect retries until --reconnect-timeout
    Recovered    // openconnect reached the Gateway again after a loss
};

// Pure mapping from one line of openconnect output (stdout or stderr) to a
// TunnelEvent. No QProcess or DBus dependency, so it can be tested alone.
TunnelEvent tunnelEventFor(const QString &line);

#endif // TUNNELEVENT_H
