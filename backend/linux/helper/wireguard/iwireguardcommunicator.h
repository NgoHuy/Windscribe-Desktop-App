#ifndef IWireGuardCommunicator_h
#define IWireGuardCommunicator_h

#include <map>
#include <string>
#include <vector>
#include <cstdint>

class IWireGuardCommunicator
{
public:
    virtual bool start(
        const std::string &exePath,
        const std::string &executable,
        const std::string &deviceName) = 0;
    virtual bool stop() = 0;
    virtual bool configure(
        const std::string &clientPrivateKey,
        const std::string &peerPublicKey,
        const std::string &peerPresharedKey,
        const std::string &peerEndpoint,
        const std::vector<std::string> &allowedIps,
        uint32_t fwmark) = 0;
    virtual unsigned long getStatus(
        unsigned int *errorCode,
        unsigned long long *bytesReceived,
        unsigned long long *bytesTransmitted) = 0;
};

#endif  // IWireGuardCommunicator_h
