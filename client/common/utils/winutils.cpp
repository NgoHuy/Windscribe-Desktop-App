#include "winutils.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <psapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <tlhelp32.h>

#include <iostream>

#include <Windows.h>
#include <LM.h>

#include <wlanapi.h>
#include <objbase.h>
#include <wtypes.h>
#include <netlistmgr.h>

#include <atlbase.h>

#include <QDir>
#include <QCoreApplication>
#include <QScopeGuard>

#include "utils.h"
#include "logger.h"

#include "../../gui/authhelper/win/ws_com/guids.h"


#pragma comment(lib, "wlanapi.lib")

// TODO: implement via new way
#define WORKING_BUFFER_SIZE 15000
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383

QString processExecutablePath( DWORD processID );
GUID guidFromQString(QString str);
QString guidToQString(GUID guid);

bool WinUtils::reboot()
{
    HANDLE hToken;
    TOKEN_PRIVILEGES tkp;

    // Get a token for this process.
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
        return false;
    }

    // Get the LUID for the shutdown privilege.
    LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);

    tkp.PrivilegeCount = 1;  // one privilege to set
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Get the shutdown privilege for this process.
    AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

    if (GetLastError() != ERROR_SUCCESS)
    {
        return false;
    }

    // Shut down the system and force all applications to close.
    if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, SHTDN_REASON_MAJOR_OPERATINGSYSTEM | SHTDN_REASON_MINOR_UPGRADE | SHTDN_REASON_FLAG_PLANNED))
    {
        return false;
    }

    return true;
}

bool getWinVersion(RTL_OSVERSIONINFOEXW *rtlOsVer)
{
    NTSTATUS (WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
    if (RtlGetVersion)
    {
        memset(rtlOsVer, 0, sizeof(RTL_OSVERSIONINFOEXW));
        rtlOsVer->dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOEXW);
        RtlGetVersion(rtlOsVer);
        return true;
    }
    return false;
}

bool WinUtils::isWindows10orGreater()
{
    RTL_OSVERSIONINFOEXW rtlOsVer;
    if (getWinVersion(&rtlOsVer))
    {
        if (rtlOsVer.dwMajorVersion >= 10)
        {
            return true;
        }
    }
    return false;
}

bool WinUtils::isWindows7()
{
    RTL_OSVERSIONINFOEXW rtlOsVer;
    if (getWinVersion(&rtlOsVer))
    {
        if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 1)
        {
            return true;
        }
    }
    return false;
}

bool WinUtils::isWindowsVISTAor7or8()
{
    RTL_OSVERSIONINFOEXW rtlOsVer;
    if (getWinVersion(&rtlOsVer))
    {
        if (rtlOsVer.dwMajorVersion == 6)
        {
            return true;
        }
    }
    return false;
}


QString WinUtils::getWinVersionString()
{
    QString ret;

    RTL_OSVERSIONINFOEXW rtlOsVer;
    if (getWinVersion(&rtlOsVer))
    {
        if (rtlOsVer.dwMajorVersion == 10 && rtlOsVer.dwMinorVersion >= 0 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = (rtlOsVer.dwBuildNumber >= 22000 ? "Windows 11" : "Windows 10");
        else if (rtlOsVer.dwMajorVersion == 10 && rtlOsVer.dwMinorVersion >= 0 && rtlOsVer.wProductType != VER_NT_WORKSTATION)  ret = "Windows 10 Server";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 3 && rtlOsVer.wProductType != VER_NT_WORKSTATION)  ret = "Windows Server 2012 R2";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 3 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = "Windows 8.1";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 2 && rtlOsVer.wProductType != VER_NT_WORKSTATION)  ret = "Windows Server 2012";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 2 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = "Windows 8";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 1 && rtlOsVer.wProductType != VER_NT_WORKSTATION)  ret = "Windows Server 2008 R2";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 1 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = "Windows 7";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 0 && rtlOsVer.wProductType != VER_NT_WORKSTATION)  ret = "Windows Server 2008";
        else if (rtlOsVer.dwMajorVersion == 6 && rtlOsVer.dwMinorVersion == 0 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = "Windows Vista";
        else if (rtlOsVer.dwMajorVersion == 5 && rtlOsVer.dwMinorVersion == 2 && rtlOsVer.wProductType == VER_NT_WORKSTATION)  ret = "Windows XP";
        else if (rtlOsVer.dwMajorVersion == 5 && rtlOsVer.dwMinorVersion == 2)   ret = "Windows Server 2003";
        else if (rtlOsVer.dwMajorVersion == 5 && rtlOsVer.dwMinorVersion == 1)   ret = "Windows XP";
        else if (rtlOsVer.dwMajorVersion == 5 && rtlOsVer.dwMinorVersion == 0)   ret = "Windows 2000";
        else ret = "Unknown";

        if (rtlOsVer.szCSDVersion[0] != L'\0')
        {
            ret += " " + QString::fromStdWString(rtlOsVer.szCSDVersion);
        }

        ret += " (major: " + QString::number(rtlOsVer.dwMajorVersion) + ", minor: " + QString::number(rtlOsVer.dwMinorVersion) + ")";
        ret += " (build: " + QString::number(rtlOsVer.dwBuildNumber) + ")";
    }
    else
    {
        ret = "Can't detect Windows version";
    }

    return ret;
}

void WinUtils::getOSVersionAndBuild(QString &osVersion, QString &build)
{
    RTL_OSVERSIONINFOEXW rtlOsVer;
    if (getWinVersion(&rtlOsVer))
    {
        osVersion = QString::number(rtlOsVer.dwMajorVersion) + "." + QString::number(rtlOsVer.dwMinorVersion);
        build = QString::number(rtlOsVer.dwBuildNumber);
    }
    else
    {
        osVersion = "NotDetected";
        build = "NotDetected";
    }
}

QString regGetLocalMachineRegistryValueSz(HKEY rootKey, QString keyPath, QString propertyName, bool wow64)
{
    QString result = "";

    DWORD options = KEY_READ;
    if (wow64) options |= KEY_WOW64_64KEY;

    HKEY hKey;
    LONG nError = RegOpenKeyEx(rootKey, keyPath.toStdWString().c_str(), NULL, options, &hKey);
    if (nError == ERROR_SUCCESS)
    {
        DWORD dwType = REG_SZ;
        char value[1024];
        DWORD value_length = 1024;
        nError = RegQueryValueEx(hKey, propertyName.toStdWString().c_str(), NULL, &dwType, (LPBYTE) &value, &value_length);
        if(nError == ERROR_SUCCESS)
        {
            // TODO: make this better!
            for (DWORD i = 0; i < value_length; i++)
            {
                if (value[i] > 16)
                {
                    result += QString(value[i]);
                }
                else
                {
                    result += QString::fromUtf8(&value[i]);
                }
            }

            RegCloseKey(hKey);
        }
        else
        {
            RegCloseKey(hKey);
        }
    }

    return result;
}

