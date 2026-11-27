#include <QtWidgets/QToolButton>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include <plog/Log.h>

#include <QtWidgets/QMessageBox>

#include "gpclient.h"
#include "aboutinfo.h"
#include "version.h"
#include "gphelper.h"
#include "ui_gpclient.h"
#include "portalauthenticator.h"
#include "gatewayauthenticator.h"
#include "settingsdialog.h"
#include "gatewayauthenticatorparams.h"

using namespace gpclient::helper;

GPClient::GPClient(QWidget *parent, IVpn *vpn)
    : QMainWindow(parent)
    , ui(new Ui::GPClient)
    , vpn(vpn)
    , settingsDialog(new SettingsDialog(this))
    , tunnelTimer(new QTimer(this))
    , switchTimeout(new QTimer(this))
{
    ui->setupUi(this);

    setWindowTitle("GlobalProtect");
    // Fixed width from the .ui file; the height follows the visible view (see showView).
    setFixedWidth(width());

    setupSettings();

    // Restore portal from the previous settings
    this->portal(settings::get("portal", "").toString());

    // DBus service setup
    QObject *ov = dynamic_cast<QObject*>(vpn);
    connect(ov, SIGNAL(connected()), this, SLOT(onVPNConnected()));
    connect(ov, SIGNAL(disconnected()), this, SLOT(onVPNDisconnected()));
    connect(ov, SIGNAL(error(QString)), this, SLOT(onVPNError(QString)));
    connect(ov, SIGNAL(logAvailable(QString)), this, SLOT(onVPNLogAvailable(QString)));

    // Connected view: the Gateway dropdown switches Gateway, the timer refreshes "Active for".
    connect(ui->gatewayInput, &QComboBox::activated, this, &GPClient::onGatewayInputActivated);
    tunnelTimer->setInterval(1000);
    connect(tunnelTimer, &QTimer::timeout, this, &GPClient::onTunnelTimerTick);
    switchTimeout->setSingleShot(true);
    switchTimeout->setInterval(30000);
    connect(switchTimeout, &QTimer::timeout, this, &GPClient::onSwitchTimeout);

    // Initialize the context menu of system tray.
    initSystemTrayIcon();
    setupMenu();
    initVpnStatus();

    // Centre once the height follows the visible view.
    gpclient::helper::moveCenter(this);
}

void GPClient::setupSettings()
{
    connect(settingsDialog, &QDialog::accepted, this, &GPClient::onSettingsAccepted);
}

namespace {

// Three dots drawn in the given colour: the "..." glyph is missing from many fonts.
QIcon ellipsisIcon(const QColor &colour)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    for (int i = 0; i < 3; ++i) {
        painter.drawEllipse(QPointF(14 + i * 18, 32), 5.5, 5.5);
    }
    return QIcon(pixmap);
}

} // namespace

// The "..." button in the top-right corner. Its entries reuse the tray's slots and its
// Switch Gateway submenu, so every action is reachable without a system tray.
void GPClient::setupMenu()
{
    moreButton = new QToolButton(this);
    moreButton->setPopupMode(QToolButton::InstantPopup);
    moreButton->setToolTip("Menu");
    moreButton->setIconSize(QSize(20, 20));
    moreButton->setFixedSize(QSize(28, 28));
    moreButton->setStyleSheet("QToolButton::menu-indicator { image: none; }");
    moreButton->move(width() - moreButton->width() - 15, 15);

    moreMenu = new QMenu(this);
    moreMenu->addAction("Settings", this, &GPClient::onSettingsButtonClicked);
    moreMenu->addMenu(gatewaySwitchMenu);
    resetMenuAction = moreMenu->addAction("Reset", this, &GPClient::reset);
    moreMenu->addAction("About", this, &GPClient::showAbout);
    moreMenu->addAction("Quit", this, &GPClient::quit);
    moreButton->setMenu(moreMenu);
    updateMenuIcon();
}

// The dots use the palette's button text colour, so they follow a light or dark theme.
void GPClient::updateMenuIcon()
{
    moreButton->setIcon(ellipsisIcon(palette().buttonText().color()));
}

void GPClient::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    // Palette events can arrive while setupUi() runs, before the button exists.
    if (moreButton && (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange)) {
        updateMenuIcon();
    }
}

void GPClient::showAbout()
{
    QMessageBox::about(this, "About GlobalProtect", aboutHtml(VERSION));
}

