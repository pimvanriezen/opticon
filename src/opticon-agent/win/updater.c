#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

// Need to include windows.h before winhttp.h to avoid missing declarations
#include <windows.h>
#include <winhttp.h>
//#include <wininet.h> // Incompatible with winhttp.h

#include <inttypes.h>
#include <stdio.h>

#include <libopticon/popen.h>
#include <sys/select.h>
#include <unistd.h>

#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/log.h>

#include "updater.h"
#include "../version.h"

// @note Manually declaring some stuff found in wininet.h, because wininet.h cannot be used together with winhttp.h because they both declare some of the same things:
// https://social.msdn.microsoft.com/Forums/en-US/8f468d9f-3f15-452c-803d-fc63ab3f684e/cannot-use-both-ltwininethgt-and-ltwinhttphgt?forum=windowssdk
typedef struct {
  DWORD           dwStructSize;
  LPSTR           lpszScheme;
  DWORD           dwSchemeLength;
  INTERNET_SCHEME nScheme;
  LPSTR           lpszHostName;
  DWORD           dwHostNameLength;
  INTERNET_PORT   nPort;
  LPSTR           lpszUserName;
  DWORD           dwUserNameLength;
  LPSTR           lpszPassword;
  DWORD           dwPasswordLength;
  LPSTR           lpszUrlPath;
  DWORD           dwUrlPathLength;
  LPSTR           lpszExtraInfo;
  DWORD           dwExtraInfoLength;
} URL_COMPONENTSA, *LPURL_COMPONENTSA;

BOOLAPI InternetCrackUrlA(LPCSTR lpszUrl,DWORD dwUrlLength,DWORD dwFlags,LPURL_COMPONENTSA lpUrlComponents);



typedef enum DInstallType {
    INSTALLTYPE_UPDATE,         // This does not force a reinstall if the exact same version is already installed
    INSTALLTYPE_INSTALLLATEST   // This forces a reinstall even if the exact same version is already installed
} DInstallType;


