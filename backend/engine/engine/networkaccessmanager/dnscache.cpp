#include "dnscache.h"
#include <QDateTime>
#include <QTimer>
#include "utils/ipvalidation.h"
#include "dnsresolver/dnsrequest.h"

class DnsCache::Usages
{
public:
    void addUsage(const QString &hostname, quint64 id)
    {
        map_.insert(hostname, id);
    }

    void deleteUsage(quint64 id)
    {
        auto it = map_.begin();
        while (it != map_.end())
        {
            if (it.value() == id)
            {
                it = map_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool isHostnameUsed(const QString &hostname) const
    {
        return !map_.values(hostname).isEmpty();
    }

private:
    QMap<QString, quint64> map_;
};

DnsCache::DnsCache(QObject *parent, int cacheTimeoutMs /*= 60000*/, int reviewCacheIntervalMs /*= 1000*/) : QObject(parent), usages_(new Usages),
    cacheTimeoutMs_(cacheTimeoutMs)
{
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(onTimer()));
    timer->start(reviewCacheIntervalMs);
}

void DnsCache::resolve(const QString &hostname, quint64 id, bool bypassCache /*= false*/, const QStringList &dnsServers /*= QStringList()*/, int timeoutMs /*= 5000*/)
{
    usages_->addUsage(hostname, id);

    if (!bypassCache)
    {
        auto it = cache_.find(hostname);
        if (it != cache_.end())
        {
            emit resolved(true, it.value().ips, id, true, 0);
            return;
        }
    }

    DnsRequest *dnsRequest = new DnsRequest(this, hostname, dnsServers, timeoutMs);
    dnsRequest->setProperty("requestId", id);
    dnsRequest->setProperty("startTime", QDateTime::currentMSecsSinceEpoch());
    connect(dnsRequest, SIGNAL(finished()), SLOT(onDnsRequestFinished()));
    dnsRequest->lookup();
}

void DnsCache::notifyFinished(quint64 id)
{
    usages_->deleteUsage(id);
    QTimer::singleShot(0, this, SLOT(checkForNewIps()));
}

const QSet<QString> &DnsCache::whitelistIps() const
{
    return lastWhitelistIps_;
}

void DnsCache::onDnsRequestFinished()
{
    DnsRequest *dnsRequest = qobject_cast<DnsRequest *>(sender());
    Q_ASSERT(dnsRequest != nullptr);

    bool bOk;
    quint64 requestId = dnsRequest->property("requestId").toULongLong(&bOk);
    Q_ASSERT(bOk);

    qint64 startTimeMs = dnsRequest->property("startTime").toLongLong(&bOk);
    Q_ASSERT(bOk);
    qint64 timeRequest = QDateTime::currentMSecsSinceEpoch() - startTimeMs;
    Q_ASSERT(timeRequest >= 0);

    bool bSuccess = false;
    if (!dnsRequest->isError())
    {
        cache_[dnsRequest->hostname()].ips = dnsRequest->ips();
        cache_[dnsRequest->hostname()].time = QDateTime::currentMSecsSinceEpoch();
        bSuccess = true;

        checkForNewIps();
    }

    emit resolved(bSuccess, dnsRequest->ips(), requestId, false, timeRequest);
    dnsRequest->deleteLater();
}

void DnsCache::onTimer()
{
    bool bChanged = false;
    // delete outdated and unused IPs from cache
    auto it = cache_.begin();
    while (it != cache_.end())
    {
        qint64 curTime = QDateTime::currentMSecsSinceEpoch();
        if ((curTime - it.value().time) > cacheTimeoutMs_ && !usages_->isHostnameUsed(it.key()))
        {
            it = cache_.erase(it);
            bChanged = true;
        }
        else
        {
            ++it;
        }
    }

    if (bChanged)
    {
        checkForNewIps();
    }
}

void DnsCache::checkForNewIps()
{
    QSet<QString> setIps;

    for (auto it = cache_.begin(); it != cache_.end(); ++it)
    {
        if (usages_->isHostnameUsed(it.key()))
        {
            for (auto ip : it.value().ips)
            {
                setIps.insert(ip);
            }
        }
    }

    if (setIps != lastWhitelistIps_)
    {
        lastWhitelistIps_ = setIps;
        emit whitelistIpsChanged(setIps);
    }
}

