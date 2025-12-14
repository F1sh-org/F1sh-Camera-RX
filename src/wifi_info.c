#include <winsock2.h>
#include <windows.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =================== GLOBAL LOG CONTROL ===================
int ENABLE_LOG = 1; // Set to 0 to disable all logs

#define LOG(fmt, ...) do { if (ENABLE_LOG) printf(fmt, ##__VA_ARGS__); } while (0)
// ==========================================================

// =================== STRUCT DEFINITIONS ===================
typedef struct {
    char ssid[64];
    char password[128];
    int success; // 1 = success, 0 = failed
} WifiInfo;

typedef struct {
    char adapter_name[256];
    char ip[32];
    char subnet[32];
    char gateway[32];
    int success;
} NetworkInfo;
// ==========================================================

/**
 * get_wifi_info()
 * ----------------
 * Description : Retrieves current connected Wi-Fi SSID and password.
 * Inputs      : None
 * Returns     : WifiInfo struct containing SSID, password, and success flag
 */
WifiInfo get_wifi_info() {
    WifiInfo info = { .success = 0 };
    DWORD dwMaxClient = 2, dwCurVersion = 0;
    HANDLE hClient = NULL;

    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanOpenHandle failed: %lu\n", dwResult);
        return info;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanEnumInterfaces failed: %lu\n", dwResult);
        WlanCloseHandle(hClient, NULL);
        return info;
    }

    for (int i = 0; i < (int)pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];

        WLAN_CONNECTION_ATTRIBUTES *pConnectInfo = NULL;
        DWORD connectInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
        WLAN_OPCODE_VALUE_TYPE opCode;

        dwResult = WlanQueryInterface(hClient, &pIfInfo->InterfaceGuid,
                                      wlan_intf_opcode_current_connection, NULL,
                                      &connectInfoSize, (PVOID *)&pConnectInfo,
                                      &opCode);

        if (dwResult == ERROR_SUCCESS &&
            pConnectInfo->isState == wlan_interface_state_connected) {

            // Extract SSID
            int len = pConnectInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
            memcpy(info.ssid,
                   pConnectInfo->wlanAssociationAttributes.dot11Ssid.ucSSID,
                   len);
            info.ssid[len] = '\0';

            LOG("Wi-Fi SSID: %s\n", info.ssid);

            // Extract password using netsh
            char command[512];
            snprintf(command, sizeof(command),
                     "netsh wlan show profile name=\"%s\" key=clear", info.ssid);

            FILE *fp = _popen(command, "r");
            if (fp) {
                char line[512];
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "Key Content")) {
                        char *pos = strchr(line, ':');
                        if (pos) {
                            pos += 2;
                            strncpy(info.password, pos, sizeof(info.password) - 1);
                            info.password[strcspn(info.password, "\r\n")] = 0;
                            LOG("Wi-Fi Password: %s\n", info.password);
                            info.success = 1;
                            break;
                        }
                    }
                }
                _pclose(fp);
            }
        }

        if (pConnectInfo)
            WlanFreeMemory(pConnectInfo);
    }

    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);

    return info;
}

/**
 * get_network_info()
 * ----------------
 * Description : Retrieves IP address, subnet mask, and default gateway.
 * Inputs      : None
 * Returns     : NetworkInfo struct with adapter name, IP, subnet, gateway
 */
NetworkInfo get_network_info() {
    NetworkInfo net = { .success = 0 };

    IP_ADAPTER_INFO *adapterInfo;
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);

    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
    }

    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        IP_ADAPTER_INFO *pAdapter = adapterInfo;
        while (pAdapter) {
            // Only grab the first non-empty IP
            if (strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {
                strncpy(net.adapter_name, pAdapter->Description, sizeof(net.adapter_name) - 1);
                strncpy(net.ip, pAdapter->IpAddressList.IpAddress.String, sizeof(net.ip) - 1);
                strncpy(net.subnet, pAdapter->IpAddressList.IpMask.String, sizeof(net.subnet) - 1);
                strncpy(net.gateway, pAdapter->GatewayList.IpAddress.String, sizeof(net.gateway) - 1);
                net.success = 1;

                LOG("Adapter: %s\n", net.adapter_name);
                LOG("  IP Address:      %s\n", net.ip);
                LOG("  Subnet Mask:     %s\n", net.subnet);
                LOG("  Default Gateway: %s\n", net.gateway);
                break;
            }
            pAdapter = pAdapter->Next;
        }
    } else {
        LOG("GetAdaptersInfo failed.\n");
    }

    free(adapterInfo);
    return net;
}

/**
 * main()
 * ----------------
 * Entry point of the program.
 * Calls get_wifi_info() and get_network_info()
 * and prints final summary if logging is enabled.
 */
int main() {
    LOG("===== Wi-Fi Info =====\n");
    WifiInfo wifi = get_wifi_info();

    LOG("\n===== Network Info =====\n");
    NetworkInfo net = get_network_info();

    // Example: use return values
    if (!ENABLE_LOG) {
        if (wifi.success) {
            printf("Connected to SSID: %s\n", wifi.ssid);
            printf("Wi-Fi Password: %s\n", wifi.password);
        } else {
            printf("Wi-Fi: Not connected or failed.\n");
        }

        if (net.success) {
            printf("Adapter: %s\n", net.adapter_name);
            printf("IP: %s\nSubnet: %s\nGateway: %s\n", net.ip, net.subnet, net.gateway);
        } else {
            printf("Network info unavailable.\n");
        }
    }

    return 0;
}
#include <winsock2.h>
#include <windows.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =================== GLOBAL LOG CONTROL ===================
int ENABLE_LOG = 1; // Set to 0 to disable all logs