void GPClient::onSettingsButtonClicked()
{
    settingsDialog->setClientos(clientos());
    settingsDialog->setOsVersion(settings::get("os-version", QSysInfo::prettyProductName()).toString());
    settingsDialog->show();
}

void GPClient::onSettingsAccepted()
{
    settings::save("clientos", settingsDialog->clientos());
    settings::save("os-version", settingsDialog->osVersion());
}

void GPClient::on_connectButton_clicked()
{
    doConnect();
}

void GPClient::on_portalInput_returnPressed()
{
    doConnect();
}

void GPClient::on_portalInput_editingFinished()
{
    populateGateways();
}

void GPClient::initSystemTrayIcon()
{
    systemTrayIcon = new QSystemTrayIcon(this);
    contextMenu = new QMenu("GlobalProtect", this);

    gatewaySwitchMenu = new QMenu("Switch Gateway", this);
    gatewaySwitchMenu->setIcon(QIcon::fromTheme("network-workgroup"));
    populateGateways();

    systemTrayIcon->setIcon(QIcon(shieldResource(Shield::NotConnected)));
    systemTrayIcon->setToolTip("GlobalProtect");
    systemTrayIcon->setContextMenu(contextMenu);

    connect(systemTrayIcon, &QSystemTrayIcon::activated, this, &GPClient::onSystemTrayActivated);
    connect(gatewaySwitchMenu, &QMenu::triggered, this, &GPClient::onGatewayChanged);

    openAction = contextMenu->addAction(QIcon::fromTheme("window-new"), "Open", this, &GPClient::activate);
    connectAction = contextMenu->addAction(QIcon::fromTheme("preferences-system-network"), "Connect", this, &GPClient::doConnect);
    contextMenu->addMenu(gatewaySwitchMenu);
    contextMenu->addSeparator();
    clearAction = contextMenu->addAction(QIcon::fromTheme("edit-clear"), "Reset", this, &GPClient::reset);
    quitAction = contextMenu->addAction(QIcon::fromTheme("application-exit"), "Quit", this, &GPClient::quit);

    systemTrayIcon->show();
}

void GPClient::initVpnStatus() {
    int status = vpn->status();

    if (status == 1) {
        updateConnectionStatus(ConnectionState::Connecting);
    } else if (status == 2) {
        // The Tunnel was already up when the client started; its real start time is
        // unknown, so the timer counts from now.
        updateConnectionStatus(ConnectionState::Connected);
    } else if (status == 3) {
        updateConnectionStatus(ConnectionState::Disconnecting);
    } else {
        updateConnectionStatus(ConnectionState::Disconnected);
    }
}

// Refills both Gateway pickers, the tray's Switch Gateway menu and the connected
// view's dropdown, from the same Gateways and current Gateway, so both always agree.
// Called whenever the Portal, its Gateways or the current Gateway change.
void GPClient::populateGateways()
{
    const QList<GPGateway> gateways = allGateways();
    const GPGateway current = currentGateway();
    populateGatewayMenu(gateways, current);
    populateGatewayInput(gateways, current);
}

void GPClient::populateGatewayMenu(const QList<GPGateway> &gateways, const GPGateway &current)
{
    LOGI << "Populating the Switch Gateway menu...";

    gatewaySwitchMenu->clear();

    if (gateways.isEmpty()) {
        gatewaySwitchMenu->addAction("<None>")->setData(-1);
        return;
    }

    const QString currentGatewayName = current.name();
    for (int i = 0; i < gateways.length(); i++) {
        const GPGateway g = gateways.at(i);
        QString iconImage = ":/images/radio_unselected.png";
        if (g.name() == currentGatewayName) {
            iconImage = ":/images/radio_selected.png";
        }
        gatewaySwitchMenu->addAction(QIcon(iconImage), QString("%1 (%2)").arg(g.name(), g.address()))->setData(i);
    }
}

// The connected view's Gateway dropdown; whether it is enabled also depends on the state.
void GPClient::populateGatewayInput(const QList<GPGateway> &gateways, const GPGateway &current)
{
    const GatewayChoice choice = presentGateways(gateways, current, state);

    const QSignalBlocker blocker(ui->gatewayInput);
    ui->gatewayInput->clear();
    for (int i = 0; i < choice.names.size(); ++i) {
        ui->gatewayInput->addItem(choice.names.at(i));
        ui->gatewayInput->setItemData(i, choice.addresses.at(i), Qt::ToolTipRole);
    }
    ui->gatewayInput->setCurrentIndex(choice.currentIndex);
    ui->gatewayInput->setEnabled(choice.enabled);
}