QList<QString> enumerateSubkeyNames(HKEY rootKey, QString keyPath, bool wow64)
{
    QList<QString> result;

    std::wstring subKey = keyPath.toStdWString();

    DWORD options = KEY_READ;
    if (wow64) options |= KEY_WOW64_64KEY;

    HKEY hKeyParent;
    if (RegOpenKeyEx( rootKey,
                      subKey.c_str(),
                      0, options, &hKeyParent) == ERROR_SUCCESS)
    {
        TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name
        DWORD    cchClassName = MAX_PATH;  // size of class string
        DWORD    cSubKeys=0;               // number of subkeys
        DWORD    cbMaxSubKey;              // longest subkey size
        DWORD    cchMaxClass;              // longest class string
        DWORD    cValues;              // number of values for key
        DWORD    cchMaxValue;          // longest value name
        DWORD    cbMaxValueData;       // longest value data
        DWORD    cbSecurityDescriptor; // size of security descriptor
        FILETIME ftLastWriteTime;      // last write time

        DWORD retCode;

        // HKEY hKey;
        // Get the class name and the value count.
        retCode = RegQueryInfoKey(
            hKeyParent,                    // key handle
            achClass,                // buffer for class name
            &cchClassName,           // size of class string
            NULL,                    // reserved
            &cSubKeys,               // number of subkeys
            &cbMaxSubKey,            // longest subkey size
            &cchMaxClass,            // longest class string
            &cValues,                // number of values for this key
            &cchMaxValue,            // longest value name
            &cbMaxValueData,         // longest value data
            &cbSecurityDescriptor,   // security descriptor
            &ftLastWriteTime);       // last write time


        if (cSubKeys)
        {
            for (DWORD i=0; i<cSubKeys; i++)
            {
                TCHAR achKey[MAX_KEY_LENGTH];   // buffer for subkey name
                DWORD cbName = MAX_KEY_LENGTH;  // size of name string
                retCode = RegEnumKeyEx(hKeyParent, i,
                         achKey,
                         &cbName,
                         NULL,
                         NULL,
                         NULL,
                         &ftLastWriteTime);
                if (retCode == ERROR_SUCCESS)
                {
                    result.append(QString::fromWCharArray(achKey));
                }
            }
        }

        RegCloseKey(hKeyParent);
    }

    return result;
}

QMap<QString,QString> enumerateCleanedProgramLocations(HKEY hKey, QString keyPath, bool wow64)
{
    QMap<QString, QString> programLocations;

    const QList<QString> uninstallSubkeys = enumerateSubkeyNames(hKey, keyPath, wow64);

    for (const QString &subkey : uninstallSubkeys)
    {
        QString fullKeyName = keyPath + "\\" + subkey;
        QString iconPath = regGetLocalMachineRegistryValueSz(hKey, fullKeyName,"DisplayIcon", wow64);
        QString programName = regGetLocalMachineRegistryValueSz(hKey, fullKeyName,"DisplayName", wow64);

        if (iconPath != "" && programName != "")
        {
            if (iconPath.contains("\"")) iconPath = iconPath.remove("\"");
            int lastIndex = iconPath.indexOf(",");

            QString filteredIconPath = iconPath.mid(0, lastIndex);
            programLocations.insert(programName, filteredIconPath);
        }
    }

    return programLocations;
}

QMap<QString, QString> WinUtils::enumerateInstalledProgramIconLocations()
{
    QMap<QString, QString> programLocations;

    QString keyPath("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    QList<QMap<QString,QString>> maps;
    maps.append(enumerateCleanedProgramLocations(HKEY_LOCAL_MACHINE, keyPath, true)); // HLM 64
    maps.append(enumerateCleanedProgramLocations(HKEY_LOCAL_MACHINE, keyPath, false)); // HLM 32
    maps.append(enumerateCleanedProgramLocations(HKEY_CURRENT_USER, keyPath, false)); // HCU (32)

    for (int i = 0; i < maps.length(); i++)
    {
        const QMap<QString,QString> map = maps[i];
        const auto mapKeys = map.keys();
        for (QString name : mapKeys)
        {
            programLocations.insert(name, map[name]);
        }
    }
    return programLocations;
}


bool WinUtils::isGuiAlreadyRunning()
{
    HWND hwnd = FindWindow(classNameIcon.c_str(), wsGuiIcon.c_str());
    if (hwnd) {
        return true;
    }

    hwnd = FindWindow(classNameIconDebug.c_str(), wsGuiIcon.c_str());
    if (hwnd) {
        return true;
    }

    return false;
}

QString WinUtils::regGetLocalMachineRegistryValueSz(QString keyPath, QString propertyName)
{
    QString result = "";

    HKEY hKey;
    LONG nError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath.toStdWString().c_str(), NULL, KEY_READ, &hKey);
    if (nError == ERROR_SUCCESS)
    {
        DWORD dwType = REG_SZ;
        char value[1024];
        DWORD value_length = 1024;
        nError = RegQueryValueEx(hKey, propertyName.toStdWString().c_str(), NULL, &dwType, (LPBYTE) &value, &value_length);
        if(nError == ERROR_SUCCESS)
        {
            // TODO: make this better!
            for (DWORD i = 0; i < value_length; i++)
            {
                if (value[i] > 16)
                {
                    result += QString(value[i]);
                }
                else
                {
                    result += QString::fromUtf8(&value[i]);
                }
            }

            RegCloseKey(hKey);
        }
        else
        {
            RegCloseKey(hKey);
        }
    }

    return result;
}

bool WinUtils::regHasLocalMachineSubkeyProperty(QString keyPath, QString propertyName)
{
    bool result = false;

    HKEY hKey;
    LONG nError = RegOpenKeyEx(HKEY_LOCAL_MACHINE, keyPath.toStdWString().c_str(), NULL, KEY_READ, &hKey);

    if (nError == ERROR_SUCCESS)
    {
        nError = RegQueryValueEx(hKey, propertyName.toStdWString().c_str(), nullptr, REG_NONE, nullptr, nullptr); // contents not required

        if(nError == ERROR_SUCCESS)
        {
            result = true;
        }

        RegCloseKey(hKey);
    }

    return result;
}