#define LOG(fmt, ...) do { if (ENABLE_LOG) printf(fmt, ##__VA_ARGS__); } while (0)
// ==========================================================

// =================== STRUCT DEFINITIONS ===================
typedef struct {
    char ssid[64];
    char password[128];
    int success; // 1 = success, 0 = failed
} WifiInfo;

typedef struct {
    char adapter_name[256];
    char ip[32];
    char subnet[32];
    char gateway[32];
    int success;
} NetworkInfo;
// ==========================================================

/**
 * get_wifi_info()
 * ----------------
 * Description : Retrieves current connected Wi-Fi SSID and password.
 * Inputs      : None
 * Returns     : WifiInfo struct containing SSID, password, and success flag
 */
WifiInfo get_wifi_info() {
    WifiInfo info = { .success = 0 };
    DWORD dwMaxClient = 2, dwCurVersion = 0;
    HANDLE hClient = NULL;

    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanOpenHandle failed: %lu\n", dwResult);
        return info;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanEnumInterfaces failed: %lu\n", dwResult);
        WlanCloseHandle(hClient, NULL);
        return info;
    }

    for (int i = 0; i < (int)pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];

        WLAN_CONNECTION_ATTRIBUTES *pConnectInfo = NULL;
        DWORD connectInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);
        WLAN_OPCODE_VALUE_TYPE opCode;

        dwResult = WlanQueryInterface(hClient, &pIfInfo->InterfaceGuid,
                                      wlan_intf_opcode_current_connection, NULL,
                                      &connectInfoSize, (PVOID *)&pConnectInfo,
                                      &opCode);

        if (dwResult == ERROR_SUCCESS &&
            pConnectInfo->isState == wlan_interface_state_connected) {

            // Extract SSID
            int len = pConnectInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
            memcpy(info.ssid,
                   pConnectInfo->wlanAssociationAttributes.dot11Ssid.ucSSID,
                   len);
            info.ssid[len] = '\0';

            LOG("Wi-Fi SSID: %s\n", info.ssid);

            // Extract password using netsh
            char command[512];
            snprintf(command, sizeof(command),
                     "netsh wlan show profile name=\"%s\" key=clear", info.ssid);

            FILE *fp = _popen(command, "r");
            if (fp) {
                char line[512];
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "Key Content")) {
                        char *pos = strchr(line, ':');
                        if (pos) {
                            pos += 2;
                            strncpy(info.password, pos, sizeof(info.password) - 1);
                            info.password[strcspn(info.password, "\r\n")] = 0;
                            LOG("Wi-Fi Password: %s\n", info.password);
                            info.success = 1;
                            break;
                        }
                    }
                }
                _pclose(fp);
            }
        }

        if (pConnectInfo)
            WlanFreeMemory(pConnectInfo);
    }

    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);

    return info;
}

/**
 * get_network_info()
 * ----------------
 * Description : Retrieves IP address, subnet mask, and default gateway.
 * Inputs      : None
 * Returns     : NetworkInfo struct with adapter name, IP, subnet, gateway
 */
NetworkInfo get_network_info() {
    NetworkInfo net = { .success = 0 };

    IP_ADAPTER_INFO *adapterInfo;
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);

    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
    }

    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        IP_ADAPTER_INFO *pAdapter = adapterInfo;
        while (pAdapter) {
            // Only grab the first non-empty IP
            if (strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {
                strncpy(net.adapter_name, pAdapter->Description, sizeof(net.adapter_name) - 1);
                strncpy(net.ip, pAdapter->IpAddressList.IpAddress.String, sizeof(net.ip) - 1);
                strncpy(net.subnet, pAdapter->IpAddressList.IpMask.String, sizeof(net.subnet) - 1);
                strncpy(net.gateway, pAdapter->GatewayList.IpAddress.String, sizeof(net.gateway) - 1);
                net.success = 1;

                LOG("Adapter: %s\n", net.adapter_name);
                LOG("  IP Address:      %s\n", net.ip);
                LOG("  Subnet Mask:     %s\n", net.subnet);
                LOG("  Default Gateway: %s\n", net.gateway);
                break;
            }
            pAdapter = pAdapter->Next;
        }
    } else {
        LOG("GetAdaptersInfo failed.\n");
    }

    free(adapterInfo);
    return net;
}

/**
 * main()
 * ----------------
 * Entry point of the program.
 * Calls get_wifi_info() and get_network_info()
 * and prints final summary if logging is enabled.
 */
int main() {
    LOG("===== Wi-Fi Info =====\n");
    WifiInfo wifi = get_wifi_info();

    LOG("\n===== Network Info =====\n");
    NetworkInfo net = get_network_info();

    // Example: use return values
    if (!ENABLE_LOG) {
        if (wifi.success) {
            printf("Connected to SSID: %s\n", wifi.ssid);
            printf("Wi-Fi Password: %s\n", wifi.password);
        } else {
            printf("Wi-Fi: Not connected or failed.\n");
        }

        if (net.success) {
            printf("Adapter: %s\n", net.adapter_name);
            printf("IP: %s\nSubnet: %s\nGateway: %s\n", net.ip, net.subnet, net.gateway);
        } else {
            printf("Network info unavailable.\n");
        }
    }

    return 0;
}
