#include "staticips.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>

const int typeIdApiInfoStaticIps = qRegisterMetaType<apiinfo::StaticIps>("apiinfo::StaticIps");

namespace apiinfo {

bool StaticIps::initFromJson(QJsonObject &init_obj)
{
    if (!init_obj.contains("static_ips"))
    {
        return false;
    }

    const QJsonArray jsonStaticIps = init_obj["static_ips"].toArray();
    for (const QJsonValue &value : jsonStaticIps)
    {
        StaticIpDescr sid;
        QJsonObject obj = value.toObject();

        if (obj.contains("id") && obj.contains("ip_id") && obj.contains("static_ip") && obj.contains("type") &&
            obj.contains("name") && obj.contains("country_code") && obj.contains("short_name") &&
            obj.contains("server_id") && obj.contains("node"))
        {
            sid.id = obj["id"].toDouble();
            sid.ipId = obj["ip_id"].toDouble();
            sid.staticIp = obj["static_ip"].toString();
            sid.type = obj["type"].toString();
            sid.name = obj["name"].toString();
            sid.countryCode = obj["country_code"].toString().toLower();
            sid.shortName = obj["short_name"].toString();
            sid.serverId = obj["server_id"].toDouble();
            sid.wgIp = obj["wg_ip"].toString();
            sid.wgPubKey = obj["wg_pubkey"].toString();
            sid.ovpnX509 = obj["ovpn_x509"].toString();

            QJsonObject objNode = obj["node"].toObject();
            if (objNode.contains("ip") && objNode.contains("ip2") && objNode.contains("ip3") &&
                objNode.contains("city_name") && objNode.contains("hostname") && objNode.contains("dns_hostname"))
            {
                sid.nodeIPs << objNode["ip"].toString();
                sid.nodeIPs << objNode["ip2"].toString();
                sid.nodeIPs << objNode["ip3"].toString();
                sid.cityName = objNode["city_name"].toString();
                sid.hostname = objNode["hostname"].toString();
                sid.dnsHostname = objNode["dns_hostname"].toString();
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (obj.contains("ports"))
        {
            const QJsonArray jsonPorts = obj["ports"].toArray();
            for (const QJsonValue &valuePort: jsonPorts)
            {
                QJsonObject objPort = valuePort.toObject();
                if (objPort.contains("ext_port") && objPort.contains("int_port"))
                {
                    StaticIpPortDescr sipd;
                    sipd.extPort = objPort["ext_port"].toDouble();
                    sipd.intPort = objPort["int_port"].toDouble();
                    sid.ports << sipd;
                }
                else
                {
                    return false;
                }
            }
        }

        if (!obj.contains("credentials"))
        {
            return false;
        }

        QJsonObject objCredentials = obj["credentials"].toObject();
        if (objCredentials.contains("username") && objCredentials.contains("password"))
        {
            sid.username = objCredentials["username"].toString();
            sid.password = objCredentials["password"].toString();
        }
        else
        {
            return false;
        }

        if (obj.contains("device_name"))
        {
            d->deviceName_ = obj["device_name"].toString();
        }

        d->ips_ << sid;
    }

    return true;
}

QStringList StaticIps::getAllPingIps() const
{
    QStringList ret;
    for (const StaticIpDescr &sid : d->ips_)
    {
        ret << sid.getPingIp();
    }
    return ret;
}

bool StaticIps::operator==(const StaticIps &other) const
{
    return d->deviceName_ == other.d->deviceName_ &&
            d->ips_ == other.d->ips_;
}

bool StaticIps::operator!=(const StaticIps &other) const
{
    return !operator==(other);
}

QString StaticIpPortsVector::getAsStringWithDelimiters() const
{
    QString ret;

    for (int i = 0; i < count(); ++i)
    {
        ret += QString::number(at(i));
        if (i < (count() - 1))
        {
            ret += ",";
        }
    }

    return ret;
}

bool operator==(const StaticIpPortDescr &l, const StaticIpPortDescr &r)
{
    return l.extPort == r.extPort && l.intPort == r.intPort;
}

QDataStream& operator <<(QDataStream& stream, const StaticIps& s)
{
    stream << s.versionForSerialization_;
    stream << s.d->deviceName_ << s.d->ips_;
    return stream;
}

QDataStream& operator >>(QDataStream& stream, StaticIps& s)
{
    quint32 version;
    stream >> version;
    if (version > s.versionForSerialization_)
    {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> s.d->deviceName_ >> s.d->ips_;
    return stream;
}

StaticIpPortsVector StaticIpDescr::getAllStaticIpIntPorts() const
{
    StaticIpPortsVector ret;
    for (const StaticIpPortDescr &portDescr : ports)
    {
        ret << portDescr.intPort;
    }
    return ret;
}

bool StaticIpDescr::operator==(const StaticIpDescr &other) const
{
    return id == other.id &&
           ipId == other.ipId &&
           staticIp == other.staticIp &&
           type == other.type &&
           name == other.name &&
           countryCode == other.countryCode &&
           shortName == other.shortName &&
           cityName == other.cityName &&
           serverId == other.serverId &&
           nodeIPs == other.nodeIPs &&
           hostname == other.hostname &&
           dnsHostname == other.dnsHostname &&
           username == other.username &&
           password == other.password &&
           wgIp == other.wgIp &&
           wgPubKey == other.wgPubKey &&
           ovpnX509 == other.ovpnX509 &&
            ports == other.ports;
}

bool StaticIpDescr::operator!=(const StaticIpDescr &other) const
{
    return !operator==(other);
}

QDataStream& operator <<(QDataStream& stream, const StaticIpDescr& s)
{
    stream << s.versionForSerialization_;
    stream << s.id << s.ipId << s.staticIp << s.type << s.name << s.countryCode << s.shortName << s.cityName << s.serverId << s.nodeIPs
           << s.hostname << s.dnsHostname << s.username << s.password << s.wgIp << s.wgPubKey << s.ovpnX509 << s.ports;
    return stream;
}

QDataStream& operator >>(QDataStream& stream, StaticIpDescr& s)
{
    quint32 version;
    stream >> version;
    if (version > s.versionForSerialization_)
    {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> s.id >> s.ipId >> s.staticIp >> s.type >> s.name >> s.countryCode >> s.shortName >> s.cityName >> s.serverId >> s.nodeIPs
           >> s.hostname >> s.dnsHostname >> s.username >> s.password >> s.wgIp >> s.wgPubKey >> s.ovpnX509 >> s.ports;
    return stream;
}

QDataStream& operator <<(QDataStream& stream, const StaticIpPortDescr& s)
{
    stream << s.versionForSerialization_;
    stream << s.extPort << s.intPort;
    return stream;
}

QDataStream& operator >>(QDataStream& stream, StaticIpPortDescr& s)
{
    quint32 version;
    stream >> version;
    if (version > s.versionForSerialization_)
    {
        stream.setStatus(QDataStream::ReadCorruptData);
        return stream;
    }
    stream >> s.extPort >> s.intPort;
    return stream;
}
} //namespace apiinfo