#ifndef SERVICECOMMUNICATION
#define SERVICECOMMUNICATION

//#define AA_COMMAND_EXECUTE                                0
#define AA_COMMAND_FIREWALL_ON                              1
#define AA_COMMAND_FIREWALL_OFF                             2
#define AA_COMMAND_FIREWALL_CHANGE                          3 // deprecated
#define AA_COMMAND_FIREWALL_STATUS                          4
#define AA_COMMAND_FIREWALL_IPV6_ENABLE                     5
#define AA_COMMAND_FIREWALL_IPV6_DISABLE                    6

#define AA_COMMAND_REMOVE_WINDSCRIBE_FROM_HOSTS             7
#define AA_COMMAND_CHECK_UNBLOCKING_CMD_STATUS              8
#define AA_COMMAND_GET_UNBLOCKING_CMD_COUNT                 9
#define AA_COMMAND_CLEAR_UNBLOCKING_CMD                     10
#define AA_COMMAND_GET_HELPER_VERSION                       11

#define AA_COMMAND_OS_IPV6_STATE                            12
#define AA_COMMAND_OS_IPV6_ENABLE                           13
#define AA_COMMAND_OS_IPV6_DISABLE                          14

#define AA_COMMAND_ICS_IS_SUPPORTED                         15
#define AA_COMMAND_ICS_START                                16
#define AA_COMMAND_ICS_CHANGE                               17
#define AA_COMMAND_ICS_STOP                                 18

#define AA_COMMAND_ADD_HOSTS                                19
#define AA_COMMAND_REMOVE_HOSTS                             20

#define AA_COMMAND_DISABLE_DNS_TRAFFIC                      21
#define AA_COMMAND_ENABLE_DNS_TRAFFIC                       22

#define AA_COMMAND_REINSTALL_WAN_IKEV2                      23
#define AA_COMMAND_ENABLE_WAN_IKEV2                         24
#define AA_COMMAND_CLOSE_TCP_CONNECTIONS                    25
#define AA_COMMAND_ENUM_PROCESSES                           26
#define AA_COMMAND_TASK_KILL                                27
#define AA_COMMAND_RESET_TAP                                28
#define AA_COMMAND_SET_METRIC                               29
#define AA_COMMAND_WMIC_ENABLE                              30
#define AA_COMMAND_WMIC_GET_CONFIG_ERROR_CODE               31
#define AA_COMMAND_CLEAR_DNS_ON_TAP                         32
#define AA_COMMAND_ENABLE_BFE                               33
#define AA_COMMAND_RUN_OPENVPN                              34
#define AA_COMMAND_WHITELIST_PORTS                          35
#define AA_COMMAND_DELETE_WHITELIST_PORTS                   36
#define AA_COMMAND_SET_MAC_ADDRESS_REGISTRY_VALUE_SZ        37
#define AA_COMMAND_REMOVE_MAC_ADDRESS_REGISTRY_PROPERTY     38
#define AA_COMMAND_RESET_NETWORK_ADAPTER                    39
#define AA_COMMAND_SPLIT_TUNNELING_SETTINGS                 40
#define AA_COMMAND_ADD_IKEV2_DEFAULT_ROUTE                  41
#define AA_COMMAND_REMOVE_WINDSCRIBE_NETWORK_PROFILES       42
#define AA_COMMAND_CHANGE_MTU                               43
#define AA_COMMAND_RESET_AND_START_RAS                      44
#define AA_COMMAND_SET_IKEV2_IPSEC_PARAMETERS               45
#define AA_COMMAND_START_WIREGUARD                          46
#define AA_COMMAND_STOP_WIREGUARD                           47
#define AA_COMMAND_GET_WIREGUARD_STATUS                     48
#define AA_COMMAND_CONNECT_STATUS                           49
#define AA_COMMAND_SUSPEND_UNBLOCKING_CMD                   50
#define AA_COMMAND_CONNECTED_DNS                            51
#define AA_COMMAND_MAKE_HOSTS_FILE_WRITABLE                 52
#define AA_COMMAND_REINSTALL_TAP_DRIVER                     53
#define AA_COMMAND_REINSTALL_WINTUN_DRIVER                  54

#include <string>
#include <vector>

struct CMD_FIREWALL_ON
{
	bool allowLanTraffic = false;
    bool isCustomConfig = false;
    std::wstring connectingIp;
    std::wstring ip;
};

struct CMD_ADD_HOSTS
{
    std::wstring hosts;
};

struct CMD_CHECK_UNBLOCKING_CMD_STATUS
{
    unsigned long cmdId;
};

struct CMD_ENABLE_FIREWALL_ON_BOOT
{
    bool bEnable;
};

struct CMD_CLEAR_UNBLOCKING_CMD
{
    unsigned long blockingCmdId;
};

struct CMD_SUSPEND_UNBLOCKING_CMD
{
    unsigned long blockingCmdId;
};

struct CMD_TASK_KILL
{
    std::wstring szExecutableName;
};

