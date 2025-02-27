#ifndef CONNECTIONMANAGER_H
#define CONNECTIONMANAGER_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include "stunnelmanager.h"
#include "wstunnelmanager.h"
#include "ctrldmanager/ictrldmanager.h"
#include "makeovpnfile.h"
#include "makeovpnfilefromcustom.h"

#include "iconnection.h"
#include "testvpntunnel.h"
#include "engine/wireguardconfig/wireguardconfig.h"
#include "engine/wireguardconfig/getwireguardconfig.h"
#include "connsettingspolicy/baseconnsettingspolicy.h"
#include "engine/customconfigs/customovpnauthcredentialsstorage.h"
#include "engine/apiinfo/servercredentials.h"
#include "engine/locationsmodel/baselocationinfo.h"
#include "types/connectionsettings.h"
#include "types/packetsize.h"
#include "types/protocol.h"
#include "types/connecteddnsinfo.h"

#ifdef Q_OS_MAC
    #include "restorednsmanager_mac.h"
#endif

class INetworkDetectionManager;
class ISleepEvents;
class IKEv2Connection;

// manage openvpn connection, reconnects, sleep mode, network change, automatic/manual connection mode

class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(QObject *parent, IHelper *helper, INetworkDetectionManager *networkDetectionManager,
                               server_api::ServerAPI *serverAPI, CustomOvpnAuthCredentialsStorage *customOvpnAuthCredentialsStorage);
    ~ConnectionManager() override;

    void clickConnect(const QString &ovpnConfig, const apiinfo::ServerCredentials &serverCredentials,
                      QSharedPointer<locationsmodel::BaseLocationInfo> bli,
                      const types::ConnectionSettings &connectionSettings,
                      const types::PortMap &portMap, const types::ProxySettings &proxySettings,
                      bool bEmitAuthError, const QString &customConfigPath);

    void clickDisconnect();
    void blockingDisconnect();
    bool isDisconnected();

    QString udpStuffingWithNtp(const QString &ip, const quint16 port);

    QString getLastConnectedIp();
    const AdapterGatewayInfo &getDefaultAdapterInfo() const;
    const AdapterGatewayInfo &getVpnAdapterInfo() const;

    void setConnectedDnsInfo(const types::ConnectedDnsInfo &info);
    const types::ConnectedDnsInfo &connectedDnsInfo() const;

    void removeIkev2ConnectionFromOS();

    void continueWithUsernameAndPassword(const QString &username, const QString &password, bool bNeedReconnect);
    void continueWithPassword(const QString &password, bool bNeedReconnect);

    bool isCustomOvpnConfigCurrentConnection() const;
    QString getCustomOvpnConfigFileName();

    bool isStaticIpsLocation() const;
    apiinfo::StaticIpPortsVector getStatisIps();

    void onWireGuardKeyLimitUserResponse(bool deleteOldestKey);

    void setMss(int mss);
    void setPacketSize(types::PacketSize ps);

    void startTunnelTests();
    bool isAllowFirewallAfterConnection() const;

    types::Protocol currentProtocol() const;

    void updateConnectionSettings(
        const types::ConnectionSettings &connectionSettings,
        const types::PortMap &portMap,
        const types::ProxySettings &proxySettings);

    void setLastKnownGoodProtocol(const types::Protocol protocol);

signals:
    void connected();
    void connectingToHostname(const QString &hostname, const QString &ip, const QStringList &dnsServers);
    void disconnected(DISCONNECT_REASON reason);
    void errorDuringConnection(CONNECT_ERROR errorCode);
    void reconnecting();
    void statisticsUpdated(quint64 bytesIn, quint64 bytesOut, bool isTotalBytes);
    void interfaceUpdated(const QString &interfaceName);  // WireGuard-specific.
    void testTunnelResult(bool success, const QString &ipAddress);
    void showFailedAutomaticConnectionMessage();
    void internetConnectivityChanged(bool connectivity);
    void protocolPortChanged(const types::Protocol &protocol, const uint port);
    void wireGuardAtKeyLimit();
    void protocolStatusChanged(const QVector<types::ProtocolStatus> &status);

    void requestUsername(const QString &pathCustomOvpnConfig);
    void requestPassword(const QString &pathCustomOvpnConfig);