bool WinUtils::regGetCurrentUserRegistryDword(QString keyPath, QString propertyName, int &dwordValue)
{
    bool result = false;
    HKEY hKey;
    LONG nError = RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.toStdWString().c_str(),
                               NULL, KEY_READ, &hKey);
    if (nError == ERROR_SUCCESS)
    {
        DWORD dwBufSize = sizeof(dwordValue);
        nError = RegQueryValueEx(hKey, propertyName.toStdWString().c_str(),
                                 nullptr, nullptr, (LPBYTE) &dwordValue, &dwBufSize);

        if(nError == ERROR_SUCCESS)
        {
            result = true;
        }

        RegCloseKey(hKey);
    }

    return result;
}

QList<QString> WinUtils::enumerateRunningProgramLocations()
{
    QList<QString> result;

    // Get the list of process identifiers.
    DWORD aProcesses[1024], cbNeeded, cProcesses;
    unsigned int i;

    if ( !EnumProcesses( aProcesses, sizeof(aProcesses), &cbNeeded ) )
    {
        return result;
    }

    // Calculate how many process identifiers were returned.
    cProcesses = cbNeeded / sizeof(DWORD);

    // Print the name and process identifier for each process.
    for ( i = 0; i < cProcesses; i++ )
    {
        if( aProcesses[i] != 0 )
        {
            QString exePath = processExecutablePath( aProcesses[i] );
            if (!result.contains(exePath) && exePath != "")
            {
                result.append(exePath);
            }
        }
    }

    return result;
}


QString processExecutablePath( DWORD processID )
{
    QString result;
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |
                                   PROCESS_VM_READ,
                                   FALSE, processID );

    // Get the process name.
    if (NULL != hProcess )
    {
        TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
        if (GetModuleFileNameEx(hProcess, NULL, szProcessName, sizeof(szProcessName)/sizeof(TCHAR)))
        {
            result = QString::fromWCharArray(szProcessName);
        }
    }

    CloseHandle( hProcess );

    return result;
}


types::NetworkInterface WinUtils::currentNetworkInterface()
{
    types::NetworkInterface curNetworkInterface;

    IfTable2Row row = lowestMetricNonWindscribeIfTableRow();
    if (!row.valid)
    {
        qCDebug(LOG_BASIC) << "WinUtils::lowestMetricNonWindscribeIfTableRow failed";
        return curNetworkInterface;
    }

    QVector<types::NetworkInterface> interfaces = currentNetworkInterfaces(true);

    for (int i = 0; i < interfaces.size(); i++)
    {
        types::NetworkInterface network = interfaces[i];

        if (network.interfaceIndex == static_cast<int>(row.index))
        {
            curNetworkInterface = network;
            break;
        }
    }

    // qDebug() << "#########Current interface found to be: ";
    // Utils::printInterface(curNetworkInterface);
    // qDebug() << "Of: ";
    // Utils::printInterfaces(interfaces);

    return curNetworkInterface;
}

QVector<types::NetworkInterface> WinUtils::currentNetworkInterfaces(bool includeNoInterface)
{
    QVector<types::NetworkInterface> networkInterfaces;

    // Add "No Interface" selection
    if (includeNoInterface)
    {
        types::NetworkInterface noInterfaceSelection = Utils::noNetworkInterface();
        networkInterfaces << noInterfaceSelection;
    }

    const QList<IpAdapter> ipAdapters = getIpAdapterTable();
    const QList<AdapterAddress> adapterAddresses = getAdapterAddressesTable(); // TODO: invalid parameters passed to C runtime

    const QList<IfTableRow> ifTable = getIfTable();
    const QList<IfTable2Row> ifTable2 = getIfTable2();
    const QList<IpForwardRow> ipForwardTable = getIpForwardTable();

    for (const IpAdapter &ia: ipAdapters)  // IpAdapters holds list of live adapters
    {
        types::NetworkInterface networkInterface;
        networkInterface.interfaceIndex = ia.index;
        networkInterface.interfaceGuid = ia.guid;
        networkInterface.physicalAddress = ia.physicalAddress;

        NETWORK_INTERACE_TYPE nicType = NETWORK_INTERFACE_NONE;

        for (const IfTableRow &itRow: ifTable)
        {
            if (itRow.index == static_cast<int>(ia.index))
            {
                nicType = itRow.type;
                networkInterface.mtu = itRow.mtu;
                networkInterface.interfaceType = nicType;
                networkInterface.active = itRow.connected;
                networkInterface.dwType = itRow.dwType;
                networkInterface.deviceName = itRow.interfaceName;

                if (nicType == NETWORK_INTERFACE_WIFI)
                {
                    networkInterface.networkOrSsid = ssidFromInterfaceGUID(itRow.guidName);
                }
                break;
            }
        }

        for (const IfTable2Row &it2Row: ifTable2)
        {
            if (it2Row.index == ia.index)
            {
                if (nicType == NETWORK_INTERFACE_ETH)
                {
                    networkInterface.networkOrSsid = networkNameFromInterfaceGUID(it2Row.interfaceGuid);
                }
                networkInterface.connectorPresent = it2Row.connectorPresent;
                networkInterface.endPointInterface = it2Row.endPointInterface;
                break;
            }
        }

        for (const IpForwardRow &ipfRow: ipForwardTable)
        {
            if (ipfRow.index == ia.index)
            {
                networkInterface.metric = ipfRow.metric;
                break;
            }
        }

        for (const AdapterAddress &aa: adapterAddresses)
        {
            if (aa.index == ia.index)
            {
                networkInterface.interfaceName = aa.friendlyName;
            }
        }

        // Filter out virtual physical adapters, but keep virtual adapters that act as Ethernet
        // network bridges (e.g. Hyper-V virtual switches on the host side).
        if (networkInterface.dwType != IF_TYPE_PPP &&
            !networkInterface.endPointInterface &&
            (networkInterface.interfaceType == NETWORK_INTERFACE_ETH
                || networkInterface.connectorPresent))
        {
            networkInterfaces << networkInterface;
        }
    }

    return networkInterfaces;
}

