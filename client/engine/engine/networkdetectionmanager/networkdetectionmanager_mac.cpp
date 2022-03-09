#include "networkdetectionmanager_mac.h"

#include <QRegularExpression>
#include "../networkdetectionmanager/reachabilityevents.h"
#include "utils/macutils.h"
#include "utils/utils.h"
#include "utils/logger.h"

const int typeIdNetworkInterface = qRegisterMetaType<ProtoTypes::NetworkInterface>("ProtoTypes::NetworkInterface");

NetworkDetectionManager_mac::NetworkDetectionManager_mac(QObject *parent, IHelper *helper) : INetworkDetectionManager (parent)
    , helper_(helper)
    , lastWifiAdapterUp_(false)
{
    connect(&ReachAbilityEvents::instance(), SIGNAL(networkStateChanged()), SLOT(onNetworkStateChanged()));
    lastNetworkInterface_ = MacUtils::networkInterfaceByName(MacUtils::getPrimaryNetworkInterface());
    lastNetworkList_ = MacUtils::currentNetworkInterfaces(true);
    lastWifiAdapterUp_ = isWifiAdapterUp(lastNetworkList_);
}

NetworkDetectionManager_mac::~NetworkDetectionManager_mac()
{
}

bool NetworkDetectionManager_mac::isOnline()
{
    QMutexLocker locker(&mutex_);
    return !MacUtils::getPrimaryNetworkInterface().isEmpty();
}

void NetworkDetectionManager_mac::onNetworkStateChanged()
{
    const ProtoTypes::NetworkInterface &networkInterface = MacUtils::networkInterfaceByName(MacUtils::getPrimaryNetworkInterface());
    const ProtoTypes::NetworkInterfaces &networkList = MacUtils::currentNetworkInterfaces(true);
    bool wifiAdapterUp = isWifiAdapterUp(networkList);

    if (!google::protobuf::util::MessageDifferencer::Equals(networkInterface, lastNetworkInterface_))
    {
        if (networkInterface.interface_name() != lastNetworkInterface_.interface_name())
        {
            if (networkInterface.interface_index() == -1)
            {
                qCDebug(LOG_BASIC) << "Primary Adapter down: " << QString::fromStdString(lastNetworkInterface_.interface_name());
                emit primaryAdapterDown(lastNetworkInterface_);
            }
            else if (lastNetworkInterface_.interface_index() == -1)
            {
                qCDebug(LOG_BASIC) << "Primary Adapter up: " << QString::fromStdString(networkInterface.interface_name());
                emit primaryAdapterUp(networkInterface);
            }
            else
            {
                qCDebug(LOG_BASIC) << "Primary Adapter changed: " << QString::fromStdString(networkInterface.interface_name());
                emit primaryAdapterChanged(networkInterface);
            }
        }
        else if (networkInterface.network_or_ssid() != lastNetworkInterface_.network_or_ssid())
        {
            qCDebug(LOG_BASIC) << "Primary Network Changed: "
                               << QString::fromStdString(networkInterface.interface_name())
                               << " : " << QString::fromStdString(networkInterface.network_or_ssid());

            // if network change or adapter down (no comeup)
            if (lastNetworkInterface_.network_or_ssid() != "")
            {
                qCDebug(LOG_BASIC) << "Primary Adapter Network changed or lost";
                emit primaryAdapterNetworkLostOrChanged(networkInterface);
            }
        }
        else
        {
            qCDebug(LOG_BASIC) << "Unidentified interface change";
            // Can happen when changing interfaces
        }

        lastNetworkInterface_ = networkInterface;

        QString strNetworkInterface;
        emit networkChanged(networkInterface.interface_index() != Utils::noNetworkInterface().interface_index(), networkInterface);
    }
    else if (wifiAdapterUp != lastWifiAdapterUp_)
    {
        if (MacUtils::isWifiAdapter(QString::fromStdString(networkInterface.interface_name()))
                || MacUtils::isWifiAdapter(QString::fromStdString(lastNetworkInterface_.interface_name())))
        {
            qCDebug(LOG_BASIC) << "Wifi adapter (primary) up state changed: " << wifiAdapterUp;
            emit wifiAdapterChanged(wifiAdapterUp);
        }
    }
    else if (!google::protobuf::util::MessageDifferencer::Equals(networkList, lastNetworkList_))
    {
        qCDebug(LOG_BASIC) << "Network list changed";
        emit networkListChanged(networkList);
    }

    lastNetworkList_ = networkList;
    lastWifiAdapterUp_ = wifiAdapterUp;
}

bool NetworkDetectionManager_mac::isWifiAdapterUp(const ProtoTypes::NetworkInterfaces &networkList)
{
    for (int i = 0; i < networkList.networks_size(); ++i)
    {
        if (networkList.networks(i).interface_type() == ProtoTypes::NETWORK_INTERFACE_WIFI)
        {
            return true;
        }
    }
    return false;
}


void NetworkDetectionManager_mac::getCurrentNetworkInterface(ProtoTypes::NetworkInterface &networkInterface)
{
    networkInterface = lastNetworkInterface_;
}