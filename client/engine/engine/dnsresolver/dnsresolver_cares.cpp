#include "dnsresolver_cares.h"
#include "dnsutils.h"
#include "utils/ws_assert.h"
#include "utils/logger.h"
#include "ares.h"

#include <QRunnable>

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <sys/select.h>
#endif

namespace {

std::atomic_bool g_FinishAll = false;

#ifdef Q_OS_WIN
        typedef QVector<IN_ADDR> DnsAddrs;
#else
        typedef QVector<in_addr> DnsAddrs;
#endif

struct UserArg
{
    QStringList ips;
    int errorCode = ARES_ECANCELLED;
};

class LookupJob : public QRunnable
{
public:
     LookupJob(const QString &hostname, QSharedPointer<QObject> object, const QStringList &dnsServers, int timeoutMs) :
        hostname_(hostname),
        object_(object),
        dnsServers_(dnsServers),
        timeoutMs_(timeoutMs),
        elapsedMs_(0)
    {
    }

    void run() override
    {
        QElapsedTimer elapsedTimer;
        elapsedTimer.start();

        while (errorCode_ != ARES_SUCCESS && elapsedTimer.elapsed() <= timeoutMs_) {

            // fill dns addresses
            DnsAddrs dnsAddrs;
            const QStringList dnsList = getDnsIps(dnsServers_);
            dnsAddrs.reserve(dnsList.count());
            for (const auto &dnsIp : dnsList) {
                struct sockaddr_in sa;
                ares_inet_pton(AF_INET, dnsIp.toStdString().c_str(), &(sa.sin_addr));
                dnsAddrs.push_back(sa.sin_addr);
            }

            // create channel and set options
            ares_channel channel;
            struct ares_options options;
            int optmask = 0;
            createOptionsForAresChannel(timeoutMs_, options, optmask, dnsAddrs);
            int status = ares_init_options(&channel, &options, optmask);
            if (status != ARES_SUCCESS) {
                qCDebug(LOG_BASIC) << "ares_init_options failed:" << QString::fromStdString(ares_strerror(status));
                return;
            }

            UserArg userArg;
            ares_gethostbyname(channel, hostname_.toStdString().c_str(), AF_INET, callback, &userArg);

            // process loop
            timeval tv;
            while (!g_FinishAll) {
                fd_set readers, writers;
                FD_ZERO(&readers);
                FD_ZERO(&writers);
                int nfds = ares_fds(channel, &readers, &writers);
                if (nfds == 0)
                    break;
                // do not block for longer than kTimeoutMs interval
                timeval tvp;
                tvp.tv_sec = 0;
                tvp.tv_usec = kTimeoutMs * 1000;
                select(nfds, &readers, &writers, NULL, &tvp);
                ares_process(channel, &readers, &writers);
                if (elapsedTimer.elapsed() > timeoutMs_) {
                    userArg.errorCode = ARES_ETIMEOUT;
                    break;
                }
            }
            ares_destroy(channel);

            ips_ = userArg.ips;
            errorCode_ = userArg.errorCode;
            elapsedMs_ = elapsedTimer.elapsed();
        }

        if (object_) {
            QString errorStr;
            if (errorCode_ != ARES_SUCCESS)
                errorStr = QString::fromStdString(ares_strerror(errorCode_));

            bool bSuccess = QMetaObject::invokeMethod(object_.get(), "onResolved",
                            Qt::QueuedConnection, Q_ARG(QStringList, ips_), Q_ARG(QString, errorStr), Q_ARG(qint64, elapsedMs_));
            WS_ASSERT(bSuccess);
        }
    }