IfTable2Row WinUtils::lowestMetricNonWindscribeIfTableRow()
{
    IfTable2Row lowestMetricIfRow;

    const QList<IpForwardRow> fwdTable = getIpForwardTable();
    const QList<IpAdapter> ipAdapters = getIpAdapterTable();

    int lowestMetric = 999999;

    for (const IpForwardRow &row: fwdTable)
    {
        for (const IpAdapter &ipAdapter: ipAdapters)
        {
            if (ipAdapter.index == row.index)
            {
                IfTable2Row ifRow = ifTable2RowByIndex(row.index);
                if (ifRow.valid && ifRow.interfaceType != IF_TYPE_PPP && !ifRow.isWindscribeAdapter())
                {
                    const auto row_metric = static_cast<int>(row.metric);
                    if (row_metric < lowestMetric)
                    {
                        lowestMetric = static_cast<int>(row_metric);
                        lowestMetricIfRow = ifRow;
                    }
                }
            }
        }
    }

    // qDebug() << "ifRow.dwType: " << lowestMetricIfRow.dwType;

    return lowestMetricIfRow;
}

IfTableRow WinUtils::ifRowByIndex(int index)
{
    IfTableRow found;

    const auto if_table = getIfTable();
    for (IfTableRow row : if_table)
    {
        if (index == static_cast<int>(row.index)) // TODO: convert all ifIndices to int
        {
            found = row;
            break;
        }
    }

    return found;
}

QString WinUtils::interfaceSubkeyPath(int interfaceIndex)
{
    IfTableRow row = ifRowByIndex(interfaceIndex);

    const QString keyPath("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}");
    const QList<QString> ifSubkeys = interfaceSubkeys(keyPath);

    QString foundSubkey = "";
    for (const QString &subkey : ifSubkeys)
    {
        QString subkeyPath = keyPath + "\\" + subkey;
        QString subkeyInterfaceGuid = WinUtils::regGetLocalMachineRegistryValueSz(subkeyPath, "NetCfgInstanceId");

        if (subkeyInterfaceGuid == row.guidName)
        {
            foundSubkey = subkeyPath; // whole key path
            break;
        }
    }

    return foundSubkey;
}

QString WinUtils::interfaceSubkeyName(int interfaceIndex)
{
    IfTableRow row = ifRowByIndex(interfaceIndex);

    const QString keyPath("SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}");
    const QList<QString> ifSubkeys = interfaceSubkeys(keyPath);

    QString foundSubkeyName = "";
    for (const QString &subkey : ifSubkeys)
    {
        QString subkeyPath = keyPath + "\\" + subkey;
        QString subkeyInterfaceGuid = WinUtils::regGetLocalMachineRegistryValueSz(subkeyPath, "NetCfgInstanceId");

        if (subkeyInterfaceGuid == row.guidName)
        {
            foundSubkeyName = subkey; // just name -- not path
            break;
        }
    }

    return foundSubkeyName;
}

bool WinUtils::interfaceSubkeyHasProperty(int interfaceIndex, QString propertyName)
{
    QString interfaceSubkeyP = interfaceSubkeyPath(interfaceIndex);
    return WinUtils::regHasLocalMachineSubkeyProperty(interfaceSubkeyP, propertyName);
}



QList<QString> WinUtils::interfaceSubkeys(QString keyPath)
{
    QList<QString> result;

    std::wstring subKey = keyPath.toStdWString();

    HKEY hKeyParent;
    if (RegOpenKeyEx( HKEY_LOCAL_MACHINE,
                      subKey.c_str(),
                      0, KEY_READ, &hKeyParent) == ERROR_SUCCESS)
    {
        TCHAR    achClass[MAX_PATH] = TEXT("");  // buffer for class name
        DWORD    cchClassName = MAX_PATH;  // size of class string
        DWORD    cSubKeys=0;               // number of subkeys
        DWORD    cbMaxSubKey;              // longest subkey size
        DWORD    cchMaxClass;              // longest class string
        DWORD    cValues;              // number of values for key
        DWORD    cchMaxValue;          // longest value name
        DWORD    cbMaxValueData;       // longest value data
        DWORD    cbSecurityDescriptor; // size of security descriptor
        FILETIME ftLastWriteTime;      // last write time

        DWORD retCode;

        // HKEY hKey;
        // Get the class name and the value count.
        retCode = RegQueryInfoKey(
            hKeyParent,                    // key handle
            achClass,                // buffer for class name
            &cchClassName,           // size of class string
            NULL,                    // reserved
            &cSubKeys,               // number of subkeys
            &cbMaxSubKey,            // longest subkey size
            &cchMaxClass,            // longest class string
            &cValues,                // number of values for this key
            &cchMaxValue,            // longest value name
            &cbMaxValueData,         // longest value data
            &cbSecurityDescriptor,   // security descriptor
            &ftLastWriteTime);       // last write time


        if (cSubKeys)
        {
            for (DWORD i=0; i<cSubKeys; i++)
            {
                TCHAR    achKey[MAX_KEY_LENGTH];   // buffer for subkey name
                DWORD cbName = MAX_KEY_LENGTH;     // size of name string
                retCode = RegEnumKeyEx(hKeyParent, i,
                         achKey,
                         &cbName,
                         NULL,
                         NULL,
                         NULL,
                         &ftLastWriteTime);
                if (retCode == ERROR_SUCCESS)
                {
                    result.append(QString::fromWCharArray(achKey));
                }
            }
        }

        RegCloseKey(hKeyParent);
    }

    return result;
}

types::NetworkInterface WinUtils::interfaceByIndex(int index, bool &success)
{
    types::NetworkInterface result;
    success = false;

    QVector<types::NetworkInterface> nis = currentNetworkInterfaces(true);

    for (int i = 0; i < nis.size(); i++)
    {
        types::NetworkInterface ni = nis[i];

        if (ni.interfaceIndex == index)
        {
            result = ni;
            success = true;
            break;
        }
    }

    return result;
}

QList<IpForwardRow> WinUtils::getIpForwardTable()
{
    QList<IpForwardRow> forwardTable;

    DWORD dwSize = 0;
    if (GetIpForwardTable(NULL, &dwSize, 0) != ERROR_INSUFFICIENT_BUFFER)
    {
        return forwardTable;
    }

    QScopedArrayPointer<unsigned char> buf(new unsigned char[dwSize]);

    /* Note that the IPv4 addresses returned in
     * GetIpForwardTable entries are in network byte order
     */
    if (GetIpForwardTable((PMIB_IPFORWARDTABLE)buf.data(), &dwSize, 0) == NO_ERROR)
    {
        PMIB_IPFORWARDTABLE pIpForwardTable = (PMIB_IPFORWARDTABLE)buf.data();

        for (int i = 0; i < (int) pIpForwardTable->dwNumEntries; i++)
        {
            IpForwardRow ipRow = IpForwardRow(pIpForwardTable->table[i].dwForwardIfIndex,
                                              pIpForwardTable->table[i].dwForwardMetric1
                                              );
            forwardTable.append(ipRow);
        }

        return forwardTable;
    }
    else
    {
        qCDebug(LOG_BASIC) << "GetIpForwardTable failed";
        return forwardTable;
    }
}

