#ifndef TYPES_STATICIPS_H
#define TYPES_STATICIPS_H

#include <QSharedPointer>
#include <QString>
#include <QVector>

namespace types {

struct StaticIpPortDescr
{
    unsigned int extPort;
    unsigned int intPort;

    friend QDataStream& operator <<(QDataStream& stream, const StaticIpPortDescr& s);
    friend QDataStream& operator >>(QDataStream& stream, StaticIpPortDescr& s);

private:
    static constexpr quint32 versionForSerialization_ = 1;
};

bool operator==(const StaticIpPortDescr &l, const StaticIpPortDescr&r);

class StaticIpPortsVector : public QVector<unsigned int>
{
public:
    QString getAsStringWithDelimiters() const;
};

struct StaticIpDescr
{
    uint id;
    uint ipId;
    QString staticIp;
    QString type;     // dc = datacenter ip, res = residential ip
    QString name;
    QString countryCode;
    QString shortName;
    QString cityName;
    uint serverId;
    QVector<QString> nodeIPs;    // 3 ips
    QString hostname;
    QString dnsHostname;
    QString username;
    QString password;
    QString wgIp;
    QString wgPubKey;
    QString ovpnX509;
    QVector<StaticIpPortDescr> ports;

    const QString& getPingIp() const { Q_ASSERT(!nodeIPs.isEmpty()); return nodeIPs[0]; }

    StaticIpPortsVector getAllStaticIpIntPorts() const;

    bool operator== (const StaticIpDescr &other) const;
    bool operator!= (const StaticIpDescr &other) const;

    friend QDataStream& operator <<(QDataStream& stream, const StaticIpDescr& s);
    friend QDataStream& operator >>(QDataStream& stream, StaticIpDescr& s);

private:
    static constexpr quint32 versionForSerialization_ = 1;
};

// internal data for StaticIps
class StaticIpsData : public QSharedData
{
public:
    StaticIpsData() {}
    StaticIpsData(const StaticIpsData &other)
        : QSharedData(other),
          deviceName_(other.deviceName_),
          ips_(other.ips_) {}
    ~StaticIpsData() {}

    QString deviceName_;
    QVector<StaticIpDescr> ips_;
};


// implicitly shared class StaticIps
class StaticIps
{
public:
    explicit StaticIps() : d(new StaticIpsData) {}
    StaticIps(const StaticIps &other) : d (other.d) {}

    bool initFromJson(QJsonObject &obj);

    const QString &getDeviceName() const { return d->deviceName_; }
    int getIpsCount() const { return d->ips_.count(); }
    const StaticIpDescr &getIp(int ind) const { Q_ASSERT(ind >= 0 && ind < d->ips_.count()); return d->ips_[ind]; }

    QStringList getAllPingIps() const;

    bool operator== (const StaticIps &other) const;
    bool operator!= (const StaticIps &other) const;

    StaticIps& operator=(const StaticIps&) = default;

    friend QDataStream& operator <<(QDataStream& stream, const StaticIps& s);
    friend QDataStream& operator >>(QDataStream& stream, StaticIps& s);

private:
    QSharedDataPointer<StaticIpsData> d;
    static constexpr quint32 versionForSerialization_ = 1;
};

} //namespace types

#endif // TYPES_STATICIPS_H