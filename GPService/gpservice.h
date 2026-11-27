#ifndef GLOBALPROTECTSERVICE_H
#define GLOBALPROTECTSERVICE_H

#include <QtCore/QObject>
#include <QtCore/QProcess>

static QList<QString> binaryPaths = QList<QString>() <<
    "/usr/local/bin/openconnect" <<
     "/usr/local/sbin/openconnect" <<
     "/usr/bin/openconnect" <<
     "/usr/sbin/openconnect" <<
     "/opt/bin/openconnect" <<
     "/opt/sbin/openconnect";

class GPService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.yuezk.qt.GPService")
public:
    explicit GPService(QObject *parent = nullptr);
    ~GPService();

    void quit();

    enum VpnStatus {
        VpnNotConnected,
        VpnConnecting,
        VpnConnected,
        VpnDisconnecting,
        VpnReconnecting,  // the Tunnel was lost and openconnect is trying to get it back
    };

signals:
    void connected();      // the Tunnel is up, first or again after reconnecting()
    void disconnected();   // the openconnect process exited
    void reconnecting();   // the Tunnel was lost; openconnect retries until it gives up
    void error(QString errorMessage);
    void logAvailable(QString log);

public slots:
    void connect(QString server, QString username, QString passwd);
    void disconnect();
    int status();

private slots:
    void onProcessStarted();
    void onProcessError(QProcess::ProcessError error);
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *openconnect;
    bool aboutToQuit = false;
    int vpnStatus = GPService::VpnNotConnected;

    void log(QString msg);
    void applyTunnelEvent(const QString &line);
    bool isValidVersion(QString &bin);
    static QString findBinary();
    static QString extraOpenconnectArgs(const QString &gateway);
    static QStringList splitCommand(const QString &command);
};

#endif // GLOBALPROTECTSERVICE_H