struct CMD_RESET_TAP
{
    std::wstring szTapName;
};

struct CMD_SET_METRIC
{
    std::wstring szInterfaceType;
    std::wstring szInterfaceName;
    std::wstring szMetricNumber;
};

struct CMD_WMIC_ENABLE
{
    std::wstring szAdapterName;
};

struct CMD_WMIC_GET_CONFIG_ERROR_CODE
{
    std::wstring szAdapterName;
};

struct CMD_RUN_OPENVPN
{
    std::wstring    szOpenVpnExecutable;
    std::wstring    szConfig;
    unsigned int    portNumber = 0;
    std::wstring    szHttpProxy;           // empty string, if no http
    unsigned int    httpPortNumber = 0;
    std::wstring    szSocksProxy;          // empty string, if no socks
    unsigned int    socksPortNumber = 0;
    bool            isCustomConfig = false;
};

struct CMD_ICS_CHANGE
{
    std::wstring szAdapterName;
};

struct CMD_WHITELIST_PORTS
{
    std::wstring ports;
};

struct CMD_SET_MAC_ADDRESS_REGISTRY_VALUE_SZ
{
    std::wstring szInterfaceName;
    std::wstring szValue;
};

struct CMD_REMOVE_MAC_ADDRESS_REGISTRY_PROPERTY
{
    std::wstring szInterfaceName;
};

struct CMD_RESET_NETWORK_ADAPTER
{
    bool bringBackUp = false;
    std::wstring szInterfaceName;
};


struct CMD_SPLIT_TUNNELING_SETTINGS
{
    bool isActive;
    bool isExclude;     // true -> SPLIT_TUNNELING_MODE_EXCLUDE, false -> SPLIT_TUNNELING_MODE_INCLUDE
    bool isAllowLanTraffic;

    std::vector<std::wstring> files;
    std::vector<std::wstring> ips;
    std::vector<std::string> hosts;
};

struct ADAPTER_GATEWAY_INFO
{
	std::string adapterName;
	std::string adapterIp;
	std::string gatewayIp;
	std::vector<std::string> dnsServers;
	unsigned long ifIndex;
};


enum CMD_PROTOCOL_TYPE {
	CMD_PROTOCOL_IKEV2,
	CMD_PROTOCOL_OPENVPN,
	CMD_PROTOCOL_STUNNEL_OR_WSTUNNEL,
	CMD_PROTOCOL_WIREGUARD
};

// used to manage split tunneling
struct CMD_CONNECT_STATUS
{
	bool isConnected;

    bool isTerminateSocket;
    bool isKeepLocalSocket;

	CMD_PROTOCOL_TYPE protocol;

	ADAPTER_GATEWAY_INFO defaultAdapter;
	ADAPTER_GATEWAY_INFO vpnAdapter;

	// need for stunnel/wstunnel/openvpn routing
	std::string connectedIp;
	std::string remoteIp;
};

struct CMD_CONNECTED_DNS
{
    unsigned long ifIndex = 0;
    std::wstring szDnsIpAddress;
};

struct CMD_CLOSE_TCP_CONNECTIONS
{
    bool isKeepLocalSockets;
};

struct CMD_CHANGE_MTU
{
    int mtu = 0;
    bool storePersistent = false;
    std::wstring szAdapterName;
};

struct CMD_START_WIREGUARD
{
    std::wstring    szExecutable;
    std::wstring    szDeviceName;
};

struct CMD_CONFIGURE_WIREGUARD
{
    std::string     clientPrivateKey;
    std::string     clientIpAddress;
    std::string     clientDnsAddressList;
    std::string     peerPublicKey;
    std::string     peerPresharedKey;
    std::string     peerEndpoint;
    std::string     allowedIps;
};

struct CMD_REINSTALL_TUN_DRIVER
{
    std::wstring driverDir;
};

struct CMD_DISABLE_DNS_TRAFFIC
{
    std::vector<std::wstring> excludedIps;
};

enum WireGuardServiceState
{
    WIREGUARD_STATE_NONE,       // WireGuard is not started.
    WIREGUARD_STATE_ERROR,      // WireGuard is in error state.
    WIREGUARD_STATE_STARTING,   // WireGuard is initializing/starting up.
    WIREGUARD_STATE_LISTENING,  // WireGuard is listening for UAPI commands, but not connected.
    WIREGUARD_STATE_CONNECTING, // WireGuard is configured and awaits for a handshake.
    WIREGUARD_STATE_ACTIVE,     // WireGuard is connected.
};

struct MessagePacketResult
{
	__int64 id;
	bool success;
    unsigned long exitCode;
	unsigned long blockingCmdId;    // id for check status of blocking cmd
	bool blockingCmdFinished;
    unsigned __int64 customInfoValue[3];
	std::string additionalString;

	MessagePacketResult() : id(0), success(false), exitCode(0), blockingCmdId(0),
		blockingCmdFinished(false), customInfoValue()
	{
	}
};

#endif // SERVICECOMMUNICATION

