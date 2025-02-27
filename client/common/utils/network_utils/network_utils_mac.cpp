#include "network_utils_mac.h"
#include "../ws_assert.h"
#include "../utils.h"
#include <QFile>
#include <QProcess>
#include <QDir>
#include <QCoreApplication>
#include <semaphore.h>

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <libproc.h>
#include <QRegExp>

namespace
{
QString ssidOfInterface(const QString &networkInterface)
{
    QString command = "echo 'show State:/Network/Interface/" + networkInterface + "/AirPort' | scutil | grep SSID_STR | sed -e 's/.*SSID_STR : //'";
    QString strReply = Utils::execCmd(command).trimmed();
    return strReply;
}

QString macAddressFromInterfaceName(const QString &interfaceName)
{
    QString command = "ifconfig " + interfaceName + " | grep 'ether' | awk '{print $2}'";
    QString strIP = Utils::execCmd(command).trimmed();
    return strIP;
}

QList<QString> currentNetworkHwInterfaces()
{
    QList<QString> result;
    QString cmd = "networksetup -listnetworkserviceorder | grep Device | sed -e 's/.*Device: //' -e 's/)//'";
    QString response = Utils::execCmd(cmd);

    const QList<QString> lines = response.split("\n");

    for (const QString &line : lines)
    {
        const QString iName = line.trimmed();
        if (iName != "")
        {
            if (NetworkUtils_mac::isAdapterUp(iName))
            {
                result.append(iName);
            }
        }
    }

    return result;
}

bool isAdapterActive(const QString &networkInterface)
{
    QString cmdInterfaceStatus = "ifconfig " + networkInterface + " | grep status | awk '{print $2}'";
    QString statusResult = Utils::execCmd(cmdInterfaceStatus).trimmed();
    return statusResult == "active";
}

QMap<QString, int> currentHardwareInterfaceIndexes()
{
    QMap<QString, int> result;
    QString cmd = "networksetup -listallhardwareports | grep Device | awk '{print $2}'";
    QString response = Utils::execCmd(cmd);

    const QList<QString> lines = response.split("\n");

    int index = 1;
    for (const QString &line : lines)
    {
        const QString iName = line.trimmed();
        if (iName != "")
        {
            result.insert(iName, index);
        }
        index++;
    }

    return result;
}

QVector<types::NetworkInterface> currentlyUpNetInterfaces()
{
    auto isUp = [](const types::NetworkInterface &ni)
    {
        return NetworkUtils_mac::isAdapterUp(ni.interfaceName);
    };

    QVector<types::NetworkInterface> interfaces = NetworkUtils_mac::currentNetworkInterfaces(false);
    QVector<types::NetworkInterface> upInterfaces;
    std::copy_if(interfaces.begin(), interfaces.end(), std::back_inserter(upInterfaces), isUp);
    return upInterfaces;
}

} // namespace

QString NetworkUtils_mac::lastConnectedNetworkInterfaceName()
{
    QString ifname("");

    struct ifaddrs * interfaces = NULL;
    struct ifaddrs * temp_addr = NULL;

    if( getifaddrs(&interfaces) == 0 )
    {
        //Loop through linked list of interfaces
        temp_addr = interfaces;
        while( temp_addr != NULL )
        {
            if( temp_addr->ifa_addr->sa_family == AF_INET )
            {
                QString tname = temp_addr->ifa_name;
                if( tname.startsWith("utun") )
                    ifname = tname;
                else if( tname.startsWith("ipsec") )
                    ifname = tname;
                else if( tname.startsWith("ppp") )
                    ifname = tname;
            }

            temp_addr = temp_addr->ifa_next;
        }

        freeifaddrs(interfaces);
    }
    return ifname;
}