uint32_t loadUrlAlloc(const char *url, void **outBuffer) {
    uint32_t e;
    
    // @note Can't get InternetCrackUrlA to work without user provided buffers
    char scheme[256] = {0};
    char hostname[256] = {0};
    char path[1024] = {0};
    char extra[512] = {0};
    URL_COMPONENTSA urlComponents;
    urlComponents.dwStructSize = sizeof(urlComponents);
    urlComponents.lpszScheme = scheme;
    urlComponents.dwSchemeLength = sizeof(scheme);
    urlComponents.lpszHostName = hostname;
    urlComponents.dwHostNameLength = sizeof(hostname);
    urlComponents.lpszUserName = NULL;
    urlComponents.dwUserNameLength = 0;
    urlComponents.lpszPassword = NULL;
    urlComponents.dwPasswordLength = 0;
    urlComponents.lpszUrlPath = path;
    urlComponents.dwUrlPathLength = sizeof(path);
    urlComponents.lpszExtraInfo = extra;
    urlComponents.dwExtraInfoLength = sizeof(extra);
    // While WinHttpCrackUrl is available and seems identical to InternetCrackUrlW, it appears to fail on non http schemes, which is just stupid
    if (InternetCrackUrlA(url, strlen(url), ICU_DECODE, &urlComponents) == false) return GetLastError();
    
    
    
    //printf("url: %s://%s%s\n", scheme, hostname, path);
    //printf("extra: %s\n", extra);
    
    char urlFullResource[1024];
    snprintf(urlFullResource, sizeof(urlFullResource), "%s%s", path, extra);
    
    wchar_t hostnameWide[512];
    // Pass -1 for cbMultiByte to tell it it's a null terminated string
    MultiByteToWideChar(CP_UTF8, 0, hostname, -1, hostnameWide, sizeof(hostnameWide) / sizeof(wchar_t));
    
    wchar_t urlFullResourceWide[2048];
    // Pass -1 for cbMultiByte to tell it it's a null terminated string
    MultiByteToWideChar(CP_UTF8, 0, urlFullResource, -1, urlFullResourceWide, sizeof(urlFullResourceWide) / sizeof(wchar_t));
    
    
    // @note Windows 2008 fails with ERROR_INVALID_PARAMETER (87 / 0x57) if we pass WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY as the second argument
    //HINTERNET session = WinHttpOpen(L"WinHTTP/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET session = WinHttpOpen(L"WinHTTP/1.0", 0, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == NULL) return GetLastError();
    
    // HINTERNET connection = WinHttpConnect(session, L"www.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET connection = WinHttpConnect(session, hostnameWide, urlComponents.nPort, 0);
    if (connection == NULL) { e = GetLastError(); goto error_1; }
    
    DWORD flags = 0;
    if (strcmp(scheme, "https") == 0) flags = WINHTTP_FLAG_SECURE;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", urlFullResourceWide, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == NULL) { e = GetLastError(); goto error_2; }
    
    // @note Windows 2008 fails with: ERROR_WINHTTP_SECURE_FAILURE (0x2f8f) (because tls1.2 isn't on by default, and if it's on, it doesn't like the certificate)
    // @note Windows 2012 fails with: ERROR_WINHTTP_SECURE_FAILURE (0x2f8f) (because tls1.2 isn't on by default)
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == false) { e = GetLastError(); goto error_3; }
    
    if (WinHttpReceiveResponse(request, NULL) == false) { e = GetLastError(); goto error_3; }
    
    //printf("Error %" PRIx32 "\n", GetLastError());
    
    
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) { e = GetLastError(); goto error_3; }
    
    size_t bufferSize = 0;
    void *buffer = NULL;
    while (true) {
        DWORD numberOfBytesAvailable;
        if (WinHttpQueryDataAvailable(request, &numberOfBytesAvailable) == false) {
            printf("Error %" PRIx32 " in WinHttpQueryDataAvailable.\n", (uint32_t)GetLastError());
            e = GetLastError();
            goto error_3;
        }
        
        if (numberOfBytesAvailable == 0) {
            // Transfer is complete
            break;
        }
        
        
        //printf("\n\nnumberOfBytesAvailable: %" PRIu32 "\n", (uint32_t)numberOfBytesAvailable);
        
        size_t bufferSizeNeeded = numberOfBytesAvailable + 1; // + 1 for the null terminator
        if (bufferSizeNeeded > bufferSize) {
            bufferSize = bufferSizeNeeded;
            if (buffer != NULL) {
                void *newBuffer = HeapReAlloc(heap, 0, buffer, bufferSize);
                if (newBuffer == NULL) { e = GetLastError(); goto error_4; }
                buffer = newBuffer;
            }
            else {
                buffer = HeapAlloc(heap, 0, bufferSize);
                if (buffer == NULL) { e = GetLastError(); goto error_3; }
            }
        }
        
        // Zero memory so any byte directly after the read data is a null terminator
        ZeroMemory(buffer, bufferSize);
        
        DWORD numberOfBytesRead;
        if (WinHttpReadData(request, buffer, numberOfBytesAvailable, &numberOfBytesRead) == false) {
            printf("Error %" PRIx32 " in WinHttpReadData.\n", (uint32_t)GetLastError());
            e = GetLastError();
            goto error_4;
        }
        
        //printf("numberOfBytesRead: %" PRIu32 "\n\n", (uint32_t)numberOfBytesRead);
        //printf("%s", (char *)buffer);
    }
    
    *outBuffer = buffer;
    
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    
    return 0;
    
error_4:
    HeapFree(heap, 0, buffer);
error_3:
    WinHttpCloseHandle(request);
error_2:
    WinHttpCloseHandle(connection);
error_1:
    WinHttpCloseHandle(session);
    
    return e;
}

uint32_t freeLoadUrl(void *buffer) {
    HANDLE heap = GetProcessHeap();
    if (heap == NULL) return GetLastError();
    HeapFree(heap, 0, buffer);
    
    return 0;
}