QList<IfTableRow> WinUtils::getIfTable()
{
    QList<IfTableRow> table;

    DWORD dwRetVal = 0;

    // Make an initial call to GetIfTable to get the necessary size into dwSize
    DWORD dwSize = 0;
    if (GetIfTable(NULL, &dwSize, FALSE) != ERROR_INSUFFICIENT_BUFFER)
    {
        return table;
    }

    QScopedArrayPointer<unsigned char> buf(new unsigned char[dwSize]);

    // Make a second call to GetIfTable to get the actual data we want.
    if ((dwRetVal = GetIfTable((PMIB_IFTABLE)buf.data(), &dwSize, FALSE)) == NO_ERROR)
    {
        PMIB_IFTABLE pIfTable = (PMIB_IFTABLE)buf.data();

        for (DWORD i = 0; i < pIfTable->dwNumEntries; i++)
        {
            NETWORK_INTERACE_TYPE nicType = NETWORK_INTERFACE_NONE;
            if (pIfTable->table[i].dwType == IF_TYPE_ETHERNET_CSMACD)
            {
                nicType = NETWORK_INTERFACE_ETH;
            }
            else if (pIfTable->table[i].dwType == IF_TYPE_IEEE80211)
            {
                nicType = NETWORK_INTERFACE_WIFI;
            }
            else if (pIfTable->table[i].dwType == IF_TYPE_PPP)
            {
                nicType = NETWORK_INTERFACE_PPP;
            }

//            qDebug() << "Index: " << pIfTable->table[i].dwIndex << ", State: " << pIfTable->table[i].dwOperStatus;

            QString interfaceName = "";
            for (DWORD j = 0; j < pIfTable->table[i].dwDescrLen; j++)
            {
                interfaceName.append(static_cast<char>(pIfTable->table[i].bDescr[j]));
            }

            QString physicalAddress = "";
            for (DWORD j = 0; j < pIfTable->table[i].dwPhysAddrLen; j++)
            {
                physicalAddress.append(static_cast<char>(pIfTable->table[i].bPhysAddr[j]));
            }

            IfTableRow ifRow(static_cast<int>(pIfTable->table[i].dwIndex),
                             QString::fromWCharArray(pIfTable->table[i].wszName),
                             interfaceName,
                             physicalAddress,
                             nicType,
                             static_cast<int>(pIfTable->table[i].dwType),
                             static_cast<int>(pIfTable->table[i].dwMtu),
                             pIfTable->table[i].dwOperStatus >= IF_OPER_STATUS_CONNECTED,
                             (int) pIfTable->table[i].dwOperStatus);
            table.append(ifRow);
        }
    }
    else
    {
        qCDebug(LOG_BASIC) << "GetIfTable failed with error: " << dwRetVal;
    }

    return table;
}

QList<IfTable2Row> WinUtils::getIfTable2()
{
    QList<IfTable2Row> if2Table;

    PMIB_IF_TABLE2 pIfTable2;
    if (GetIfTable2(&pIfTable2) != NO_ERROR)
    {
        return if2Table;
    }

    for (ULONG i = 0; i < pIfTable2->NumEntries; i++)
    {
        PMIB_IF_ROW2 pEntry = &pIfTable2->Table[i];
        if (!pEntry->InterfaceAndOperStatusFlags.FilterInterface)
        {
            QString guid = guidToQString(pEntry->InterfaceGuid);
            QString description = QString::fromWCharArray(pEntry->Description);
            QString alias = QString::fromWCharArray(pEntry->Alias);

            IfTable2Row row(pEntry->InterfaceIndex,
                            guid,
                            description,
                            alias,
                            pEntry->AccessType,
                            pEntry->InterfaceAndOperStatusFlags.ConnectorPresent,
                            pEntry->InterfaceAndOperStatusFlags.EndPointInterface,
                            pEntry->Type);
            if2Table.append(row);
        }
    }

    FreeMibTable(pIfTable2);

    return if2Table;
}

QList<IpAdapter> WinUtils::getIpAdapterTable()
{
    QList<IpAdapter> adapters;

    ULONG bufSize = sizeof(IP_ADAPTER_INFO) * 32;
    QByteArray pAdapterInfo(bufSize, Qt::Uninitialized);

    DWORD result = ::GetAdaptersInfo((IP_ADAPTER_INFO*)pAdapterInfo.data(), &bufSize);

    if (result == ERROR_BUFFER_OVERFLOW) {
        pAdapterInfo.resize(bufSize);
        result = ::GetAdaptersInfo((IP_ADAPTER_INFO*)pAdapterInfo.data(), &bufSize);
    }

    if (result == NO_ERROR) {
        IP_ADAPTER_INFO *pAdapter = (IP_ADAPTER_INFO*)pAdapterInfo.data();
        while (pAdapter) {
            QString physAddress = "";
            for (UINT i = 0; i < pAdapter->AddressLength; i++) {
                QString s = QString("%1").arg(pAdapter->Address[i], 0, 16);

                if (singleHexChars().contains(s)) {
                    s = "0" + s;
                }
                physAddress += s;
            }

            IpAdapter ipAdapter(pAdapter->Index, pAdapter->AdapterName, pAdapter->Description, physAddress);
            adapters.append(ipAdapter);

            pAdapter = pAdapter->Next;
        }
    }
    else {
        qCDebug(LOG_BASIC) << "WinUtils::getIpAdapterTable(): GetAdaptersInfo failed" << result;
    }

    return adapters;
}

