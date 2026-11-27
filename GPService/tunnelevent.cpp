#include "tunnelevent.h"

namespace {

struct Marker {
    const char *text;  // a fixed part of the openconnect message
    TunnelEvent event;
};

// openconnect messages that mark a change of the Tunnel state. To support the
// wording of a new openconnect version, update this table and the test fixtures.
const Marker markers[] = {
    // gpst.c: the HTTPS Tunnel missed its keepalive replies.
    { "GPST Dead Peer Detection detected dead peer!", TunnelEvent::Lost },
    // esp.c: the ESP Tunnel missed its probe replies.
    { "ESP detected dead peer", TunnelEvent::Lost },
    // ssl.c, ssl_reconnect: "sleep %ds, remaining timeout %ds" after a failed attempt.
    { "s, remaining timeout ", TunnelEvent::Lost },
    // ssl.c: the TCP connection to the known Gateway address failed.
    { "Failed to reconnect to host ", TunnelEvent::Lost },
    { "Failed to reconnect to proxy ", TunnelEvent::Lost },

    // gnutls.c: a new TLS connection to the Gateway succeeded. During a reconnect in
    // HTTPS mode it is the last message openconnect prints at its default verbosity.
    { "Connected to HTTPS on ", TunnelEvent::Recovered },
    // esp.c: the Gateway answered the ESP probes.
    { "ESP session established with server", TunnelEvent::Recovered },
    // gpst.c: traffic moved to the ESP Tunnel.
    { "ESP tunnel connected; exiting HTTPS mainloop.", TunnelEvent::Recovered },

    // main.c: the tun device is configured, printed once when the Tunnel first comes up.
    { "Configured as", TunnelEvent::Configured },
    { "Connected as", TunnelEvent::Configured },      // openconnect 8
    { "Configurado como", TunnelEvent::Configured },  // Spanish locale
};

} // namespace

TunnelEvent tunnelEventFor(const QString &line)
{
    for (const Marker &m : markers) {
        if (line.contains(QLatin1String(m.text))) {
            return m.event;
        }
    }
    return TunnelEvent::None;
}
