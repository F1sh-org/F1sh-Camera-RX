/*
 * wifi_info.c
 *
 * Exposes get_wifi_info() and get_network_info() for the application.
 * The original file contained a standalone test `main()` and a duplicate
 * copy; that test code was removed so this file builds cleanly as part of
 * the application.
 */

#include "f1sh_camera_rx.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#include <winsock2.h>
#include <windows.h>
#include <wlanapi.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ENABLE_LOG = 1;
#define LOG(fmt, ...) do { if (ENABLE_LOG) printf(fmt, ##__VA_ARGS__); } while (0)

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

            int len = pConnectInfo->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
            if (len > (int)sizeof(info.ssid) - 1) len = (int)sizeof(info.ssid) - 1;
            memcpy(info.ssid,
                   pConnectInfo->wlanAssociationAttributes.dot11Ssid.ucSSID,
                   len);
            info.ssid[len] = '\0';

            // Attempt to read clear-key via netsh (best-effort)
            char command[512];
            snprintf(command, sizeof(command), "netsh wlan show profile name=\"%s\" key=clear", info.ssid);
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

NetworkInfo get_network_info() {
    NetworkInfo net = { .success = 0 };

    IP_ADAPTER_INFO *adapterInfo = NULL;
    DWORD bufLen = sizeof(IP_ADAPTER_INFO);
    adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
    if (!adapterInfo) return net;

    if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        free(adapterInfo);
        adapterInfo = (IP_ADAPTER_INFO *)malloc(bufLen);
        if (!adapterInfo) return net;
    }

    if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
        IP_ADAPTER_INFO *pAdapter = adapterInfo;
        while (pAdapter) {
            if (strcmp(pAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {
                strncpy(net.adapter_name, pAdapter->Description, sizeof(net.adapter_name) - 1);
                strncpy(net.ip, pAdapter->IpAddressList.IpAddress.String, sizeof(net.ip) - 1);
                strncpy(net.subnet, pAdapter->IpAddressList.IpMask.String, sizeof(net.subnet) - 1);
                strncpy(net.gateway, pAdapter->GatewayList.IpAddress.String, sizeof(net.gateway) - 1);
                net.success = 1;
                break;
            }
            pAdapter = pAdapter->Next;
        }
    }

    free(adapterInfo);
    return net;
}

#else

/* Non-Windows stubs so the file builds on other platforms. */
WifiInfo get_wifi_info(void) {
    WifiInfo info = { .success = 0 };
    return info;
}

NetworkInfo get_network_info(void) {
    NetworkInfo net = { .success = 0 };
    return net;
}

#endif

/* Test `main()` removed: use get_wifi_info() and get_network_info() from the application */
