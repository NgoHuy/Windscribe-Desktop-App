#include <syslog.h>
#include <signal.h>
#include "server.h"
#include "logger.h"
#include "utils.h"

Server server;

void handler_sigterm(int signum)
{
    UNUSED(signum);
    Logger::instance().out("Windscribe helper terminated");
    server.stop();
}

int main(int argc, const char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    signal(SIGSEGV, handler_sigterm);
    signal(SIGFPE, handler_sigterm);
    signal(SIGABRT, handler_sigterm);
    signal(SIGILL, handler_sigterm);
    signal(SIGINT, handler_sigterm);
    signal(SIGTERM, handler_sigterm);

    Logger::instance().checkLogSize();

    // restore firewall setting on OS reboot, if there are saved rules on /etc/windscribe dir

    if (Utils::isFileExists("/etc/windscribe/rules.v4"))
    {
        Utils::executeCommand("iptables-restore -n < /etc/windscribe/rules.v4");
    }
    if (Utils::isFileExists("/etc/windscribe/rules.v6"))
    {
        Utils::executeCommand("ip6tables-restore -n < /etc/windscribe/rules.v6");
    }

    server.run();

    Logger::instance().out("Windscribe helper finished");
    return EXIT_SUCCESS;
}