    QStringList ips() const { return ips_; }
    int errorCode() const { return errorCode_; }
    qint64 elapsedMs() const { return elapsedMs_; }

private:
    // 200 ms settled for faster switching to the next try (next server)
    // this does not mean that the current request will be limited to 200ms,
    // but after 200 ms, the next one will start parallel to the first one
    // (see discussion for details: https://lists.haxx.se/pipermail/c-ares/2022-January/000032.html
    static constexpr int kTimeoutMs = 200;
    static constexpr int kTries = 4; // default value in c-ares, let's leave it as it is
    QString hostname_;
    QSharedPointer<QObject> object_;
    QStringList dnsServers_;
    int timeoutMs_;
    qint64 elapsedMs_;

    QStringList ips_;
    int errorCode_ = ARES_ECANCELLED;

    QStringList getDnsIps(const QStringList &ips)
    {
        if (ips.isEmpty()) {
    #if defined(Q_OS_MAC)
            QStringList osDefaultList;  // Empty by default.
            // On Mac, don't rely on automatic OS default DNS fetch in CARES, because it reads them from
            // the "/etc/resolv.conf", which is sometimes not available immediately after reboot.
            // Feed the CARES with valid OS default DNS values taken from scutil.
            const auto listDns = DnsUtils::getOSDefaultDnsServers();
            for (auto it = listDns.cbegin(); it != listDns.cend(); ++it)
                osDefaultList.push_back(QString::fromStdWString(*it));
            return osDefaultList;
    #endif
        }
        return ips;
    }

    void createOptionsForAresChannel(int timeoutMs, ares_options &options, int &optmask, DnsAddrs &dnsAddrs)
    {
        memset(&options, 0, sizeof(options));

        if (dnsAddrs.isEmpty()) {
            optmask = ARES_OPT_TRIES | ARES_OPT_TIMEOUTMS;
            options.tries = kTries;
            options.timeout = kTimeoutMs;
        } else {
            optmask = ARES_OPT_TRIES | ARES_OPT_SERVERS | ARES_OPT_TIMEOUTMS;
            options.tries = kTries;
            options.timeout = kTimeoutMs;
            options.nservers = dnsAddrs.count();
            options.servers = &(dnsAddrs[0]);
        }
    }

    static void callback(void *arg, int status, int timeouts, struct hostent *host)
    {
        Q_UNUSED(timeouts);
        UserArg *userArg = static_cast<UserArg *>(arg);

        if (status == ARES_SUCCESS) {
            QStringList addresses;
            for (char **p = host->h_addr_list; *p; p++) {
                char addr_buf[46] = "??";

                ares_inet_ntop(host->h_addrtype, *p, addr_buf, sizeof(addr_buf));
                addresses << QString::fromStdString(addr_buf);
            }
            userArg->ips = addresses;
        } else {
            userArg->ips.clear();
        }
        userArg->errorCode = status;
    }
};

} // namespace

DnsResolver_cares::DnsResolver_cares()
{
    aresLibraryInit_.init();
    threadPool_ = new QThreadPool();
}

DnsResolver_cares::~DnsResolver_cares()
{
    g_FinishAll = true;
    threadPool_->waitForDone();
    delete threadPool_;
    qCDebug(LOG_BASIC) << "DnsResolver stopped";
}

void DnsResolver_cares::lookup(const QString &hostname, QSharedPointer<QObject> object, const QStringList &dnsServers, int timeoutMs)
{
    LookupJob *job = new LookupJob(hostname, object, dnsServers, timeoutMs);
    threadPool_->start(job);
    WS_ASSERT(threadPool_->activeThreadCount() <= threadPool_->maxThreadCount());   // in this case, we probably need to redo the logic
}

QStringList DnsResolver_cares::lookupBlocked(const QString &hostname, const QStringList &dnsServers, int timeoutMs, QString *outError)
{
    LookupJob job(hostname, nullptr, dnsServers, timeoutMs);
    job.run();
    if (outError) {
        if (job.errorCode() != ARES_SUCCESS)
            *outError = QString::fromStdString(ares_strerror(job.errorCode()));
        else
            *outError = QString();
    }
    return job.ips();
}