// The only place that sets the status text, the main button and the tray Connect action:
// it applies what the presenter returns for the state.
void GPClient::updateConnectionStatus(ConnectionState newState)
{
    state = newState;
    if (state == ConnectionState::Connected && !tunnelStartedAt.isValid()) {
        tunnelStartedAt = QDateTime::currentDateTime();
    } else if (state == ConnectionState::Disconnected) {
        tunnelStartedAt = QDateTime();
    }

    // The timer runs only while Connected, so a new Tunnel starts at 00:00:00.
    if (state == ConnectionState::Connected) {
        tunnelTimer->start();
    } else {
        tunnelTimer->stop();
    }

    const ConnectionView shown = presentConnection(state, tunnelSeconds());
    const QIcon shieldIcon{ shieldResource(shown.shield) };
    const bool idle = state == ConnectionState::Disconnected;

    ui->statusLabel->setText(shown.statusText);
    // Rendered at the screen's pixel ratio, so the SVG stays sharp on HiDPI screens.
    ui->statusImage->setAlignment(Qt::AlignCenter);
    ui->statusImage->setPixmap(shieldIcon.pixmap(QSize(80, 80)));
    ui->connectButton->setText(shown.buttonText);
    ui->connectButton->setEnabled(shown.buttonEnabled);
    ui->portalInput->setReadOnly(!idle);
    ui->activeForLabel->setText(shown.activeForText);
    ui->connectedPortalValue->setText(portal());
    populateGatewayInput(allGateways(), currentGateway());
    showView(shown.visibleView);

    systemTrayIcon->setIcon(shieldIcon);
    connectAction->setText(shown.buttonText);
    connectAction->setEnabled(shown.buttonEnabled);
    gatewaySwitchMenu->setEnabled(shown.buttonEnabled);
    clearAction->setEnabled(idle);
    resetMenuAction->setEnabled(idle);
}

// Applies an in-progress state, except during a Gateway switch: "Switching Gateway..."
// lasts until the Tunnel is up on the new Gateway, through its login and connection.
void GPClient::updateProgress(ConnectionState newState)
{
    updateConnectionStatus(state == ConnectionState::SwitchingGateway ? state : newState);
}

qint64 GPClient::tunnelSeconds() const
{
    return tunnelStartedAt.isValid() ? tunnelStartedAt.secsTo(QDateTime::currentDateTime()) : 0;
}

void GPClient::onTunnelTimerTick()
{
    ui->activeForLabel->setText(presentConnection(state, tunnelSeconds()).activeForText);
}

// Shows the page of the central area matching the view, and sizes the window to it:
// hidden pages get an Ignored size policy so the stack only measures the current one.
void GPClient::showView(VisibleView view)
{
    QWidget *const current = view == VisibleView::Connected ? ui->connectedView : ui->disconnectedView;
    for (int i = 0; i < ui->viewStack->count(); ++i) {
        QWidget *page = ui->viewStack->widget(i);
        const auto policy = page == current ? QSizePolicy::Preferred : QSizePolicy::Ignored;
        page->setSizePolicy(policy, policy);
    }
    ui->viewStack->setCurrentWidget(current);

    centralWidget()->layout()->activate();
    layout()->activate();
    setFixedHeight(sizeHint().height());
}

void GPClient::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            this->activate();
            break;
        default:
            break;
    }
}

void GPClient::onGatewayChanged(QAction *action)
{
    switchGateway(action->data().toInt());
}

void GPClient::onGatewayInputActivated(int index)
{
    switchGateway(index);
}

// The one Gateway switch path, shared by the tray's Switch Gateway menu and the
// connected view's Gateway dropdown. The index is into allGateways().
void GPClient::switchGateway(int index)
{
    const GatewayPick pick = gatewayPickFor(state);
    if (pick == GatewayPick::Ignore) {
        LOGI << "Busy, ignoring the Gateway switch.";
        return;
    }

    const QList<GPGateway> gateways = allGateways();
    if (index < 0 || index >= gateways.size()) {
        return;
    }

    const auto g = gateways.at(index);

    // If the selected gateway is the same as the current gateway
    if (g.name() == currentGateway().name()) {
        return;
    }

    setCurrentGateway(g);

    if (pick == GatewayPick::Switch) {
        updateConnectionStatus(ConnectionState::SwitchingGateway);
        vpn->disconnect();
        switchTimeout->start();
    }
}