uint32_t downloadUrl(const char *url, const char *filePath) {
    uint32_t e;
    
    // @note Can't get InternetCrackUrlA to work without user provided buffers
    char scheme[256] = {0};
    char hostname[256] = {0};
    char path[1024] = {0};
    char extra[512] = {0};
    URL_COMPONENTSA urlComponents;
    urlComponents.dwStructSize = sizeof(urlComponents);
    urlComponents.lpszScheme = scheme;
    urlComponents.dwSchemeLength = sizeof(scheme);
    urlComponents.lpszHostName = hostname;
    urlComponents.dwHostNameLength = sizeof(hostname);
    urlComponents.lpszUserName = NULL;
    urlComponents.dwUserNameLength = 0;
    urlComponents.lpszPassword = NULL;
    urlComponents.dwPasswordLength = 0;
    urlComponents.lpszUrlPath = path;
    urlComponents.dwUrlPathLength = sizeof(path);
    urlComponents.lpszExtraInfo = extra;
    urlComponents.dwExtraInfoLength = sizeof(extra);
    if (InternetCrackUrlA(url, strlen(url), ICU_DECODE, &urlComponents) == false) return GetLastError();
    
    
    //HANDLE downloadFileHandle = CreateFileA("D:\\moveConfigFileFromExistingInstall.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    HANDLE downloadFileHandle = CreateFileA(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (downloadFileHandle == INVALID_HANDLE_VALUE) return GetLastError();
    
    
    char urlFullResource[1024];
    snprintf(urlFullResource, sizeof(urlFullResource), "%s%s", path, extra);
    
    wchar_t hostnameWide[512];
    // Pass -1 for cbMultiByte to tell it it's a null terminated string
    MultiByteToWideChar(CP_UTF8, 0, hostname, -1, hostnameWide, sizeof(hostnameWide) / sizeof(wchar_t));
    
    wchar_t urlFullResourceWide[2048];
    // Pass -1 for cbMultiByte to tell it it's a null terminated string
    MultiByteToWideChar(CP_UTF8, 0, urlFullResource, -1, urlFullResourceWide, sizeof(urlFullResourceWide) / sizeof(wchar_t));
    
    
    // @note Windows 2008 fails with ERROR_INVALID_PARAMETER (87 / 0x57) if we pass WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY as the second argument
    //HINTERNET session = WinHttpOpen(L"WinHTTP/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET session = WinHttpOpen(L"WinHTTP/1.0", 0, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == NULL) { e = GetLastError(); goto error_1; }
    
    HINTERNET connection = WinHttpConnect(session, hostnameWide, urlComponents.nPort, 0);
    if (connection == NULL) { e = GetLastError(); goto error_2; }

    DWORD flags = 0;
    if (strcmp(scheme, "https") == 0) flags = WINHTTP_FLAG_SECURE;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", urlFullResourceWide, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (request == NULL) { e = GetLastError(); goto error_3; }
    
    if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) == false) { e = GetLastError(); goto error_4; }
    
    if (WinHttpReceiveResponse(request, NULL) == false) { e = GetLastError(); goto error_4; }
    
    
// @todo check http != 200 response
    
    //printf("Error %" PRIx32 "\n", GetLastError());
    
    char buffer[4096];
    size_t bufferSize = sizeof(buffer);
    while (true) {
        // The amount of data that remains is not recalculated until all available data indicated by the call to WinHttpQueryDataAvailable is read.
        DWORD numberOfBytesAvailable;
        if (WinHttpQueryDataAvailable(request, &numberOfBytesAvailable) == false) {
            printf("Error %" PRIx32 " in WinHttpQueryDataAvailable.\n", (uint32_t)GetLastError());
            e = GetLastError();
            goto error_4;
        }
        
        if (numberOfBytesAvailable == 0) {
            // Transfer is complete
            break;
        }
        
        
        //printf("-\n");
        //printf("numberOfBytesAvailable: %" PRIu32 "\n", (uint32_t)numberOfBytesAvailable);
        
        while (numberOfBytesAvailable > 0) {
            // Cap nr of bytes to read to the buffer size
            DWORD numberOfBytesToRead = numberOfBytesAvailable;
            if (numberOfBytesAvailable > bufferSize) numberOfBytesToRead = bufferSize;
            
            // Zero memory so any byte directly after the read data is a null terminator
            ZeroMemory(buffer, sizeof(buffer));
            
            //printf("numberOfBytesToRead: %" PRIu32 "\n", (uint32_t)numberOfBytesToRead);
            
            DWORD numberOfBytesRead;
            if (WinHttpReadData(request, buffer, numberOfBytesToRead, &numberOfBytesRead) == false) {
                printf("Error %" PRIx32 " in WinHttpReadData.\n", (uint32_t)GetLastError());
                e = GetLastError();
                goto error_4;
            }
            
            //printf("numberOfBytesRead: %" PRIu32 "\n", (uint32_t)numberOfBytesRead);
            
            numberOfBytesAvailable -= numberOfBytesRead;
            
            DWORD numberOfBytesWritten;
            if (WriteFile(downloadFileHandle, buffer, numberOfBytesRead, &numberOfBytesWritten, NULL) == false) {
                e = GetLastError();
                goto error_4;
            }
            // @todo check if numberOfBytesWritten == numberOfBytesRead?
        }
    }
    
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    CloseHandle(downloadFileHandle);
    
    return 0;
    
error_4:
    WinHttpCloseHandle(request);
error_3:
    WinHttpCloseHandle(connection);
error_2:
    WinHttpCloseHandle(session);
error_1:
    CloseHandle(downloadFileHandle);
    
    return e;
}






