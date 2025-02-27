#include "networkdetectionmanager_linux.h"

#include <QRegularExpression>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <linux/wireless.h>

#include "utils/logger.h"
#include "utils/utils.h"

const int typeIdNetworkInterface = qRegisterMetaType<types::NetworkInterface>("types::NetworkInterface");

NetworkDetectionManager_linux::NetworkDetectionManager_linux(QObject *parent, IHelper *helper) : INetworkDetectionManager(parent)
{
    Q_UNUSED(helper);

    networkInterface_ = Utils::noNetworkInterface();
    getDefaultRouteInterface(isOnline_);
    updateNetworkInfo(false);

    routeMonitorThread_ = new QThread;
    routeMonitor_ = new RouteMonitor_linux;
    connect(routeMonitor_, &RouteMonitor_linux::routesChanged, this, &NetworkDetectionManager_linux::onRoutesChanged);
    connect(routeMonitorThread_, &QThread::started, routeMonitor_, &RouteMonitor_linux::init);
    connect(routeMonitorThread_, &QThread::finished, routeMonitor_, &RouteMonitor_linux::finish);
    connect(routeMonitorThread_, &QThread::finished, routeMonitor_, &RouteMonitor_linux::deleteLater);
    routeMonitor_->moveToThread(routeMonitorThread_);
    routeMonitorThread_->start(QThread::LowPriority);
}

NetworkDetectionManager_linux::~NetworkDetectionManager_linux()
{
    if (routeMonitorThread_) {
        routeMonitorThread_->quit();
        routeMonitorThread_->wait();
        routeMonitorThread_->deleteLater();
    }
}

void NetworkDetectionManager_linux::getCurrentNetworkInterface(types::NetworkInterface &networkInterface)
{
    networkInterface = networkInterface_;
}

bool NetworkDetectionManager_linux::isOnline()
{
    return isOnline_;
}

void NetworkDetectionManager_linux::onRoutesChanged()
{
    updateNetworkInfo(true);
}

void NetworkDetectionManager_linux::updateNetworkInfo(bool bWithEmitSignal)
{
    bool newIsOnline;
    QString ifname = getDefaultRouteInterface(newIsOnline);

    if (isOnline_ != newIsOnline)
    {
        isOnline_ = newIsOnline;
        emit onlineStateChanged(isOnline_);
    }


    types::NetworkInterface newNetworkInterface = Utils::noNetworkInterface();
    if (!ifname.isEmpty())
    {
        getInterfacePars(ifname, newNetworkInterface);
    }

    if (newNetworkInterface != networkInterface_)
    {
        networkInterface_ = newNetworkInterface;
        if (bWithEmitSignal)
        {
            emit networkChanged(networkInterface_);
        }
    }
}

QString NetworkDetectionManager_linux::getDefaultRouteInterface(bool &isOnline)
{
    QString strReply;
    FILE *file = popen("/sbin/route -n | grep '^0\\.0\\.0\\.0'", "r");
    if (file)
    {
        char szLine[4096];
        while(fgets(szLine, sizeof(szLine), file) != 0)
        {
            strReply += szLine;
        }
        pclose(file);
    }

    const QStringList lines = strReply.split('\n', Qt::SkipEmptyParts);

    isOnline = !lines.isEmpty();

    for (auto &it : lines)
    {
        const QStringList pars = it.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (pars.size() == 8)
        {
            if (!pars[7].startsWith("tun") && !pars[7].startsWith("utun"))
            {
                return pars[7];
            }
        }
        else
        {
            qDebug(LOG_BASIC) << "NetworkDetectionManager_linux::getDefaultRouteInterface parse error";
            return QString();
        }
    }
    return QString();
}

void NetworkDetectionManager_linux::getInterfacePars(const QString &ifname, types::NetworkInterface &outNetworkInterface)
{
    outNetworkInterface.interfaceName = ifname;
    outNetworkInterface.interfaceIndex = if_nametoindex(ifname.toStdString().c_str());
    QString macAddress = getMacAddressByIfName(ifname);
    outNetworkInterface.physicalAddress = macAddress;

    bool isWifi = checkWirelessByIfName(ifname);
    if (isWifi)
    {
        outNetworkInterface.interfaceType = NETWORK_INTERFACE_WIFI;
        QString friendlyName = getFriendlyNameByIfName(ifname);
        if (!friendlyName.isEmpty())
        {
            outNetworkInterface.networkOrSsid = friendlyName;
        }
        else
        {
            outNetworkInterface.networkOrSsid = macAddress;
        }
    }
    else
    {
        outNetworkInterface.interfaceType = NETWORK_INTERFACE_ETH;
        QString friendlyName = getFriendlyNameByIfName(ifname);
        if (!friendlyName.isEmpty())
        {
            outNetworkInterface.networkOrSsid = friendlyName;
        }
        else
        {
            outNetworkInterface.networkOrSsid = macAddress;
        }
    }

    outNetworkInterface.active = isActiveByIfName(ifname);
}

QString NetworkDetectionManager_linux::getMacAddressByIfName(const QString &ifname)
{
    QString ret;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd != -1)
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name , ifname.toStdString().c_str() , IFNAMSIZ-1);
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0)
        {
           unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
           // format mac address
           ret = QString::asprintf("%.2X:%.2X:%.2X:%.2X:%.2X:%.2X" , mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
       }
       close(fd);
    }
    return ret;
}

bool NetworkDetectionManager_linux::isActiveByIfName(const QString &ifname)
{
    bool ret = false;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd != -1)
    {
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        ifr.ifr_addr.sa_family = AF_INET;
        strncpy(ifr.ifr_name , ifname.toStdString().c_str() , IFNAMSIZ-1);
        if (ioctl(fd, SIOCGIFFLAGS, &ifr) == 0)
        {
           ret = (ifr.ifr_flags & ( IFF_UP | IFF_RUNNING )) == ( IFF_UP | IFF_RUNNING );
        }
        close(fd);
    }
    return ret;
}

bool NetworkDetectionManager_linux::checkWirelessByIfName(const QString &ifname)
{
    bool ret = false;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd != -1)
    {
        struct iwreq pwrq;
        memset(&pwrq, 0, sizeof(pwrq));
        strncpy(pwrq.ifr_name, ifname.toStdString().c_str(), IFNAMSIZ-1);
        if (ioctl(fd, SIOCGIWNAME, &pwrq) != -1)
        {
            ret = true;
        }
        close(fd);
    }
    return ret;
}

QString NetworkDetectionManager_linux::getFriendlyNameByIfName(const QString &ifname)
{
    QString strReply;
    FILE *file = popen("nmcli -t -f NAME,DEVICE c show", "r");
    if (file)
    {
        char szLine[4096];
        while(fgets(szLine, sizeof(szLine), file) != 0)
        {
            strReply += szLine;
        }
        pclose(file);
    }

    const QStringList lines = strReply.split('\n', Qt::SkipEmptyParts);
    for (auto &it : lines)
    {
        const QStringList pars = it.split(':', Qt::SkipEmptyParts);
        if (pars.size() == 2)
        {
            if (pars[1] == ifname)
            {
                return pars[0];
            }
        }
    }
    return QString();
}