void GPClient::doConnect()
{
    LOGI << "Start connecting...";

    const auto portal = this->portal();

    // Display the main window if portal is empty
    if (portal.isEmpty()) {
        activate();
        return;
    }

    if (state == ConnectionState::Disconnected) {
        settings::save("portal", portal);

        // Login to the previously saved gateway
        if (!currentGateway().name().isEmpty()) {
            LOGI << "Start gateway login using the previously saved gateway...";
            isQuickConnect = true;
            gatewayLogin();
        } else {
            // Perform the portal login
            LOGI << "Start portal login...";
            portalLogin();
        }
    } else if (state == ConnectionState::Connected) {
        LOGI << "Start disconnecting the VPN...";

        updateConnectionStatus(ConnectionState::Disconnecting);
        vpn->disconnect();
    } else {
        // A login, connection or disconnection is already in progress.
        LOGI << "Busy, ignoring the request.";
    }
}

// Login to the portal interface to get the portal config and preferred gateway
void GPClient::portalLogin()
{
    auto *portalAuth = new PortalAuthenticator(portal(), clientos());

    connect(portalAuth, &PortalAuthenticator::success, [this, portalAuth](const PortalConfigResponse response, const QString region) {
        this->onPortalSuccess(response, region);
        portalAuth->deleteLater();
    });
    // Prelogin failed on the portal interface, try to treat the portal as a gateway interface
    connect(portalAuth, &PortalAuthenticator::preloginFailed, [this, portalAuth](const QString msg) {
        this->onPortalPreloginFail(msg);
        portalAuth->deleteLater();
    });
    connect(portalAuth, &PortalAuthenticator::portalConfigFailed, [this, portalAuth](const QString msg) {
        this->onPortalConfigFail(msg);
        portalAuth->deleteLater();
    });
    // Portal login failed
    connect(portalAuth, &PortalAuthenticator::fail, [this, portalAuth](const QString &msg) {
        this->onPortalFail(msg);
        portalAuth->deleteLater();
    });

    updateConnectionStatus(ConnectionState::Authenticating);
    portalAuth->authenticate();
}

void GPClient::onPortalSuccess(const PortalConfigResponse portalConfig, const QString region)
{
    LOGI << "Portal authentication succeeded.";

    // No gateway found in portal configuration
    if (portalConfig.allGateways().size() == 0) {
        LOGI << "No gateway found in portal configuration, treat the portal address as a gateway.";
        tryGatewayLogin();
        return;
    }

    GPGateway gateway = filterPreferredGateway(portalConfig.allGateways(), region);
    setAllGateways(portalConfig.allGateways());
    setCurrentGateway(gateway);
    this->portalConfig = portalConfig;

    gatewayLogin();
}

void GPClient::onPortalPreloginFail(const QString msg)
{
    LOGI << "Portal prelogin failed, treat the portal address as a gateway." << msg;
    tryGatewayLogin();
}

void GPClient::onPortalConfigFail(const QString msg)
{
    LOGI << "Failed to get the portal configuration, " << msg << " Treat the portal address as gateway.";
    tryGatewayLogin();
}

void GPClient::onPortalFail(const QString &msg)
{
    if (!msg.isEmpty()) {
        openMessageBox("Portal authentication failed.", msg);
    }

    updateConnectionStatus(ConnectionState::Disconnected);
}

void GPClient::tryGatewayLogin()
{
    LOGI << "Try to perform login on the the gateway interface...";

    // Treat the portal input as the gateway address
    GPGateway g;
    g.setName(portal());
    g.setAddress(portal());

    QList<GPGateway> gateways;
    gateways.append(g);

    setAllGateways(gateways);
    setCurrentGateway(g);

    gatewayLogin();
}

// Login to the gateway
void GPClient::gatewayLogin()
{
    LOGI << "Performing gateway login...";

    if (currentGateway().address().isEmpty()) {
        LOGE << "No gateway selected (selected: " << settings::get(portal() + "_selectedGateway").toString() << "), cannot login.";
        onGatewayFail("No gateway selected, please reconnect to the portal.");
        return;
    }

    GatewayAuthenticatorParams params = GatewayAuthenticatorParams::fromPortalConfigResponse(portalConfig);
    params.setClientos(clientos());

    GatewayAuthenticator *gatewayAuth;
    gatewayAuth = new GatewayAuthenticator(currentGateway().address(), params);

    connect(gatewayAuth, &GatewayAuthenticator::success, [this, gatewayAuth](const QString &authToken) {
        this->onGatewaySuccess(authToken);
        gatewayAuth->deleteLater();
    });
    connect(gatewayAuth, &GatewayAuthenticator::fail, [this, gatewayAuth](const QString &msg) {
        this->onGatewayFail(msg);
        gatewayAuth->deleteLater();
    });

    updateProgress(ConnectionState::Authenticating);
    gatewayAuth->authenticate();
}