uint32_t checkUpdate(const char *url, const char *channel) {
    uint32_t e;
    
    log_info("Checking for update of the opticon agent at: %s in channel %s", url, channel);
    
    char *json;
    if ((e = loadUrlAlloc(url, (void **)&json))) {
        log_error("Failed to load update url: %s", url);
        return e;
    }
    
    var *versionsVar = var_alloc();
    if (var_parse_json(versionsVar, json) == 0) { e = 0xcde269d2; goto error_1; }
    
    var *allChannelDict = var_find_key(versionsVar, "channel");
    if (allChannelDict == NULL) {
        log_error("Did not find any channels");
        e = 0xf55cf603;
        goto error_1;
    }
    
    var *chosenChannelDict = var_find_key(allChannelDict, channel);
    if (chosenChannelDict == NULL) {
        log_error("Did not find channel: %s", channel);
        e = 0xa63bcc8d;
        goto error_1;
    }
    
    var *targetVersionVar = var_find_key(chosenChannelDict, "version");
    if (targetVersionVar == NULL) {
        log_error("Did not find any latest version");
        e = 0xecdf55c6;
        goto error_1;
    }
    
    const char *targetVersion = var_get_str(targetVersionVar);
    if (targetVersion == NULL) {
        e = 0xa32051d3;
        goto error_1;
    }
    
    if (strcmp(targetVersion, VERSION) == 0) {
        log_info("No new version available");
    }
    else {
        log_info("Found new version: %s", targetVersion);
    }
    
    var_free(versionsVar);
    freeLoadUrl(json);
    
    return 0;
    
error_1:
    var_free(versionsVar);
    freeLoadUrl(json);
    
    return e;
}

