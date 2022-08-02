#ifndef TYPES_CONNECTIONSETTINGS_H
#define TYPES_CONNECTIONSETTINGS_H

#include <QSettings>
#include <QString>
#include "protocoltype.h"

namespace types {

struct ConnectionSettings
{
    ConnectionSettings();
    explicit ConnectionSettings(const ProtoTypes::ConnectionSettings &s);
    void set(const ProtocolType &protocol, uint port, bool bAutomatic);
    void set(const ConnectionSettings &s);

    ProtocolType protocol() const;
    uint port() const;
    bool isAutomatic() const;

    bool readFromSettingsV1(QSettings &settings);

    bool isInitialized() const;

    bool isEqual(const ConnectionSettings &s) const;

    void debugToLog() const;
    void logConnectionSettings() const;

    ProtoTypes::ConnectionSettings convertToProtobuf() const;

    bool operator==(const ConnectionSettings &other) const
    {
        return other.protocol_ == protocol_ &&
               other.port_ == port_ &&
               other.bAutomatic_ == bAutomatic_ &&
               other.bInitialized_ == bInitialized_;
    }

    bool operator!=(const ConnectionSettings &other) const
    {
        return !(*this == other);
    }


private:
    ProtocolType protocol_;
    uint    port_;
    bool    bAutomatic_;
    bool    bInitialized_;
};

} //namespace types

#endif // TYPES_CONNECTIONSETTINGS_H
