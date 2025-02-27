#include "preferenceshelper.h"
#include "utils/languagesutil.h"
#include "version/appversion.h"

PreferencesHelper::PreferencesHelper(QObject *parent) : QObject(parent),
    isWifiSharingSupported_(true), bIpv6StateInOS_(true), isFirewallBlocked_(false),
    isDockedToTray_(false), isExternalConfigMode_(false)
{
    availableLanguageCodes_ << "ar" << "cs" << "de" << "en" << "es" << "fr" << "hi" << "ru" << "zh";
}

QString PreferencesHelper::buildVersion()
{
    return AppVersion::instance().fullVersionString();
}

QList<QPair<QString, QVariant>> PreferencesHelper::availableLanguages() const
{
    QList<QPair<QString, QVariant>> pairs;
    for (const QString &lang : availableLanguageCodes_) {
        pairs << QPair<QString, QString>(LanguagesUtil::convertCodeToNative(lang), lang);
    }

    return pairs;
}

void PreferencesHelper::setProxyGatewayAddress(const QString &address)
{
    if (address != proxyGatewayAddress_)
    {
        proxyGatewayAddress_ = address;
        emit proxyGatewayAddressChanged(proxyGatewayAddress_);
    }
}

QString PreferencesHelper::getProxyGatewayAddress() const
{
    return proxyGatewayAddress_;
}

QVector<TAP_ADAPTER_TYPE> PreferencesHelper::getAvailableTapAdapters(const QString & /*openVpnVersion*/)
{
    return QVector<TAP_ADAPTER_TYPE>() << TAP_ADAPTER << WINTUN_ADAPTER;
}

void PreferencesHelper::setPortMap(const types::PortMap &portMap)
{
    portMap_ = portMap;
    emit portMapChanged();
}

QVector<types::Protocol> PreferencesHelper::getAvailableProtocols()
{
    QVector<types::Protocol> p;
    for (auto it : portMap_.const_items())
        p << it.protocol;
    return p;
}

QVector<uint> PreferencesHelper::getAvailablePortsForProtocol(types::Protocol protocol)
{
    QVector<uint> v;
    for (auto it : portMap_.const_items())
    {
        if (it.protocol == protocol)
        {
            for (int p = 0; p < it.ports.size(); ++p)
            {
                v << it.ports[p];
            }
        }
    }

    return v;
}

void PreferencesHelper::setWifiSharingSupported(bool bSupported)
{
    if (bSupported != isWifiSharingSupported_)
    {
        isWifiSharingSupported_ = bSupported;
        emit wifiSharingSupportedChanged(isWifiSharingSupported_);
    }
}

bool PreferencesHelper::isWifiSharingSupported() const
{
    return isWifiSharingSupported_;
}

void PreferencesHelper::setBlockFirewall(bool b)
{
    if (isFirewallBlocked_ != b)
    {
        isFirewallBlocked_ = b;
        emit isFirewallBlockedChanged(isFirewallBlocked_);
    }
}

bool PreferencesHelper::isFirewallBlocked() const
{
    return isFirewallBlocked_;
}

void PreferencesHelper::setIsDockedToTray(bool b)
{
    if (isDockedToTray_ != b)
    {
        isDockedToTray_ = b;
        emit isDockedModeChanged(isDockedToTray_);
    }
}

bool PreferencesHelper::isDockedToTray() const
{
    return isDockedToTray_;
}

void PreferencesHelper::setIsExternalConfigMode(bool b)
{
    if (isExternalConfigMode_ != b)
    {
        isExternalConfigMode_ = b;
        emit isExternalConfigModeChanged(isExternalConfigMode_);
    }
}

bool PreferencesHelper::isExternalConfigMode() const
{
    return isExternalConfigMode_;
}

#ifdef Q_OS_WIN
void PreferencesHelper::setIpv6StateInOS(bool bEnabled)
{
    if (bIpv6StateInOS_ != bEnabled)
    {
        bIpv6StateInOS_ = bEnabled;
        emit ipv6StateInOSChanged(bIpv6StateInOS_);
    }
}

bool PreferencesHelper::getIpv6StateInOS() const
{
    return bIpv6StateInOS_;
}
#endif
