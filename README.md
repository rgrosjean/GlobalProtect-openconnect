> # Disclaimer
>
> This project is a modified fork of [kakwa/GlobalProtect-openconnect](https://github.com/kakwa/GlobalProtect-openconnect),
> itself a fork of the [yuezk/GlobalProtect-openconnect 1.x branch](https://github.com/yuezk/GlobalProtect-openconnect/tree/1.x).
>
> # Changes from Upstream
>
> List of modifications:
> * Switch to Qt6 (kakwa)
>
> Modified by Romain Grosjean, starting November 2026:
> * **CAS Login** (Prisma Access Cloud Authentication Service, e.g. `*.gpcloudservice.com` with a Microsoft login):
>   the Prelogin advertises CAS support and the embedded browser handles the `globalprotectcallback:` result.
> * **Client identity** aligned with the official client (Client OS, OS version, client version, stable Host ID).
>   The values follow the MIT-licensed `gpapi` crate of [yuezk/GlobalProtect-openconnect 2.x](https://github.com/yuezk/GlobalProtect-openconnect).
>   They can be overridden in the client settings with `clientos`, `os-version`, `client-version` and `host-id`.
> * **Persistent login profile**: the identity provider remembers your account between runs. `gpclient --reset` clears it.
> * **TLS certificate verification** of the Portal and Gateways against the system trust store, which used to be disabled.
>   For a Portal with a self-signed certificate, see [Self-signed Portal certificates](#self-signed-portal-certificates).
> * Credentials (cookies, tokens, passwords) are redacted from the logs.
> * Build fixes for Ubuntu 26.04 (Qt 6.10, CMake 3.31) and automated tests for the request helper (`ctest`).

# GlobalProtect-openconnect

A GlobalProtect VPN client (GUI) for Linux based on Openconnect and built with Qt6, supports SAML auth mode, inspired by [gp-saml-gui](https://github.com/dlenski/gp-saml-gui).

<p align="center">
  <img src="https://user-images.githubusercontent.com/3297602/133869036-5c02b0d9-c2d9-4f87-8c81-e44f68cfd6ac.png">
</p>

## Features

- Similar user experience as the official client in macOS.
- Supports both SAML and non-SAML authentication modes.
- Supports automatically selecting the preferred gateway from the multiple gateways.
- Supports switching gateway from the system tray menu manually.

## Build & Install from source code

Clone this repo with:

```sh
git clone https://github.com/rgrosjean/GlobalProtect-openconnect.git
cd GlobalProtect-openconnect
```

### Ubuntu/Mint

Build and install with:

```sh
./scripts/install-ubuntu.sh
```

### Other Linux

Install the Qt6 dependencies and OpenConnect:

- QtCore
- QtWebEngine
- QtWebSockets
- QtDBus
- openconnect v8.x
- qtkeychain

...then build and install with:

```sh
./scripts/install.sh
```

To rebuild and reinstall after a change, run `./scripts/install.sh` (it asks for your password to install).

The build also compiles the tests when Qt6 Test and the `openssl` command are available. Run them with:

```sh
cd build && ctest
```

## Run

Once the software is installed, you can run `gpclient` to start the UI.

## Self-signed Portal certificates

The client verifies the TLS certificate of the Portal and of every Gateway against the system trust store.
If verification fails, the connection stops and the error message names the host and the TLS error.

If your Portal uses a self-signed certificate and you trust it, add this to `~/.config/com.yuezk.qt/GPClient.conf`:

```ini
[General]
insecure-tls=true
```

This turns verification off for the Portal and all Gateways, so anyone on your network could impersonate them.
The client logs a warning on each request while it is set, and `gpclient --reset` keeps it.
Remove the line to turn verification back on.

## Passing the Custom Parameters to `OpenConnect` CLI

See [Configuration](https://github.com/yuezk/GlobalProtect-openconnect/wiki/Configuration)

## Display the system tray icon on Gnome 40

Install the [AppIndicator and KStatusNotifierItem Support](https://extensions.gnome.org/extension/615/appindicator-support/) extension and you will see the system try icon (Restart the system after the installation).

<p align="center">
  <img src="https://user-images.githubusercontent.com/3297602/130831022-b93492fd-46dd-4a8e-94a4-13b5747120b7.png" />
<p>


## Troubleshooting

Run `gpclient` in the Terminal and collect the logs.

## [License](./LICENSE)
GPLv3
