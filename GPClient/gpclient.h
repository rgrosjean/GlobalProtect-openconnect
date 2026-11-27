#ifndef GPCLIENT_H
#define GPCLIENT_H

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QPushButton>

#include "connectionview.h"
#include "portalconfigresponse.h"
#include "settingsdialog.h"
#include "vpn.h"
#include "gatewayauthenticator.h"

QT_BEGIN_NAMESPACE
namespace Ui { class GPClient; }
QT_END_NAMESPACE

class GPClient : public QMainWindow
{
    Q_OBJECT

public:
    GPClient(QWidget *parent, IVpn *vpn);

    void activate();
    void quit();

    QString portal() const;
    void portal(QString);

    GPGateway currentGateway() const;
    void setCurrentGateway(const GPGateway gateway);

    void doConnect();
    void reset();

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void onSettingsButtonClicked();
    void showAbout();
    void onSettingsAccepted();

    void on_connectButton_clicked();
    void on_portalInput_returnPressed();
    void on_portalInput_editingFinished();

    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onGatewayChanged(QAction *action);
    void onGatewayInputActivated(int index);
    void onTunnelTimerTick();
    void onSwitchTimeout();

    void onPortalSuccess(const PortalConfigResponse portalConfig, const QString region);
    void onPortalPreloginFail(const QString msg);
    void onPortalConfigFail(const QString msg);
    void onPortalFail(const QString &msg);

    void onGatewaySuccess(const QString &authCookie);
    void onGatewayFail(const QString &msg);

    void onVPNConnected();
    void onVPNDisconnected();
    void onVPNError(QString errorMessage);
    void onVPNLogAvailable(QString log);

private:
    Ui::GPClient *ui;
    IVpn *vpn;

    QSystemTrayIcon *systemTrayIcon;
    QMenu *contextMenu;
    QAction *openAction;
    QAction *connectAction;

    QMenu *gatewaySwitchMenu;
    QAction *clearAction;
    QAction *quitAction;

    SettingsDialog *settingsDialog;
    QToolButton *moreButton { nullptr };
    QMenu *moreMenu;
    QAction *resetMenuAction;

    ConnectionState state { ConnectionState::Disconnected };
    // When the current Tunnel came up; invalid while no Tunnel is up (including the
    // part of a Gateway switch after the old Tunnel went down).
    QDateTime tunnelStartedAt;
    QTimer *tunnelTimer;  // refreshes the "Active for" text once per second while Connected
    QTimer *switchTimeout;  // ends a Gateway switch whose old Tunnel never reports going down

    bool isQuickConnect { false };
    PortalConfigResponse portalConfig;

    void setupSettings();
    void setupMenu();
    void updateMenuIcon();

    void initSystemTrayIcon();
    void initVpnStatus();
    void populateGatewayMenu();
    void populateGatewayInput();
    void updateConnectionStatus(ConnectionState newState);
    void updateProgress(ConnectionState newState);
    qint64 tunnelSeconds() const;
    void switchGateway(int index);
    void showView(VisibleView view);

    void portalLogin();
    void tryGatewayLogin();
    void gatewayLogin();

    QList<GPGateway> allGateways() const;
    void setAllGateways(QList<GPGateway> gateways);
};
#endif // GPCLIENT_H