QVector<types::NetworkInterface> NetworkUtils_mac::currentNetworkInterfaces(bool includeNoInterface)
{
    QVector<types::NetworkInterface> networkInterfaces;

    // Add "No Interface" selection
    if (includeNoInterface)
    {
        networkInterfaces << Utils::noNetworkInterface();
    }

    const QList<QString> hwInterfaces = currentNetworkHwInterfaces();
    const QMap<QString, int> interfaceIndexes = currentHardwareInterfaceIndexes();

    for (const QString &interfaceName : hwInterfaces)
    {
        types::NetworkInterface networkInterface;

        int index = 0;
        if (interfaceIndexes.contains(interfaceName)) index = interfaceIndexes[interfaceName];
        networkInterface.interfaceIndex = index;
        networkInterface.interfaceName = interfaceName;

        bool wifi = isWifiAdapter(interfaceName);
        QString macAddress = macAddressFromInterfaceName(interfaceName);
        networkInterface.physicalAddress = macAddress;

        if (wifi)
        {
            networkInterface.interfaceType = NETWORK_INTERFACE_WIFI;
            QString ssid = ssidOfInterface(interfaceName);
            networkInterface.networkOrSsid = ssid;
        }
        else // Eth
        {
            networkInterface.interfaceType = NETWORK_INTERFACE_ETH;
            networkInterface.networkOrSsid = macAddress;
        }

        networkInterface.active = isAdapterActive(interfaceName);
        networkInterfaces << networkInterface;

        // TODO: The following fields should be removeable from types::NetworkInterface:
        // interface_guid
        // state
        // metric
        // dw_type
        // device_name
        // connector_present
        // end_point_interface
        // active ?
    }

    return networkInterfaces;
}

QString NetworkUtils_mac::trueMacAddress(const QString &interfaceName)
{
    QString cmdGetTrueMAC = "networksetup -getmacaddress " + interfaceName + " | awk '{print $3}'";
    return Utils::execCmd(cmdGetTrueMAC).trimmed();
}

QVector<types::NetworkInterface> NetworkUtils_mac::currentSpoofedInterfaces()
{
    QVector<types::NetworkInterface> spoofed;
    QVector<types::NetworkInterface> currentInterfaces = currentlyUpNetInterfaces();
    for (const types::NetworkInterface &ii : currentInterfaces)
    {
        const QString &interfaceName = ii.interfaceName;

        if (isInterfaceSpoofed(interfaceName))
        {
            spoofed << ii;
        }
    }

    return spoofed;
}

bool NetworkUtils_mac::isInterfaceSpoofed(const QString &interfaceName)
{
    return trueMacAddress(interfaceName) != macAddressFromInterfaceName(interfaceName);
}

bool NetworkUtils_mac::isWifiAdapter(const QString &networkInterface)
{
    QString command = "echo 'list' | scutil | grep State:/Network/Interface/" + networkInterface + "/AirPort";
    QString statusResult = Utils::execCmd(command).trimmed();
    return statusResult != "";
}

bool NetworkUtils_mac::isAdapterUp(const QString &networkInterfaceName)
{
    if (isWifiAdapter(networkInterfaceName))
    {
        QString command = "echo 'show State:/Network/Interface/" + networkInterfaceName + "/AirPort' | scutil | grep 'Power Status' | awk '{print $4}'";
        QString result = Utils::execCmd(command).trimmed();
        return result == "TRUE";
    }
    else
    {
        QString command = "echo 'list' | scutil | grep State:/Network/Interface/" + networkInterfaceName + "/IPv4";
        QString result = Utils::execCmd(command).trimmed();
        return result != "";
    }
}

const types::NetworkInterface NetworkUtils_mac::currentNetworkInterface()
{
    QVector<types::NetworkInterface> ni = currentNetworkInterfaces(false);
    if (ni.size() > 0)
    {
        return ni[0];
    }

    return Utils::noNetworkInterface();
}

bool NetworkUtils_mac::pingWithMtu(const QString &url, int mtu)
{
    const QString cmd = QString("ping -c 1 -W 1000 -D -s %1 %2 2> /dev/null").arg(mtu).arg(url);
    QString result = Utils::execCmd(cmd).trimmed();

    if (result.contains("icmp_seq="))
    {
        return true;
    }
    return false;
}

QString NetworkUtils_mac::getLocalIP()
{
    QString command = "ifconfig "
                      "| grep -E \"([0-9]{1,3}\\.){3}[0-9]{1,3}\" "
                      "| grep -vE \"127\\.([0-9]{1,3}\\.){2}[0-9]{1,3}\" "
                      "| awk '{ print $2 }' | cut -f2 -d: | head -n1";
    QString result = Utils::execCmd(command).trimmed();
    return result;
}


