#include "enginesettings.h"

#include "legacy_protobuf_support/legacy_protobuf.h"
#include "types/global_consts.h"
#include "utils/languagesutil.h"
#include "utils/logger.h"
#include "utils/simplecrypt.h"

const int typeIdEngineSettings = qRegisterMetaType<types::EngineSettings>("types::EngineSettings");

namespace types {

EngineSettings::EngineSettings() : d(new EngineSettingsData)
{
}

void EngineSettings::saveToSettings()
{
    QByteArray arr;
    {
        QDataStream ds(&arr, QIODevice::WriteOnly);
        ds << magic_;
        ds << versionForSerialization_;
        ds << d->language << d->updateChannel << d->isIgnoreSslErrors << d->isTerminateSockets << d->isAllowLanTraffic <<
              d->firewallSettings << d->connectionSettings << d->apiResolutionSettings << d->proxySettings << d->packetSize <<
              d->macAddrSpoofing << d->dnsPolicy << d->tapAdapter << d->customOvpnConfigsPath << d->isKeepAliveEnabled <<
              d->connectedDnsInfo << d->dnsManager << d->networkPreferredProtocols << d->networkLastKnownGoodProtocols <<
              d->isAntiCensorship;
    }

    QSettings settings;
    SimpleCrypt simpleCrypt(SIMPLE_CRYPT_KEY);
    settings.setValue("engineSettings", simpleCrypt.encryptToString(arr));
    settings.sync();
}

void EngineSettings::loadFromSettings()
{
    bool bLoaded = false;
    SimpleCrypt simpleCrypt(SIMPLE_CRYPT_KEY);

    QSettings settings;
    if (settings.contains("engineSettings"))
    {
        QString str = settings.value("engineSettings", "").toString();
        QByteArray arr = simpleCrypt.decryptToByteArray(str);

        QDataStream ds(&arr, QIODevice::ReadOnly);

        quint32 magic, version;
        ds >> magic;
        if (magic == magic_)
        {
            ds >> version;
            ds >> d->language >> d->updateChannel >> d->isIgnoreSslErrors >> d->isTerminateSockets >> d->isAllowLanTraffic >>
                    d->firewallSettings >> d->connectionSettings >> d->apiResolutionSettings >> d->proxySettings >> d->packetSize >>
                    d->macAddrSpoofing >> d->dnsPolicy >> d->tapAdapter >> d->customOvpnConfigsPath >> d->isKeepAliveEnabled >>
                    d->connectedDnsInfo >> d->dnsManager;
            if (version >= 2)
            {
                ds >> d->networkPreferredProtocols;
            }
            if (version >= 3)
            {
                ds >> d->networkLastKnownGoodProtocols;
            }
            if (version >= 4)
            {
                ds >> d->isAntiCensorship;
            }
            if (ds.status() == QDataStream::Ok)
            {
                bLoaded = true;
            }
        }
    }

    if (!bLoaded && settings.contains("engineSettings2"))
    {
        // try load from legacy protobuf
        // todo remove this code at some point later
        QString str = settings.value("engineSettings2", "").toString();
        QByteArray arr = simpleCrypt.decryptToByteArray(str);
        if (LegacyProtobufSupport::loadEngineSettings(arr, *this))
        {
            bLoaded = true;
        }

        settings.remove("engineSettings2");
    }

    if (!bLoaded)
    {
        qCDebug(LOG_BASIC) << "Could not load engine settings -- resetting to defaults";
        *this = EngineSettings();   // reset to defaults
    }

    if (d->language.isEmpty()) {
        d->language = LanguagesUtil::systemLanguage();
    }
}

QString EngineSettings::language() const
{
    return d->language;
}

void EngineSettings::setLanguage(const QString &lang)
{
    d->language = lang;
}

bool EngineSettings::isIgnoreSslErrors() const
{
    return d->isIgnoreSslErrors;
}

void EngineSettings::setIsIgnoreSslErrors(bool ignore)
{
    d->isIgnoreSslErrors = ignore;
}

bool EngineSettings::isTerminateSockets() const
{
    return d->isTerminateSockets;
}

void EngineSettings::setIsTerminateSockets(bool close)
{
    d->isTerminateSockets = close;
}

bool EngineSettings::isAntiCensorship() const
{
    return d->isAntiCensorship;
}

void EngineSettings::setIsAntiCensorship(bool enable)
{
    d->isAntiCensorship = enable;
}

bool EngineSettings::isAllowLanTraffic() const
{
    return d->isAllowLanTraffic;
}

void EngineSettings::setIsAllowLanTraffic(bool isAllowLanTraffic)
{
    d->isAllowLanTraffic = isAllowLanTraffic;
}

ConnectionSettings EngineSettings::connectionSettingsForNetworkInterface(const QString &networkOrSsid) const
{
    if (!networkOrSsid.isEmpty() &&
        d->networkPreferredProtocols.contains(networkOrSsid) &&
        !d->networkPreferredProtocols[networkOrSsid].isAutomatic())
    {
        return d->networkPreferredProtocols[networkOrSsid];
    } else {
        return d->connectionSettings;
    }
}

const types::FirewallSettings &EngineSettings::firewallSettings() const
{
    return d->firewallSettings;
}

void EngineSettings::setFirewallSettings(const FirewallSettings &fs)
{
    d->firewallSettings = fs;
}

const types::ConnectionSettings &EngineSettings::connectionSettings() const
{
    return d->connectionSettings;
}

void EngineSettings::setConnectionSettings(const ConnectionSettings &cs)
{
    d->connectionSettings = cs;
}

const types::ApiResolutionSettings &EngineSettings::apiResolutionSettings() const
{
    return d->apiResolutionSettings;
}

void EngineSettings::setApiResolutionSettings(const ApiResolutionSettings &drs)
{
    d->apiResolutionSettings = drs;
}

const types::ProxySettings &EngineSettings::proxySettings() const
{
    return d->proxySettings;
}

void EngineSettings::setProxySettings(const ProxySettings &ps)
{
    d->proxySettings = ps;
}

DNS_POLICY_TYPE EngineSettings::dnsPolicy() const
{
    return d->dnsPolicy;
}

void EngineSettings::setDnsPolicy(DNS_POLICY_TYPE policy)
{
    d->dnsPolicy = policy;
}

DNS_MANAGER_TYPE EngineSettings::dnsManager() const
{
    return d->dnsManager;
}

void EngineSettings::setDnsManager(DNS_MANAGER_TYPE dnsManager)
{
    d->dnsManager = dnsManager;
}

const types::MacAddrSpoofing &EngineSettings::macAddrSpoofing() const
{
    return d->macAddrSpoofing;
}

const types::PacketSize &EngineSettings::packetSize() const
{
    return d->packetSize;
}

UPDATE_CHANNEL EngineSettings::updateChannel() const
{
    return d->updateChannel;
}

void EngineSettings::setUpdateChannel(UPDATE_CHANNEL channel)
{
    d->updateChannel = channel;
}

const types::ConnectedDnsInfo &EngineSettings::connectedDnsInfo() const
{
    return d->connectedDnsInfo;
}

void EngineSettings::setConnectedDnsInfo(const ConnectedDnsInfo &info)
{
    d->connectedDnsInfo = info;
}

bool EngineSettings::isUseWintun() const
{
    return d->tapAdapter == WINTUN_ADAPTER;
}

TAP_ADAPTER_TYPE EngineSettings::tapAdapter() const
{
    return d->tapAdapter;
}

void EngineSettings::setTapAdapter(TAP_ADAPTER_TYPE tap)
{
    d->tapAdapter = tap;
}

QString EngineSettings::customOvpnConfigsPath() const
{
    return d->customOvpnConfigsPath;
}

void EngineSettings::setCustomOvpnConfigsPath(const QString &path)
{
    d->customOvpnConfigsPath = path;
}

bool EngineSettings::isKeepAliveEnabled() const
{
    return d->isKeepAliveEnabled;
}

void EngineSettings::setIsKeepAliveEnabled(bool enabled)
{
    d->isKeepAliveEnabled = enabled;
}

void EngineSettings::setMacAddrSpoofing(const MacAddrSpoofing &macAddrSpoofing)
{
    d->macAddrSpoofing = macAddrSpoofing;
}

void EngineSettings::setPacketSize(const PacketSize &packetSize)
{
    d->packetSize = packetSize;
}

const QMap<QString, types::ConnectionSettings> &EngineSettings::networkPreferredProtocols() const
{
    return d->networkPreferredProtocols;
}

void EngineSettings::setNetworkPreferredProtocols(const QMap<QString, types::ConnectionSettings> &preferredProtocols)
{
    d->networkPreferredProtocols = preferredProtocols;
}

const types::Protocol EngineSettings::networkLastKnownGoodProtocol(const QString &network) const
{
    return d->networkLastKnownGoodProtocols[network].first;
}

uint EngineSettings::networkLastKnownGoodPort(const QString &network) const
{
    return d->networkLastKnownGoodProtocols[network].second;
}

void EngineSettings::setNetworkLastKnownGoodProtocolPort(const QString &network, const types::Protocol &protocol, uint port)
{
    d->networkLastKnownGoodProtocols[network] = std::make_pair(protocol, port);
}

void EngineSettings::clearLastKnownGoodProtocols(const QString &network)
{
    if (network.isEmpty()) {
        d->networkLastKnownGoodProtocols.clear();
    } else {
        d->networkLastKnownGoodProtocols.remove(network);
    }
}

bool EngineSettings::operator==(const EngineSettings &other) const
{
    return  other.d->language == d->language &&
            other.d->updateChannel == d->updateChannel &&
            other.d->isIgnoreSslErrors == d->isIgnoreSslErrors &&
            other.d->isTerminateSockets == d->isTerminateSockets &&
            other.d->isAntiCensorship == d->isAntiCensorship &&
            other.d->isAllowLanTraffic == d->isAllowLanTraffic &&
            other.d->firewallSettings == d->firewallSettings &&
            other.d->connectionSettings == d->connectionSettings &&
            other.d->apiResolutionSettings == d->apiResolutionSettings &&
            other.d->proxySettings == d->proxySettings &&
            other.d->packetSize == d->packetSize &&
            other.d->macAddrSpoofing == d->macAddrSpoofing &&
            other.d->dnsPolicy == d->dnsPolicy &&
            other.d->tapAdapter == d->tapAdapter &&
            other.d->customOvpnConfigsPath == d->customOvpnConfigsPath &&
            other.d->isKeepAliveEnabled == d->isKeepAliveEnabled &&
            other.d->connectedDnsInfo == d->connectedDnsInfo &&
            other.d->dnsManager == d->dnsManager &&
            other.d->networkPreferredProtocols == d->networkPreferredProtocols &&
            other.d->networkLastKnownGoodProtocols == d->networkLastKnownGoodProtocols;
}

bool EngineSettings::operator!=(const EngineSettings &other) const
{
    return !(*this == other);
}

QDebug operator<<(QDebug dbg, const EngineSettings &es)
{
    QDebugStateSaver saver(dbg);
    dbg.nospace();
    dbg << "{language:" << es.d->language << "; ";
    dbg << "updateChannel:" << UPDATE_CHANNEL_toString(es.d->updateChannel) << "; ";
    dbg << "isIgnoreSslErrors:" << es.d->isIgnoreSslErrors << "; ";
    dbg << "isTerminateSockets:" << es.d->isTerminateSockets << "; ";
    dbg << "isAntiCensorship:" << es.d->isAntiCensorship << "; ";
    dbg << "isAllowLanTraffic:" << es.d->isAllowLanTraffic << "; ";
    dbg << "firewallSettings: " << es.d->firewallSettings << "; ";
    dbg << "connectionSettings: " << es.d->connectionSettings << "; ";
    dbg << "dnsResolutionSettings: " << es.d->apiResolutionSettings << "; ";
    dbg << "proxySettings: " << es.d->proxySettings << "; ";
    dbg << "packetSize: " << es.d->packetSize << "; ";
    dbg << "macAddrSpoofing: " << es.d->macAddrSpoofing << "; ";
    dbg << "dnsPolicy: " << DNS_POLICY_TYPE_ToString(es.d->dnsPolicy) << "; ";
#ifdef Q_OS_WIN
    dbg << "tapAdapter: " << TAP_ADAPTER_TYPE_toString(es.d->tapAdapter) << "; ";
#endif
    dbg << "customOvpnConfigsPath: " << (es.d->customOvpnConfigsPath.isEmpty() ? "empty" : "settled") << "; ";
    dbg << "isKeepAliveEnabled:" << es.d->isKeepAliveEnabled << "; ";
    dbg << "connectedDnsInfo:" << es.d->connectedDnsInfo << "; ";
    dbg << "dnsManager:" << DNS_MANAGER_TYPE_toString(es.d->dnsManager) << "; ";
    dbg << "networkPreferredProtocols:" << es.d->networkPreferredProtocols << "; ";
    dbg << "networkLastKnownGoodProtocols: {" << es.d->networkPreferredProtocols << "; ";
    for (auto network : es.d->networkLastKnownGoodProtocols.keys()) {
        dbg << network << ": ";
        dbg << es.d->networkLastKnownGoodProtocols[network].first.toLongString() << ":";
        dbg << es.d->networkLastKnownGoodProtocols[network].second << "; ";
    }
    dbg << "}}";
    return dbg;
}

} // types namespace
