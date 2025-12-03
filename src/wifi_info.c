#include <winsock2.h>     // must include before windows.h
#include <windows.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

// Link with Windows libraries (ignored in MinGW)
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

// =================== GLOBAL CONFIG ===================
int ENABLE_LOG = 1; // Set to 0 to disable all logs (quiet mode)

#define LOG(fmt, ...)             \
    do {                          \
        if (ENABLE_LOG) {         \
            printf(fmt, ##__VA_ARGS__); \
        }                         \
    } while (0)
// =====================================================

/**
 * get_wifi_info()
 * ----------------
 * Function: Retrieves current connected Wi-Fi SSID and saved password.
 * Inputs  : None
 * Returns : None (prints to stdout if logging enabled)
 */
void get_wifi_info() {
    DWORD dwMaxClient = 2;
    DWORD dwCurVersion = 0;
    HANDLE hClient = NULL;

    DWORD dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanOpenHandle failed: %lu\n", dwResult);
        return;
    }

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
    if (dwResult != ERROR_SUCCESS) {
        LOG("WlanEnumInterfaces failed: %lu\n", dwResult);
        return;
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
            char ssid[DOT11_SSID_MAX_LENGTH + 1] = {0};
            memcpy(ssid,
                   pConnectInfo->wlanAssociationAttributes.dot11Ssid.ucSSID,
                   pConnectInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength);
            ssid[pConnectInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength] = '\0';

            LOG("Wi-Fi SSID: %s\n", ssid);

            // Get password using netsh
            char command[512];
            snprintf(command, sizeof(command),
                     "netsh wlan show profile name=\"%s\" key=clear", ssid);

            FILE *fp = _popen(command, "r");
            if (fp) {
                char line[512];
                while (fgets(line, sizeof(line), fp)) {
                    if (strstr(line, "Key Content")) {
                        LOG("Wi-Fi Password: %s", line); // includes newline
                    }
                }
                _pclose(fp);
            } else {
                LOG("Failed to run netsh to retrieve password.\n");
            }
        } else {
            LOG("Not connected to Wi-Fi.\n");
        }

        if (pConnectInfo) {
            WlanFreeMemory(pConnectInfo);
        }
    }

    WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);
}

/**
 * get_ip_info()
 * ----------------
 * Function: Retrieves current IP address, subnet mask, and default gateway.
 * Inputs  : None
 * Returns : None (prints to stdout if logging enabled)
 */
void get_ip_info() {
    IP_ADAPTER_INFO *adapterInfo;
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);

    // Handle buffer resizing
    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
    }

    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        IP_ADAPTER_INFO *pAdapter = adapterInfo;
        while (pAdapter) {
            LOG("Adapter: %s\n", pAdapter->Description);
            LOG("  IP Address:      %s\n", pAdapter->IpAddressList.IpAddress.String);
            LOG("  Subnet Mask:     %s\n", pAdapter->IpAddressList.IpMask.String);
            LOG("  Default Gateway: %s\n", pAdapter->GatewayList.IpAddress.String);
            LOG("\n");
            pAdapter = pAdapter->Next;
        }
    } else {
        LOG("GetAdaptersInfo failed.\n");
    }

    free(adapterInfo);
}

/**
 * main()
 * ----------------
 * Function: Entry point of the program.
 * Purpose : Calls Wi-Fi info and network info printing functions.
 */
int main() {
    LOG("===== Wi-Fi Information =====\n");
    get_wifi_info();

    LOG("\n===== Network Information =====\n");
    get_ip_info();

    return 0;
}