// TODO: use system APIs for this rather than relying on flaky and changeable system tools (in terms of their output format)
void NetworkUtils_mac::getDefaultRoute(QString &outGatewayIp, QString &outInterfaceName)
{
    QString command = "netstat -nr -f inet | sed '1,3 d' | awk 'NR==1 { for (i=1; i<=NF; i++) { f[$i] = i  } } NR>1 && $(f[\"Destination\"])==\"default\" { print $(f[\"Gateway\"]), $(f[\"Netif\"]) ; exit }'";
    QString strReply = Utils::execCmd(command);
    QStringList result = strReply.split(' ');
    if (result.count() == 2)
    {
        outGatewayIp = result.first().trimmed();
        outInterfaceName = result.last().trimmed();
    }
    else
    {
        outGatewayIp.clear();
        outInterfaceName.clear();
    }
}

QString NetworkUtils_mac::ipAddressByInterfaceName(const QString &interfaceName)
{
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        return "";
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
        {
            continue;
        }
        int family = ifa->ifa_addr->sa_family;
        QString iname = QString::fromStdString(ifa->ifa_name);

        if (family == AF_INET && iname == interfaceName)
        {
            int s = getnameinfo(ifa->ifa_addr,
                            (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                                  sizeof(struct sockaddr_in6),
                            host, NI_MAXHOST,
                            NULL, 0, NI_NUMERICHOST);
            if (s != 0)
            {
                continue;
            }

            return QString::fromStdString(host);
        }
    }

    freeifaddrs(ifaddr);
    return "";
}

// parse lines, like this:
// resolver #1
//      nameserver[0] : 192.168.1.1
//      if_index : 8 (en0)
QStringList NetworkUtils_mac::getDnsServersForInterface(const QString &interfaceName)
{
    QString command = "scutil --dns";
    QStringList dnsList;

    FILE *file = popen(command.toStdString().c_str(), "r");
    if (file)
    {
        char szLine[4096];
        while (fgets(szLine, sizeof(szLine), file) != 0)
        {
            QString line = QString::fromStdString(szLine);
            // parse line: "nameserver[0] : 192.168.1.1"
            if (line.contains("nameserver", Qt::CaseInsensitive))
            {
                QStringList strs = line.split(':');
                if (strs.count() == 2)
                {
                    QString dnsIp = strs[1].trimmed();
                    // get next line and parse:
                    // example:
                    //      if_index : 8 (en0)
                    if (fgets(szLine, sizeof(szLine), file) != 0)
                    {
                        QString line2 = QString::fromStdString(szLine);
                        if (line2.contains("if_index", Qt::CaseInsensitive) && line2.contains(interfaceName))
                        {
                            dnsList << dnsIp;
                        }
                    }
                }
            }
        }
        pclose(file);
    }

    dnsList.removeDuplicates();
    return dnsList;
}

QStringList NetworkUtils_mac::getListOfDnsNetworkServiceEntries()
{
    QStringList result;
    QString command = "echo 'list' | scutil | grep /Network/Service | grep DNS";
    QString cmdOutput = Utils::execCmd(command).trimmed();
    // qDebug() << "Raw result: " << cmdOutput;

    QStringList lines = cmdOutput.split('\n');
    for (QString line : lines)
    {
        if (line.contains("="))
        {
            QString entry = line.mid(line.indexOf("=")+1).trimmed();
            result.append(entry);
        }
    }
    return result;
}


bool NetworkUtils_mac::checkMacAddr(const QString &interfaceName, const QString &macAddr)
{
    return macAddressFromInterfaceName(interfaceName).toUpper().remove(':') == macAddr;
}

QStringList NetworkUtils_mac::getP2P_AWDL_NetworkInterfaces()
{
    QString res = Utils::execCmd("ifconfig -l -u").trimmed();
    QStringList list = res.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    QStringList ret;
    for (const auto &s : list)
        if (s.startsWith("p2p") || s.startsWith("awdl"))
            ret << s;
    return ret;
}
