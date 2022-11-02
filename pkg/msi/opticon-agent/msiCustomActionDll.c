#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <windows.h>
#include <msi.h>
#include <msiquery.h>

#include <stdio.h>
#include <string.h>

#include <libopticon/ioport.h>
#include <libopticon/ioport_file.h>
#include <libopticon/var.h>
#include <libopticon/var_dump.h>
#include <libopticon/var_parse.h>

//#pragma comment(linker, "/EXPORT:writeConfig=_writeConfigValues@4")


typedef struct DKeyVarMappingEntry {
    const char *key;
    char **var;
} DKeyVarMappingEntry;

/**
 *    @usage
        char *installPath = NULL;
        char *tenantId = NULL;
        char *tenantKey = NULL;
        DKeyVarMappingEntry keyVarMapping[] = {
            {"installPath", &installPath},
            {"tenantId", &tenantId},
            {"tenantKey", &tenantKey},
            {NULL, NULL}
        };
**/
uint32_t parseSemicolonSeparatedKeyValueStringToVars(char *string, DKeyVarMappingEntry *keyVarMapping) {
    // @note the string pointer and its target buffer are mutated by strsep
    
    char *keyValuePair;
    while ((keyValuePair = strsep(&string, ";")) != NULL) {
        char *key = strsep(&keyValuePair, "=");
        if (key == NULL) return 0xec9f1591;
        
        char *value = keyValuePair;
        if (value == NULL) return 0xbe2407f6;
        
        while (keyVarMapping->key != NULL) {
            DKeyVarMappingEntry *keyVarMappingEntry = keyVarMapping;
            ++keyVarMapping; // Point to the next entry
            if (strcmp(key, keyVarMappingEntry->key) != 0) continue;
            *keyVarMappingEntry->var = value;
            break;
        }
    }
    
    return 0;
}

uint32_t parseCustomActionData(MSIHANDLE session, char *customActionDataBuffer, uint32_t customActionDataBufferSize, DKeyVarMappingEntry *keyVarMapping) {
    uint32_t e;
    
    DWORD customActionDataSizeOrRequired = customActionDataBufferSize;
    if (MsiGetPropertyA(session, "CustomActionData", customActionDataBuffer, &customActionDataSizeOrRequired) != ERROR_SUCCESS) return 0xb7a78978;
    
    // @note the customActionDataString pointer and customActionDataBuffer are mutated by strsep
    char *customActionDataString = customActionDataBuffer;
    if ((e = parseSemicolonSeparatedKeyValueStringToVars(customActionDataString, keyVarMapping))) return e;
    
    return 0;
}