QList<AdapterAddress> WinUtils::getAdapterAddressesTable()
{
    QList<AdapterAddress> adapters;

    ULONG bufSize = sizeof(IP_ADAPTER_ADDRESSES_LH) * 32;
    QByteArray pAddresses(bufSize, Qt::Uninitialized);

    ULONG result = ::GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, (PIP_ADAPTER_ADDRESSES)pAddresses.data(), &bufSize);

    if (result == ERROR_BUFFER_OVERFLOW) {
        pAddresses.resize(bufSize);
        result = ::GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, (PIP_ADAPTER_ADDRESSES)pAddresses.data(), &bufSize);
    }

    if (result == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES pCurrAddresses = (PIP_ADAPTER_ADDRESSES)pAddresses.data();
        while (pCurrAddresses) {
            QString guidString = guidToQString(pCurrAddresses->NetworkGuid);
            AdapterAddress aa(pCurrAddresses->IfIndex, guidString, QString::fromWCharArray(pCurrAddresses->FriendlyName));
            adapters.append(aa);

            pCurrAddresses = pCurrAddresses->Next;
        }
    }
    else {
        if (result == ERROR_NO_DATA) {
            qCDebug(LOG_BASIC) << "WinUtils::getAdapterAddressesTable(): GetAdaptersAddresses failed - no addresses were found for the requested parameters";
        }
        else {
            wchar_t strErr[1024];
            WinUtils::Win32GetErrorString(result, strErr, _countof(strErr));
            qCDebug(LOG_BASIC) << "WinUtils::getAdapterAddressesTable(): GetAdaptersAddresses failed (" << result << ")" << strErr;
        }
    }

    return adapters;
}

QString WinUtils::ssidFromInterfaceGUID(QString interfaceGUID)
{
    QString ssid = "";

    HANDLE hClient = NULL;
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    DWORD dwResult = 0;

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;

    PWLAN_CONNECTION_ATTRIBUTES pConnectInfo = NULL;
    DWORD connectInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
    WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;

    dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS)
    {

        qCDebug(LOG_BASIC) << "WlanOpenHandle failed with error: " << dwResult;
        return ssid;
    }

    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS)
    {
        qCDebug(LOG_BASIC) << "WlanEnumInterfaces failed with error:" << dwResult;
        return ssid;
    }
    else
    {
        GUID actualGUID = guidFromQString(interfaceGUID);

        dwResult = WlanQueryInterface(hClient,
                                      &actualGUID,
                                      wlan_intf_opcode_current_connection,
                                      NULL,
                                      &connectInfoSize,
                                      (PVOID *) &pConnectInfo,
                                      &opCode);

        if (dwResult != ERROR_SUCCESS)
        {
            //qCDebug(LOG_BASIC) << "WlanQueryInterface failed with error:" << dwResult;
        }
        else
        {
            std::string str_ssid;
            const auto &dot11Ssid = pConnectInfo->wlanAssociationAttributes.dot11Ssid;
            if (dot11Ssid.uSSIDLength != 0) {
                str_ssid.reserve(dot11Ssid.uSSIDLength);
                for (ULONG k = 0; k < dot11Ssid.uSSIDLength; k++)
                    str_ssid.push_back(static_cast<char>(dot11Ssid.ucSSID[k]));
            }
            // Note: |str_ssid| can contain UTF-8 characters, but QString::fromStdString() can
            // handle the case.
            ssid = QString::fromStdString(str_ssid);
        }
    }
    if (pConnectInfo != NULL)
    {
        WlanFreeMemory(pConnectInfo);
        pConnectInfo = NULL;
    }

    if (pIfList != NULL)
    {
        WlanFreeMemory(pIfList);
        pIfList = NULL;
    }

    if (hClient != NULL)
    {
        WlanCloseHandle(hClient, NULL);
        hClient = NULL;
    }

    return ssid;
}