private slots:
    void onConnectionConnected(const AdapterGatewayInfo &connectionAdapterInfo);
    void onConnectionDisconnected();
    void onConnectionReconnecting();
    void onConnectionError(CONNECT_ERROR err);
    void onConnectionStatisticsUpdated(quint64 bytesIn, quint64 bytesOut, bool isTotalBytes);
    void onConnectionInterfaceUpdated(const QString &interfaceName);

    void onConnectionRequestUsername();
    void onConnectionRequestPassword();

    void onSleepMode();
    void onWakeMode();

    void onNetworkOnlineStateChanged(bool isAlive);

    void onTimerReconnection();
    void onConnectTrigger();
    void onConnectingTimeout();

    void onWstunnelStarted();
    void onTunnelTestsFinished(bool bSuccess, const QString &ipAddress);

    void onTimerWaitNetworkConnectivity();

    void onHostnamesResolved();

    void onGetWireGuardConfigAnswer(WireGuardConfigRetCode retCode, const WireGuardConfig &config);

private:
    enum {STATE_DISCONNECTED, STATE_CONNECTING_FROM_USER_CLICK, STATE_CONNECTED, STATE_RECONNECTING,
          STATE_DISCONNECTING_FROM_USER_CLICK, STATE_WAIT_FOR_NETWORK_CONNECTIVITY, STATE_RECONNECTION_TIME_EXCEED,
          STATE_SLEEP_MODE_NEED_RECONNECT, STATE_WAKEUP_RECONNECTING, STATE_AUTO_DISCONNECT, STATE_ERROR_DURING_CONNECTION};

    IHelper *helper_;
    INetworkDetectionManager *networkDetectionManager_;
    CustomOvpnAuthCredentialsStorage *customOvpnAuthCredentialsStorage_;

    IConnection *connector_;
    ISleepEvents *sleepEvents_;
    StunnelManager *stunnelManager_;
    WstunnelManager *wstunnelManager_;
    ICtrldManager *ctrldManager_;

#ifdef Q_OS_MAC
    RestoreDNSManager_mac restoreDnsManager_;
#endif

    QScopedPointer<BaseConnSettingsPolicy> connSettingsPolicy_;

    QString lastIp_;

    QString lastOvpnConfig_;
    apiinfo::ServerCredentials lastServerCredentials_;
    types::ProxySettings lastProxySettings_;
    bool bEmitAuthError_;

    QString customConfigPath_;

    QTimer timerWaitNetworkConnectivity_;

    MakeOVPNFile *makeOVPNFile_;
    MakeOVPNFileFromCustom *makeOVPNFileFromCustom_;
    TestVPNTunnel *testVPNTunnel_;

    bool bNeedResetTap_;
    bool bIgnoreConnectionErrorsForOpenVpn_;
    bool bWasSuccessfullyConnectionAttempt_;
    CONNECT_ERROR latestConnectionError_;

    QTimer timerReconnection_;
    enum { MAX_RECONNECTION_TIME = 60 * 60 * 1000 };  // 1 hour

    // this timer is used to 'rest' between protocol failovers
    QTimer connectTimer_;
    static constexpr int kConnectionWaitTimeMsec = 10 * 1000;

    // this timer is used to cap the login attempt time
    QTimer connectingTimer_;
    static constexpr int kConnectingTimeoutWireGuard = 20 * 1000;
    static constexpr int kConnectingTimeout = 30 * 1000;

    int state_;
    bool bLastIsOnline_;
    bool bWakeSignalReceived_;

    types::Protocol currentProtocol_;

    CurrentConnectionDescr currentConnectionDescr_;

    QString usernameForCustomOvpn_;     // can be empty
    QString passwordForCustomOvpn_;     // can be empty

    types::PacketSize packetSize_;

    WireGuardConfig wireGuardConfig_;
    GetWireGuardConfig *getWireGuardConfig_;

    AdapterGatewayInfo defaultAdapterInfo_;
    AdapterGatewayInfo vpnAdapterInfo_;

    types::ConnectedDnsInfo connectedDnsInfo_;

    QSharedPointer<locationsmodel::BaseLocationInfo> bli_;

    types::Protocol lastKnownGoodProtocol_;

    void doConnect();
    void doConnectPart2();
    void doConnectPart3();
    bool checkFails();

    void doMacRestoreProcedures();
    void startReconnectionTimer();
    void waitForNetworkConnectivity();
    void recreateConnector(types::Protocol protocol);
    void restoreConnectionAfterWakeUp();
    void updateConnectionSettingsPolicy(
        const types::ConnectionSettings &connectionSettings,
        const types::PortMap &portMap,
        const types::ProxySettings &proxySettings);
    void connectOrStartConnectTimer();
};

#endif // CONNECTIONMANAGER_H