__declspec(dllexport) UINT __stdcall debugProperties(MSIHANDLE session) {
    //uint32_t e;
    
    // @todo maybe write an error log file next to the msi?
    HANDLE debugFileHandle = CreateFileA("D:\\debugProperties.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD numberOfBytesWritten;
    
    char customActionDataBuffer[1024];
    DWORD customActionDataSizeOrRequired = sizeof(customActionDataBuffer);
    if (MsiGetPropertyA(session, "CustomActionData", customActionDataBuffer, &customActionDataSizeOrRequired) != ERROR_SUCCESS) return 0xb7a78978;
    
    WriteFile(debugFileHandle, "1:", 2, &numberOfBytesWritten, NULL);
    WriteFile(debugFileHandle, customActionDataBuffer, strlen(customActionDataBuffer), &numberOfBytesWritten, NULL);
    WriteFile(debugFileHandle, "\r\n", 2, &numberOfBytesWritten, NULL);
    
    CloseHandle(debugFileHandle);
    
    return ERROR_SUCCESS;
}

__declspec(dllexport) UINT __stdcall readPropsFromConfigFile(MSIHANDLE session) {
    //uint32_t e;
    
    //HANDLE debugFileHandle = CreateFileA("D:\\readPropsFromConfigFile.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //DWORD numberOfBytesWritten;
    
    char existingInstallPath[256];
    DWORD existingInstallPathSizeOrRequired = sizeof(existingInstallPath);
    if (MsiGetPropertyA(session, "PROP_EXISTING_INSTALLDIR", existingInstallPath, &existingInstallPathSizeOrRequired) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    
    // @note existingInstallPath (the INSTALLDIR property) has a trailing slash
    char configFilePath[256];
    snprintf(configFilePath, sizeof(configFilePath), "%s%s", existingInstallPath, "opticon-agent.conf");
    
    var *config = var_alloc();
    var_load_json(config, configFilePath); // We don't know if it fails because the config file doesn't exist (yet) or because the parse failed, so we simply ignore all failures
    
    var *collector = var_find_key(config, "collector");
    if (collector == NULL) collector = var_get_dict_forkey(config, "collector");
    
    if (collector->type == VAR_ARRAY) {
        collector = var_first(config);
        if (collector == NULL) collector = var_add_dict(collector);
    }
    
    const char *tenantId = var_get_str_forkey(collector, "tenant");
    if (tenantId != NULL) {
        if (MsiSetPropertyA(session, "PROP_TENANT_ID", tenantId) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    }
    const char *tenantKey = var_get_str_forkey(collector, "key");
    if (tenantKey != NULL) {
        if (MsiSetPropertyA(session, "PROP_TENANT_KEY", tenantKey) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    }
    const char *address = var_get_str_forkey(collector, "key");
    if (address != NULL) {
        if (MsiSetPropertyA(session, "PROP_ADDRESS", address) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    }
    
    uint64_t port = var_get_int_forkey(collector, "port");
    if (port != 0) {
        char portString[256];
        snprintf(portString, sizeof(portString), "%" PRIu64, port);
        if (MsiSetPropertyA(session, "PROP_PORT", portString) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    }
    
    var *updateDict = var_get_dict_forkey(config, "update");
    const char *updateUrl = var_get_str_forkey(updateDict, "url");
    if (updateUrl != NULL) {
        if (MsiSetPropertyA(session, "PROP_UPDATE_URL", updateUrl) != ERROR_SUCCESS) return ERROR_INSTALL_FAILURE;
    }
    
    var_free(config);
    
    //CloseHandle(debugFileHandle);
    
    return ERROR_SUCCESS;
}

__declspec(dllexport) UINT __stdcall moveConfigFileFromExistingInstall(MSIHANDLE session) {
    uint32_t e;
    
    // @todo maybe write an error log file next to the msi?
    //HANDLE debugFileHandle = CreateFileA("D:\\moveConfigFileFromExistingInstall.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //DWORD numberOfBytesWritten;
    
    char *installPath = NULL;
    char *existingInstallPath = NULL;
    DKeyVarMappingEntry keyVarMapping[] = {
        {"installPath", &installPath},
        {"existingInstallPath", &existingInstallPath},
        {NULL, NULL}
    };
    char customActionDataBuffer[1024];
    if ((e = parseCustomActionData(session, customActionDataBuffer, sizeof(customActionDataBuffer), keyVarMapping))) return ERROR_INSTALL_FAILURE;
    
    if (installPath == NULL) return ERROR_INSTALL_FAILURE;
    if (existingInstallPath == NULL) return ERROR_INSTALL_FAILURE;
    
    // @note installPath (the INSTALLDIR property) has a trailing slash
    char configFilePath[256];
    snprintf(configFilePath, sizeof(configFilePath), "%s%s", installPath, "opticon-agent.conf");
    
    //if (existingInstallPath[0] != '\0' && strcmp(existingInstallPath, installPath) != 0) {
        // @note existingInstallPath (the INSTALLDIR property) has a trailing slash
        char existingConfigFilePath[256];
        snprintf(existingConfigFilePath, sizeof(existingConfigFilePath), "%s%s", existingInstallPath, "opticon-agent.conf");
        MoveFileA(existingConfigFilePath, configFilePath);
        // @todo check for error?
    //}
    
    //CloseHandle(debugFileHandle);
    
    return ERROR_SUCCESS;
}

__declspec(dllexport) UINT __stdcall writePropsToConfigFile(MSIHANDLE session) {
    uint32_t e;
    
    //HANDLE debugFileHandle = CreateFileA("D:\\writePropsToConfigFile.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //DWORD numberOfBytesWritten;
    
    
    char *installPath = NULL;
    char *tenantId = NULL;
    char *tenantKey = NULL;
    char *address = NULL;
    char *port = NULL;
    char *updateUrl = NULL;
    DKeyVarMappingEntry keyVarMapping[] = {
        {"installPath", &installPath},
        {"tenantId", &tenantId},
        {"tenantKey", &tenantKey},
        {"address", &address},
        {"port", &port},
        {"updateUrl", &updateUrl},
        {NULL, NULL}
    };
    char customActionDataBuffer[1024];
    if ((e = parseCustomActionData(session, customActionDataBuffer, sizeof(customActionDataBuffer), keyVarMapping))) return ERROR_INSTALL_FAILURE;
    
    //char x[256];
    //snprintf(x, sizeof(x), "installPath:%s;tenantId:%s;tenantKey:%s", installPath, tenantId, tenantKey);
    //snprintf(x, sizeof(x), "tenantId:%s;tenantKey:%s", tenantId, tenantKey);
    //WriteFile(debugFileHandle, x, strlen(x), &numberOfBytesWritten, NULL);
    //WriteFile(debugFileHandle, "\r\n", 2, &numberOfBytesWritten, NULL);
    
    if (installPath == NULL) return ERROR_INSTALL_FAILURE;
    if (tenantId == NULL) return ERROR_INSTALL_FAILURE;
    if (tenantKey == NULL) return ERROR_INSTALL_FAILURE;
    if (address == NULL) return ERROR_INSTALL_FAILURE;
    if (port == NULL) return ERROR_INSTALL_FAILURE;
    if (updateUrl == NULL) return ERROR_INSTALL_FAILURE;
    
    // @note installPath (the INSTALLDIR property) has a trailing slash
    char configFilePath[256];
    snprintf(configFilePath, sizeof(configFilePath), "%s%s", installPath, "opticon-agent.conf");
    
    //HANDLE configFileHandle = CreateFileA(configFilePath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //if (configFileHandle == INVALID_HANDLE_VALUE) return ERROR_INSTALL_FAILURE;
    //if (GetLastError() == ERROR_ALREADY_EXISTS) {
    //}
    
    var *config = var_alloc();
    var_load_json(config, configFilePath); // We don't know if it fails because the config file doesn't exist (yet) or because the parse failed, so we simply ignore all failures
    
    var *collector = var_find_key(config, "collector");
    if (collector == NULL) collector = var_get_dict_forkey(config, "collector");
    
    if (collector->type == VAR_ARRAY) {
        collector = var_first(config);
        if (collector == NULL) collector = var_add_dict(collector);
    }
    
    // @todo
    //if (collector->type != VAR_DICT)
    if (var_find_key(collector, "address") == NULL) var_set_str_forkey(collector, "address", address);
    if (var_find_key(collector, "port") == NULL) var_set_int_forkey(collector, "port", strtoull(port, NULL, 10));
    var_set_str_forkey(collector, "tenant", tenantId);
    var_set_str_forkey(collector, "key", tenantKey);
    
    var *updateDict = var_get_dict_forkey(config, "update");
    if (var_find_key(updateDict, "url") == NULL) var_set_str_forkey(updateDict, "url", updateUrl);
    
    FILE *configFileHandle = fopen(configFilePath, "wb");
    char *comment;
    comment = "# This file is written by the Opticon Agent MSI installer\r\n";
    fwrite(comment, 1, strlen(comment), configFileHandle);
    comment = "# When edited using the MSI, any comments other than this one will be removed\r\n";
    fwrite(comment, 1, strlen(comment), configFileHandle);
    
    // @note var_write_indented writes proper json, not relaxed conf format, so we can just use var_dump instead
    var_dump(config, configFileHandle);
    fclose(configFileHandle);
    
    var_free(config);
    
    //CloseHandle(debugFileHandle);
    
    return ERROR_SUCCESS;
}

__declspec(dllexport) UINT __stdcall deleteGeneratedFiles(MSIHANDLE session) {
    uint32_t e;
    
    //HANDLE debugFileHandle = CreateFileA("D:\\deleteGeneratedFiles.txt", FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    //DWORD numberOfBytesWritten;
    
    
    char *installPath = NULL;
    DKeyVarMappingEntry keyVarMapping[] = {
        {"installPath", &installPath},
        {NULL, NULL}
    };
    char customActionDataBuffer[1024];
    if ((e = parseCustomActionData(session, customActionDataBuffer, sizeof(customActionDataBuffer), keyVarMapping))) return ERROR_INSTALL_FAILURE;
    
    if (installPath == NULL) return ERROR_INSTALL_FAILURE;
    
    // @note installPath (the INSTALLDIR property) has a trailing slash
    char filePath[256];
    
    snprintf(filePath, sizeof(filePath), "%s%s", installPath, "opticon-agent.conf");
    DeleteFileA(filePath);
    
    snprintf(filePath, sizeof(filePath), "%s%s", installPath, "opticon-agent.log");
    DeleteFileA(filePath);
    
    //CloseHandle(debugFileHandle);
    
    return ERROR_SUCCESS;
}

