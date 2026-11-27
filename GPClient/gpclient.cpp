#include <QtGui/QIcon>
#include <plog/Log.h>

#include "gpclient.h"
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

    // Initialize the context menu of system tray.
    initSystemTrayIcon();
    initVpnStatus();

    // Centre once the height follows the visible view.
    gpclient::helper::moveCenter(this);
}

void GPClient::setupSettings()
{
    settingsButton = new QPushButton(this);
    settingsButton->setIcon(QIcon(":/images/settings_icon.png"));
    settingsButton->setFixedSize(QSize(28, 28));

    QRect rect = this->geometry();
    settingsButton->setGeometry(
                rect.width() - settingsButton->width() - 15,
                15,
                settingsButton->geometry().width(),
                settingsButton->geometry().height()
                );

    connect(settingsButton, &QPushButton::clicked, this, &GPClient::onSettingsButtonClicked);
    connect(settingsDialog, &QDialog::accepted, this, &GPClient::onSettingsAccepted);
}

void GPClient::onSettingsButtonClicked()
{
    settingsDialog->setClientos(settings::get("clientos", "Linux").toString());
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
    populateGatewayMenu();
}

void GPClient::initSystemTrayIcon()
{
    systemTrayIcon = new QSystemTrayIcon(this);
    contextMenu = new QMenu("GlobalProtect", this);

    gatewaySwitchMenu = new QMenu("Switch Gateway", this);
    gatewaySwitchMenu->setIcon(QIcon::fromTheme("network-workgroup"));
    populateGatewayMenu();

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

void GPClient::populateGatewayMenu()
{
    LOGI << "Populating the Switch Gateway menu...";

    const QList<GPGateway> gateways = allGateways();
    gatewaySwitchMenu->clear();

    if (gateways.isEmpty()) {
        gatewaySwitchMenu->addAction("<None>")->setData(-1);
        return;
    }

    const QString currentGatewayName = currentGateway().name();
    for (int i = 0; i < gateways.length(); i++) {
        const GPGateway g = gateways.at(i);
        QString iconImage = ":/images/radio_unselected.png";
        if (g.name() == currentGatewayName) {
            iconImage = ":/images/radio_selected.png";
        }
        gatewaySwitchMenu->addAction(QIcon(iconImage), QString("%1 (%2)").arg(g.name(), g.address()))->setData(i);
    }
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

    const qint64 tunnelSeconds = tunnelStartedAt.isValid()
        ? tunnelStartedAt.secsTo(QDateTime::currentDateTime())
        : 0;
    const ConnectionView shown = presentConnection(state, tunnelSeconds);
    const QIcon shieldIcon{ shieldResource(shown.shield) };
    const bool idle = state == ConnectionState::Disconnected;

    ui->statusLabel->setText(shown.statusText);
    // Rendered at the screen's pixel ratio, so the SVG stays sharp on HiDPI screens.
    ui->statusImage->setAlignment(Qt::AlignCenter);
    ui->statusImage->setPixmap(shieldIcon.pixmap(QSize(80, 80)));
    ui->connectButton->setText(shown.buttonText);
    ui->connectButton->setEnabled(shown.buttonEnabled);
    ui->portalInput->setReadOnly(!idle);
    showView(shown.visibleView);

    systemTrayIcon->setIcon(shieldIcon);
    connectAction->setText(shown.buttonText);
    connectAction->setEnabled(shown.buttonEnabled);
    gatewaySwitchMenu->setEnabled(shown.buttonEnabled);
    clearAction->setEnabled(idle);
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
    const int index = action->data().toInt();

    if (index == -1) {
        return;
    }

    const auto g = allGateways().at(index);

    // If the selected gateway is the same as the current gateway
    if (g.name() == currentGateway().name()) {
        return;
    }

    setCurrentGateway(g);

    if (connected()) {
        updateConnectionStatus(ConnectionState::SwitchingGateway);
        vpn->disconnect();
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
    auto *portalAuth = new PortalAuthenticator(portal(), settings::get("clientos", "Linux").toString());

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
    params.setClientos(settings::get("clientos", "Linux").toString());

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

    updateConnectionStatus(ConnectionState::Authenticating);
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
    updateConnectionStatus(ConnectionState::Connecting);
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

bool GPClient::connected() const
{
    return state == ConnectionState::Connected;
}

QList<GPGateway> GPClient::allGateways() const
{

    QList<GPGateway> gateways;

    for (auto g :settings::get_all("_gateways$") ){

    	gateways.append(GPGateway::fromJson(settings::get(g).toString()));
    }
    return gateways;
}

void GPClient::setAllGateways(QList<GPGateway> gateways)
{
    LOGI << "Updating all the gateways...";

    settings::save(portal() + "_gateways", GPGateway::serialize(gateways));
    populateGatewayMenu();
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
    populateGatewayMenu();
}

void GPClient::reset()
{
    settings::clear();
    populateGatewayMenu();
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
    const bool switchingGateway = state == ConnectionState::SwitchingGateway;
    updateConnectionStatus(ConnectionState::Disconnected);

    if (switchingGateway) {
        gatewayLogin();
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
