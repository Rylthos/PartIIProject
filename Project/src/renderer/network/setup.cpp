#include "setup.hpp"

#include "callbacks.hpp"

#include "node.hpp"
#include "string.h"
#include <iostream>
#include <msquic.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logger/logger.hpp"

const static QUIC_BUFFER c_ALPN = {
    .Length = sizeof("sample") - 1,
    .Buffer = (uint8_t*)"sample",
};

namespace Network {

void cleanupQuic()
{
    if (s_QuicConfiguration) {
        s_QuicAPI->ConfigurationClose(s_QuicConfiguration);
    }

    if (s_QuicRegistration) {
        s_QuicAPI->RegistrationClose(s_QuicRegistration);
    }

    if (s_QuicAPI) {
        MsQuicClose(s_QuicAPI);
        s_QuicAPI = nullptr;
    }
}

void initQuic()
{
    const QUIC_REGISTRATION_CONFIG regConfig = {
        .AppName = "Raymarcher",
        .ExecutionProfile = QUIC_EXECUTION_PROFILE_LOW_LATENCY,
    };

    QUIC_STATUS status = QUIC_STATUS_SUCCESS;

    if (QUIC_FAILED(status = MsQuicOpen2(&s_QuicAPI))) {
        LOG_ERROR("MsQuicOpen2 failed, 0x{:x}\n", status);
        exit(-1);
    }

    if (QUIC_FAILED(status = s_QuicAPI->RegistrationOpen(&regConfig, &s_QuicRegistration))) {
        LOG_ERROR("Failed to register: 0x{:x}", status);
        cleanupQuic();
        exit(-1);
    }
}

bool loadServerConfiguration()
{
    QUIC_SETTINGS settings {};
    settings.IdleTimeoutMs = 0;
    settings.IsSet.IdleTimeoutMs = TRUE;

    settings.ServerResumptionLevel = QUIC_SERVER_RESUME_AND_ZERORTT;
    settings.IsSet.ServerResumptionLevel = TRUE;

    settings.PeerBidiStreamCount = 2;
    settings.IsSet.PeerBidiStreamCount = TRUE;

    settings.PeerUnidiStreamCount = 1;
    settings.IsSet.PeerUnidiStreamCount = TRUE;

    QUIC_CREDENTIAL_CONFIG config;
    memset(&config, 0, sizeof(config));
    config.Flags = QUIC_CREDENTIAL_FLAG_NONE;

    QUIC_CERTIFICATE_FILE certFile;
    certFile.CertificateFile = "res/server.cert";
    certFile.PrivateKeyFile = "res/server.key";
    config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE;
    config.CertificateFile = &certFile;

    QUIC_STATUS status = QUIC_STATUS_SUCCESS;
    if (QUIC_FAILED(status = s_QuicAPI->ConfigurationOpen(s_QuicRegistration, &c_ALPN, 1, &settings,
                        sizeof(settings), NULL, &s_QuicConfiguration))) {
        LOG_CRITICAL("Configuration open failed {:x}", status);
        return false;
    }

    if (QUIC_FAILED(
            status = s_QuicAPI->ConfigurationLoadCredential(s_QuicConfiguration, &config))) {
        LOG_CRITICAL("Configuration load credential {:x}", status);
        return false;
    }

    return true;
}

bool loadClientConfiguration()
{
    QUIC_SETTINGS settings {};
    settings.IdleTimeoutMs = 0;
    settings.IsSet.IdleTimeoutMs = TRUE;

    settings.PeerBidiStreamCount = 2;
    settings.IsSet.PeerBidiStreamCount = TRUE;

    settings.PeerUnidiStreamCount = 1;
    settings.IsSet.PeerUnidiStreamCount = TRUE;

    QUIC_CREDENTIAL_CONFIG credConfig;
    memset(&credConfig, 0, sizeof(credConfig));
    credConfig.Type = QUIC_CREDENTIAL_TYPE_NONE;
    credConfig.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;

#ifdef DEBUG
    credConfig.Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
#endif

    QUIC_STATUS status = QUIC_STATUS_SUCCESS;
    if (QUIC_FAILED(status = s_QuicAPI->ConfigurationOpen(s_QuicRegistration, &c_ALPN, 1, &settings,
                        sizeof(settings), NULL, &s_QuicConfiguration))) {
        LOG_CRITICAL("Configuration open failed: {:x}", status);

        return false;
    }

    if (QUIC_FAILED(
            status = s_QuicAPI->ConfigurationLoadCredential(s_QuicConfiguration, &credConfig))) {
        LOG_CRITICAL("Configuration load credential failed: {:x}", status);
        return false;
    }

    return true;
}

void initServer(uint16_t port, bool waitForClient)
{
    initQuic();

    QUIC_STATUS status;

    QUIC_ADDR address = {};
    QuicAddrSetFamily(&address, QUIC_ADDRESS_FAMILY_UNSPEC);
    QuicAddrSetPort(&address, port);

    if (!loadServerConfiguration()) {
        cleanup();
        LOG_CRITICAL("Failed to load config");
        exit(-1);
    }

    if (QUIC_FAILED(status = s_QuicAPI->ListenerOpen(
                        s_QuicRegistration, listenerCallback, NULL, &s_Node.listener))) {
        LOG_ERROR("Failed to open listener {:x}\n", status);
        cleanup();
        exit(-1);
    }

    if (QUIC_FAILED(status = s_QuicAPI->ListenerStart(s_Node.listener, &c_ALPN, 1, &address))) {
        LOG_CRITICAL("ListenerStart failed, 0x{:x}!", status);
        cleanup();
        exit(-1);
    }
}

void initClient(const char* target, uint16_t port)
{
    initQuic();

    if (!loadClientConfiguration()) {
        cleanup();
        exit(-1);
    }

    QUIC_STATUS status;

    if (QUIC_FAILED(status = s_QuicAPI->ConnectionOpen(
                        s_QuicRegistration, connectionCallback, NULL, &s_Node.connection))) {
        LOG_ERROR("Connection open failed: {:x}", status);
        cleanup();
        exit(-1);
    }

    if (QUIC_FAILED(status = s_QuicAPI->ConnectionStart(s_Node.connection, s_QuicConfiguration,
                        QUIC_ADDRESS_FAMILY_UNSPEC, target, port))) {
        LOG_CRITICAL("Failed to start connection: {:x}", status);
        cleanup();
        exit(-1);
    }
}

void cleanup()
{
    if (s_Node.listener != nullptr) {
        s_QuicAPI->ListenerClose(s_Node.listener);
    }

    if (s_Node.connection != nullptr) {
        s_QuicAPI->ConnectionClose(s_Node.connection);
    }

    cleanupQuic();
}
}