static uint32_t _install(const char *url, const char *channel, DInstallType installType) {
    uint32_t e;
    
    // @todo request/response headers + http response status code != 200
    
    if (installType == INSTALLTYPE_UPDATE) {
        log_info("Checking for update of the opticon agent at: %s in channel %s", url, channel);
    }
    else if (installType == INSTALLTYPE_INSTALLLATEST) {
        log_info("Getting latest version of the opticon agent at: %s in channel %s", url, channel);
    }
    
    // @note Can't get InternetCrackUrlA to work without user provided buffers
    char scheme[256] = {0};
    char hostname[256] = {0};
    char path[1024] = {0};
    char extra[512] = {0};
    URL_COMPONENTSA urlComponents;
    urlComponents.dwStructSize = sizeof(urlComponents);
    urlComponents.lpszScheme = scheme;
    urlComponents.dwSchemeLength = sizeof(scheme);
    urlComponents.lpszHostName = hostname;
    urlComponents.dwHostNameLength = sizeof(hostname);
    urlComponents.lpszUserName = NULL;
    urlComponents.dwUserNameLength = 0;
    urlComponents.lpszPassword = NULL;
    urlComponents.dwPasswordLength = 0;
    urlComponents.lpszUrlPath = path;
    urlComponents.dwUrlPathLength = sizeof(path);
    urlComponents.lpszExtraInfo = extra;
    urlComponents.dwExtraInfoLength = sizeof(extra);
    if (InternetCrackUrlA(url, strlen(url), ICU_DECODE, &urlComponents) == false) {
        log_error("Invalid update url");
        return GetLastError();
    }
    
    bool isMsiInstall = false;
    
    // The msi product code is generated while building the msi and also must be different for different languages/versions/builds/whatever, so we don't know it
    // We do manually store it in the registry during installation
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "Software\\Opticon-Agent", "msiProductCode", RRF_RT_REG_SZ, NULL, NULL, NULL) == ERROR_SUCCESS) {
        isMsiInstall = true;
    }
    
    if (!isMsiInstall) {
        log_error("Updater does not support non-MSI installations yet");
        return 0x2c0d6c32;
    }
    
    // @note Assume if the registry value exists, the msi is installed, no real need to actually check the msi database
    
    //TCHAR szVersion[20];
    //DWORD cchVersion = 20;
    //BOOL fInstalled = (ERROR_SUCCESS == MsiGetProductInfoEx (TEXT("{ProductCode}"), NULL, MSIINSTALLCONTEXT_MACHINE, INSTALLPROPERTY_VERSIONSTRING, szVersion, &cchVersion));
    
    
    char *json;
    if ((e = loadUrlAlloc(url, (void **)&json))) {
        log_error("Failed to load update url: %s", url);
        return e;
    }
    //printf("\n%s\n", json);
    
    var *versionsVar = var_alloc();
    if (var_parse_json(versionsVar, json) == 0) { e = 0xcde269d2; goto error_1; }
    
    var *allChannelDict = var_find_key(versionsVar, "channel");
    if (allChannelDict == NULL) {
        log_error("Did not find any channels");
        e = 0xf55cf603;
        goto error_1;
    }
    
    var *chosenChannelDict = var_find_key(allChannelDict, channel);
    if (chosenChannelDict == NULL) {
        log_error("Did not find channel: %s", channel);
        e = 0xa63bcc8d;
        goto error_1;
    }
    
    var *targetVersionVar = var_find_key(chosenChannelDict, "version");
    if (targetVersionVar == NULL) {
        log_error("Did not find any latest version");
        e = 0xecdf55c6;
        goto error_1;
    }
    
    const char *targetVersion = var_get_str(targetVersionVar);
    if (targetVersion == NULL) {
        e = 0xa32051d3;
        goto error_1;
    }
    
    bool doInstall = false;
    if (installType == INSTALLTYPE_UPDATE) {
        if (strcmp(targetVersion, VERSION) == 0) {
            log_info("No new version available");
        }
        else {
            log_info("Found new version: %s", targetVersion);
            doInstall = true;
        }
    }
    else if (installType == INSTALLTYPE_INSTALLLATEST) {
        doInstall = true;
    }
    
    if (doInstall) {
        var *msiVar = var_find_key(chosenChannelDict, "msi");
        if (msiVar == NULL) {
            log_error("Did not find any msi name");
            e = 0x806ed4a9;
            goto error_1;
        }
        
        const char *msiFileName = var_get_str(msiVar);
        if (msiFileName == NULL) {
            log_error("Did not find any msi name");
            e = 0x8b58b278;
            goto error_1;
        }
        
        // Turn the last slash in the path into a null terminator to strip everything that comes after
        char *lastSlash = strrchr(path, '/');
        if (lastSlash != NULL) *lastSlash = '\0';
        
        char msiUrl[256];
        // @todo port?
        snprintf(msiUrl, sizeof(msiUrl), "%s://%s%s/%s", scheme, hostname, path, msiFileName);
        
        
        // @note Apparently there's no guarantee GetTempPathA gives a valid path
        char tempFolderPath[MAX_PATH];
        DWORD ret = GetTempPathA(sizeof(tempFolderPath), tempFolderPath);
        if (ret > MAX_PATH || ret == 0) { e = GetLastError(); goto error_1; }
        
        char tempFilePath[MAX_PATH];
        if (GetTempFileNameA(tempFolderPath, "opticonAgentMsi", 0, tempFilePath) == 0) return GetLastError();
        
        log_info("Downloading MSI: %s", msiUrl);
        
        if ((e = downloadUrl(msiUrl, tempFilePath))) {
            log_error("Failed to download msi");
            DeleteFileA(tempFilePath);
            goto error_1;
        }
        
        log_info("MSI download complete");
        log_debug("MSI downloaded at: %s", tempFilePath);
        
        // Need a little sleep to avoid "triggering" the service manager that feels the service stopped too soon after starting (only if the service has just started)
        //sleep(5);
        
        char msiInstallCommand[256];
        snprintf(msiInstallCommand, sizeof(msiInstallCommand), "msiexec /quiet /i \"%s\"", tempFilePath);
        
        log_info("Installing MSI");
        log_debug("Running command: %s", msiInstallCommand);
        
        char buffer[4096];
        FILE *p = popen_safe(msiInstallCommand, "r");
        while (!feof(p)) {
            // @note Msi stops the service which doesn't really quit gracefully, but does an ExitProcess, so will stop this while loop
            size_t result = fread(buffer, 1, sizeof(buffer), p);
            (void)result;
        }
        int pret = pclose_safe(p);
        int status = WEXITSTATUS(pret);
        
        // @note If we made it this far, it means the msi hasn't stopped our service (or we are not ourselves running as that service)
        DeleteFileA(tempFilePath);
        
        if (status != 0) {
            log_error("MSI install failed with code: %i (be sure to run as administrator)", status);
            e = 0x33b73137;
            goto error_1;
        }
    }
    
    var_free(versionsVar);
    freeLoadUrl(json);
    
    return 0;
    
error_1:
    var_free(versionsVar);
    freeLoadUrl(json);
    
    return e;
}

uint32_t checkAndInstallUpdate(const char *url, const char *channel) {
    return _install(url, channel, INSTALLTYPE_UPDATE);
}

uint32_t installLatest(const char *url, const char *channel) {
    return _install(url, channel, INSTALLTYPE_INSTALLLATEST);
}