void GPClient::onGatewaySuccess(const QString &authCookie)
{
    LOGI << "Gateway login succeeded, got the cookie " << redact(authCookie);

    isQuickConnect = false;
    QList<QString> gatewayAddresses;
    for (GPGateway &gw : allGateways()) {
      gatewayAddresses.push_back(gw.address());
    }
    vpn->connect(currentGateway().address(), gatewayAddresses, portalConfig.username(), authCookie);
    updateProgress(ConnectionState::Connecting);
}

void GPClient::onGatewayFail(const QString &msg)
{
    // If the quick connect on gateway failed, perform the portal login
    if (isQuickConnect && !msg.isEmpty()) {
        isQuickConnect = false;
        LOGI << "Quick connection failed, trying to portal login...";
        portalLogin();
        return;
    }

    if (!msg.isEmpty()) {
        openMessageBox("Gateway authentication failed.", msg);
    }

    updateConnectionStatus(ConnectionState::Disconnected);
}

void GPClient::activate()
{
    activateWindow();
    showNormal();
}

QString GPClient::portal() const
{
    const QString input = ui->portalInput->text().trimmed();

    if (input.startsWith("http")) {
        return QUrl(input).authority();
    }
    return input;
}

void GPClient::portal(QString p)
{
    ui->portalInput->setText(p);
}

// The Gateways saved for the current Portal only: from its Portal Config, or the
// Portal itself when it was used directly as a Gateway.
QList<GPGateway> GPClient::allGateways() const
{
    return GPGateway::fromJson(settings::get(portal() + "_gateways").toString());
}

void GPClient::setAllGateways(QList<GPGateway> gateways)
{
    LOGI << "Updating all the gateways...";

    settings::save(portal() + "_gateways", GPGateway::serialize(gateways));
    populateGateways();
}

GPGateway GPClient::currentGateway() const
{
    const QString selectedGateway = settings::get(portal() + "_selectedGateway").toString();

    for (auto g : allGateways()) {
        if (g.name() == selectedGateway) {
            return g;
        }
    }
    return GPGateway{};
}

void GPClient::setCurrentGateway(const GPGateway gateway)
{
    LOGI << "Updating the current gateway to " << gateway.name();

    // Do not overwrite the portal input with the gateway address: the saved gateways
    // and the selected gateway are keyed by the portal address.
    settings::save(portal() + "_selectedGateway", gateway.name());
    populateGateways();
}

void GPClient::reset()
{
    settings::clear();
    populateGateways();
    ui->portalInput->clear();
}

void GPClient::quit()
{
    vpn->disconnect();
    QApplication::quit();
}

void GPClient::onVPNConnected()
{
    updateConnectionStatus(ConnectionState::Connected);
}

void GPClient::onVPNDisconnected()
{
    switchTimeout->stop();

    // During a Gateway switch the old Tunnel (the one with a start time) went down:
    // log in to the new Gateway and stay in SwitchingGateway until its Tunnel is up.
    // If the new Tunnel goes down before it came up, the switch ends as a disconnect.
    if (state == ConnectionState::SwitchingGateway && tunnelStartedAt.isValid()) {
        tunnelStartedAt = QDateTime();
        gatewayLogin();
        return;
    }

    updateConnectionStatus(ConnectionState::Disconnected);
}

// The old Tunnel never reported going down during a Gateway switch: show what the
// VPN service reports instead of "Switching Gateway..." forever.
void GPClient::onSwitchTimeout()
{
    if (state == ConnectionState::SwitchingGateway && tunnelStartedAt.isValid()) {
        LOGW << "The Tunnel did not go down for the Gateway switch, reading the VPN status again.";
        initVpnStatus();
    }
}

void GPClient::onVPNError(QString errorMessage)
{
    updateConnectionStatus(ConnectionState::Disconnected);
    openMessageBox("Failed to connect", errorMessage);
}

void GPClient::onVPNLogAvailable(QString log)
{
    LOGI << log;
}