QString WinUtils::networkNameFromInterfaceGUID(QString adapterGUID)
{
    QString result = "";

    INetworkListManager *pNetListManager = NULL;
    HRESULT hr = CoCreateInstance(CLSID_NetworkListManager, NULL,
                                  CLSCTX_ALL, IID_INetworkListManager,
                                  (LPVOID *)&pNetListManager);
    if (hr == S_OK)
    {
        CComPtr<IEnumNetworkConnections> pEnumNetworkConnections;
        if(SUCCEEDED(pNetListManager->GetNetworkConnections(&pEnumNetworkConnections)))
        {
            DWORD dwReturn = 0;
            while(true)
            {
                CComPtr<INetworkConnection> pNetConnection;
                hr = pEnumNetworkConnections->Next(1, &pNetConnection, &dwReturn);
                if (hr == S_OK && dwReturn > 0)
                {
                    GUID adapterID;
                    if (pNetConnection->GetAdapterId(&adapterID) == S_OK)
                    {
                        QString adapterIDStr = guidToQString(adapterID);

                        if (adapterGUID == adapterIDStr)
                        {
                            CComPtr<INetwork> pNetwork;
                            if (pNetConnection->GetNetwork(&pNetwork) == S_OK)
                            {
                                BSTR bstrName = NULL;
                                if (pNetwork->GetName(&bstrName) == S_OK)
                                {
                                    result = QString::fromWCharArray(bstrName);
                                    SysFreeString(bstrName);
                                }
                            }
                            break;
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }

        pNetListManager->Release();
    }

    return result;
}

QList<QString> WinUtils::singleHexChars()
{
    return QList<QString>() << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7" << "8" << "9" << "a" << "b" << "c" << "d" << "e" << "f";
}

GUID guidFromQString(QString str)
{
    GUID reqGUID;
    unsigned long p0;
    unsigned int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;

    sscanf_s(str.toStdString().c_str(), "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
             &p0, &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10);
    reqGUID.Data1 = p0;
    reqGUID.Data2 = p1;
    reqGUID.Data3 = p2;
    reqGUID.Data4[0] = p3;
    reqGUID.Data4[1] = p4;
    reqGUID.Data4[2] = p5;
    reqGUID.Data4[3] = p6;
    reqGUID.Data4[4] = p7;
    reqGUID.Data4[5] = p8;
    reqGUID.Data4[6] = p9;
    reqGUID.Data4[7] = p10;

    return reqGUID;
}

QString guidToQString(GUID guid)
{
    OLECHAR* guidString;
    StringFromCLSID(guid, &guidString);

    std::wstring str = (guidString);
    QString guidQString = QString::fromWCharArray(str.c_str());

    ::CoTaskMemFree(guidString);

    return guidQString;
}

std::string readAllFromPipe(HANDLE hPipe)
{
    const int BUFFER_SIZE = 1024;
    std::string csoutput;
    while (true)
    {
        char buf[BUFFER_SIZE + 1];
        DWORD readword;
        if (!::ReadFile(hPipe, buf, BUFFER_SIZE, &readword, 0))
        {
            break;
        }
        if (readword == 0)
        {
            break;
        }
        buf[readword] = '\0';
        std::string cstemp = buf;
        csoutput += cstemp;
    }
    return csoutput;
}

QString WinUtils::executeBlockingCmd(QString cmd, const QString & /*params*/, int timeoutMs)
{
    QString result = "";

    SECURITY_ATTRIBUTES sa;
    ZeroMemory(&sa,sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE rPipe, wPipe;
    if (!CreatePipe(&rPipe, &wPipe, &sa, 0))
    {
        return result;
    }

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory( &si, sizeof(si) );
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = NULL;
    si.hStdError = wPipe;
    si.hStdOutput = wPipe;

    ZeroMemory( &pi, sizeof(pi) );
    if (CreateProcess(NULL, (wchar_t *) cmd.toStdWString().c_str(), NULL, NULL, TRUE, CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
    {
        DWORD exitCode;
        if (timeoutMs < 0)
        {
            exitCode = WaitForSingleObject(pi.hProcess, INFINITE);
        }
        else
        {
            exitCode = WaitForSingleObject(pi.hProcess, timeoutMs);
        }

        CloseHandle(wPipe);
        std::string str;
        if (exitCode != WAIT_TIMEOUT)
        {
            str = readAllFromPipe(rPipe);
        }

        CloseHandle(rPipe);
        CloseHandle( pi.hProcess );
        CloseHandle( pi.hThread );

        result = QString::fromStdString(str);
    }
    else
    {
        qCDebug(LOG_BASIC) << "Failed create process: %x", GetLastError();
        CloseHandle(rPipe);
        CloseHandle(wPipe);
    }
    return result;
}

bool WinUtils::pingWithMtu(const QString &url, int mtu)
{
    const QString cmd = QString("C:\\Windows\\system32\\ping.exe");
    const QString params = QString(" -n 1 -l %1 -f %2").arg(mtu).arg(url);
    QString result = executeBlockingCmd(cmd + params, params, 1000).trimmed();
    if (result.contains("bytes="))
    {
        return true;
    }
    return false;
}

QString WinUtils::getLocalIP()
{
    ULONG ulAdapterInfoSize = sizeof(IP_ADAPTER_INFO);
    std::vector<unsigned char> pAdapterInfo(ulAdapterInfoSize);

    if (GetAdaptersInfo((IP_ADAPTER_INFO *)&pAdapterInfo[0], &ulAdapterInfoSize) == ERROR_BUFFER_OVERFLOW) // out of buff
    {
        pAdapterInfo.resize(ulAdapterInfoSize);
    }

    if (GetAdaptersInfo((IP_ADAPTER_INFO *)&pAdapterInfo[0], &ulAdapterInfoSize) == ERROR_SUCCESS)
    {
        IP_ADAPTER_INFO *ai = (IP_ADAPTER_INFO *)&pAdapterInfo[0];

        do
        {
            if ((ai->Type == MIB_IF_TYPE_ETHERNET) || (ai->Type == IF_TYPE_IEEE80211))
            {
                // JDRM - I don't think this check for "Windscribe VPN" is necessary, as the tun/tap adapters are
                // Type = 53 (IF_TYPE_PROP_VIRTUAL).  However, if it is, shouldn't we also be checking for
                // "Windscribe Windtun420" and the wireguard-nt adapters?
                if (strstr(ai->Description, "Windscribe VPN") == 0 && strcmp(ai->IpAddressList.IpAddress.String, "0.0.0.0") != 0
                    && strcmp(ai->GatewayList.IpAddress.String, "0.0.0.0") != 0)
                {
                    return ai->IpAddressList.IpAddress.String;
                }
            }
            ai = ai->Next;
        } while (ai);
    }
    return "";
}

bool WinUtils::isServiceRunning(const QString &serviceName)
{
    SC_HANDLE schSCManager = NULL;
    SC_HANDLE schService = NULL;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (schSCManager == NULL)
    {
        DWORD err = GetLastError();
        qCDebug(LOG_BASIC) << "OpenSCManager failed: " << err;
        return false;
    }

    schService = OpenService(schSCManager, serviceName.toStdWString().c_str(), SERVICE_QUERY_STATUS);
    if (schService == NULL)
    {
        DWORD err = GetLastError();
        qCDebug(LOG_BASIC) << "OpenService for " << serviceName << " failed: " << err;
        CloseServiceHandle(schSCManager);
        return false;
    }

    SERVICE_STATUS_PROCESS ssStatus;
    DWORD dwBytesNeeded;
    if (!QueryServiceStatusEx(schService, SC_STATUS_PROCESS_INFO,
                (LPBYTE) &ssStatus, sizeof(SERVICE_STATUS_PROCESS), &dwBytesNeeded ))
    {
        qCDebug(LOG_BASIC) << "QueryServiceStatusEx for " << serviceName << " failed: " << GetLastError();
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    bool bRet = ssStatus.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return bRet;
}

bool WinUtils::is32BitAppRunningOn64Bit()
{
    // Check if this is a 64-bit app used in a 32-bit app.
#if defined (_M_X64)
    return false;
#else
    BOOL iswow64 = FALSE;
    static bool result = IsWow64Process(GetCurrentProcess(), &iswow64) && iswow64;
    return result;
#endif
}

QString WinUtils::iconPathFromBinPath(const QString &binPath)
{
    QString result = binPath;
    if (is32BitAppRunningOn64Bit()) {
        result.replace("\\", "/");
        if (result.contains("/System32/", Qt::CaseInsensitive) && !QFileInfo(result).exists())
            result.replace("/System32/", "/Sysnative/", Qt::CaseInsensitive);
    }
    return result;
}

HRESULT CoCreateInstanceAsAdmin(HWND hwnd, REFCLSID rclsid, REFIID riid, __out void ** ppv)
{
    BIND_OPTS3 bo;
    WCHAR  wszCLSID[50];
    WCHAR  wszMonikerName[300];

    StringFromGUID2(rclsid, wszCLSID, sizeof(wszCLSID) / sizeof(wszCLSID[0]));
    HRESULT hr = StringCchPrintf(wszMonikerName, sizeof(wszMonikerName) / sizeof(wszMonikerName[0]), L"Elevation:Administrator!new:%s", wszCLSID);
    if (FAILED(hr))
        return hr;
    // std::wcout << L"Moniker name: " << wszMonikerName << std::endl;

    memset(&bo, 0, sizeof(bo));
    bo.cbStruct = sizeof(bo);
    bo.hwnd = hwnd;
    bo.dwClassContext = CLSCTX_LOCAL_SERVER;
    return CoGetObject(wszMonikerName, &bo, riid, ppv);
}

bool WinUtils::authorizeWithUac()
{
    bool result = false;
    CoInitializeEx(0, COINIT_APARTMENTTHREADED);

    IUnknown *pThing = NULL;
    HRESULT hr = CoCreateInstanceAsAdmin(NULL, CLSID_AUTH_HELPER, IID_AUTH_HELPER, (void**)&pThing);
    if (FAILED(hr))
    {
        if (HRESULT_CODE(hr) == ERROR_CANCELLED)
        {
            std::cout << "Authentication failed due to user selection" << std::endl;
        }
        else
        {
            // Can fail here if StubProxyDll isn't in CLSID\InprocServer32
            int facility = HRESULT_FACILITY(hr); // If returns 4 (FACILITY_ITF) then error codes are interface specific
            int errorCode = HRESULT_CODE(hr);
            wchar_t strErr[1024];
            WinUtils::Win32GetErrorString(errorCode, strErr, _countof(strErr));
            std::cout << "Failed to CoCreateInstance of MyThing, facility: " << facility << ", code: " << errorCode << std::endl;
            std::cout << " (" << hr << "): " << strErr << std::endl;
        }
    }
    else
    {
        // CoCreateInstanceAsAdmin will return S_OK if authorization was successful
        std::cout << "Helper process is Authorized" << std::endl;
        pThing->Release();
        result = true;
    }

    CoUninitialize();
    return result;
}

bool WinUtils::isWindows64Bit()
{
    bool is_64_bit = true;

    if (::GetSystemWow64DirectoryW(nullptr, 0u) == FALSE)
    {
        const DWORD last_error = ::GetLastError();
        if (last_error == ERROR_CALL_NOT_IMPLEMENTED) {
            is_64_bit = false;
        }
    }

    return is_64_bit;
}

unsigned long WinUtils::Win32GetErrorString(unsigned long errorCode, wchar_t *buffer, unsigned long bufferSize)
{
    DWORD nLength = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM    |
                                    FORMAT_MESSAGE_IGNORE_INSERTS |
                                    FORMAT_MESSAGE_MAX_WIDTH_MASK,
                                    NULL, errorCode,
                                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                    buffer, bufferSize, NULL);
    if (nLength == 0) {
       nLength = _snwprintf_s(buffer, bufferSize, _TRUNCATE, L"Unknown error code [%lu].", errorCode);
    }

    return nLength;
}

IfTable2Row WinUtils::ifTable2RowByIndex(int index)
{
    IfTable2Row found;

    const auto if_table = getIfTable2();
    for (IfTable2Row row : if_table)
    {
        if (index == static_cast<int>(row.index)) // TODO: convert all ifIndices to int
        {
            found = row;
            break;
        }
    }

    return found;
}

std::optional<bool> WinUtils::haveInternetConnectivity()
{
    INetworkListManager* mgr = nullptr;
    HRESULT res = ::CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL,
                                     IID_INetworkListManager, reinterpret_cast<void**>(&mgr));
    if (res != S_OK) {
        qCDebug(LOG_BASIC) << "WinUtils::haveInternetConnectivity() could not create an INetworkListManager instance" << HRESULT_CODE(res);
        return std::nullopt;
    }

    auto comRelease = qScopeGuard([&]
    {
        if (mgr != nullptr) {
            mgr->Release();
        }
    });

    NLM_CONNECTIVITY connectivity;
    res = mgr->GetConnectivity(&connectivity);

    if (res != S_OK) {
        qCDebug(LOG_BASIC) << "WinUtils::haveInternetConnectivity() GetConnectivity failed" << HRESULT_CODE(res);
        return std::nullopt;
    }

    qCDebug(LOG_BASIC) << "WinUtils::haveInternetConnectivity() GetConnectivity returned" << connectivity;

    if ((connectivity & NLM_CONNECTIVITY_IPV4_INTERNET) || (connectivity & NLM_CONNECTIVITY_IPV6_INTERNET)) {
        return true;
    }

    return false;
}

QString WinUtils::getVersionInfoItem(QString exeName, QString itemName)
{
    QString itemValue;

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate;

    std::wstring binary(exeName.toStdWString());

    DWORD dwNotUsed;
    DWORD dwSize = ::GetFileVersionInfoSize(binary.c_str(), &dwNotUsed);

    if (dwSize > 0) {
        std::unique_ptr<UCHAR[]> versionInfo(new UCHAR[dwSize]);
        BOOL bResult = ::GetFileVersionInfo(binary.c_str(), 0L, dwSize, versionInfo.get());
        if (bResult) {
            // To get a string value must pass query in the form
            // "\StringFileInfo\<langID><codepage>\keyname"
            // where <langID><codepage> is the languageID concatenated with the code page, in hex.
            UINT nTranslateLen;
            bResult = ::VerQueryValue(versionInfo.get(), _T("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &nTranslateLen);

            if (bResult) {
                for (UINT i = 0; i < (nTranslateLen/sizeof(struct LANGANDCODEPAGE)); ++i)
                {
                    wchar_t subBlock[MAX_PATH];
                    swprintf_s(subBlock, MAX_PATH, _T("\\StringFileInfo\\%04x%04x\\%s"), lpTranslate[i].wLanguage,
                               lpTranslate[i].wCodePage, itemName.toStdWString().c_str());

                    LPVOID lpvi;
                    UINT   nLen;
                    bResult = ::VerQueryValue(versionInfo.get(), subBlock, &lpvi, &nLen);

                    if (bResult && nLen > 0) {
                        itemValue = QString::fromStdWString(std::wstring((LPCTSTR)lpvi));
                        break;
                    }
                }
            }
        }
    }

    return itemValue;
}

GUID WinUtils::stringToGuid(const char *str)
{
    GUID guid;
    unsigned long p0;
    unsigned int p1, p2, p3, p4, p5, p6, p7, p8, p9, p10;

    sscanf_s(str, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        &p0, &p1, &p2, &p3, &p4, &p5, &p6, &p7, &p8, &p9, &p10);

    guid.Data1 = p0;
    guid.Data2 = p1;
    guid.Data3 = p2;
    guid.Data4[0] = p3;
    guid.Data4[1] = p4;
    guid.Data4[2] = p5;
    guid.Data4[3] = p6;
    guid.Data4[4] = p7;
    guid.Data4[5] = p8;
    guid.Data4[6] = p9;
    guid.Data4[7] = p10;

    return guid;
}
