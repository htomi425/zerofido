/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "zerofido_identity.h"

#include <stdio.h>
#include <string.h>

#include "zerofido_storage.h"

#define ZF_IDENTITY_PATH ZF_APP_DATA_DIR "/identity.bin"
#define ZF_IDENTITY_TEMP ZF_APP_DATA_DIR "/identity.tmp"
/* Explicit byte layout: magic/version, VID/PID (big endian), UUID, two names. */
#define ZF_IDENTITY_RECORD_SIZE (8U + ZF_IDENTITY_AAGUID_SIZE + 2U * ZF_IDENTITY_NAME_SIZE)

static ZfIdentity zf_active_identity;
static bool zf_active_identity_ready;

static int zf_identity_hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool zf_identity_name_valid(const char *name) {
    size_t length = 0;
    while (length < ZF_IDENTITY_NAME_SIZE && name[length]) {
        if ((unsigned char)name[length] < 32U || (unsigned char)name[length] > 126U)
            return false;
        ++length;
    }
    return length > 0U && length < ZF_IDENTITY_NAME_SIZE;
}

static bool zf_identity_valid(const ZfIdentity *identity) {
    return identity && identity->vid != 0U && identity->vid != 0xffffU && identity->pid != 0U &&
           identity->pid != 0xffffU && zf_identity_name_valid(identity->manufacturer) &&
           zf_identity_name_valid(identity->product);
}

bool zf_identity_set_text(ZfIdentity *identity, ZfIdentityField field, const char *text) {
    if (!identity || !text)
        return false;
    if (field == ZfIdentityFieldVid || field == ZfIdentityFieldPid) {
        if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            text += 2;
        if (strlen(text) != 4U)
            return false;
        uint16_t value = 0;
        for (size_t i = 0; i < 4U; ++i) {
            int digit = zf_identity_hex(text[i]);
            if (digit < 0)
                return false;
            value = (uint16_t)((value << 4U) | (unsigned)digit);
        }
        if (value == 0U || value == 0xffffU)
            return false;
        if (field == ZfIdentityFieldVid)
            identity->vid = value;
        else
            identity->pid = value;
        return true;
    }
    if (field == ZfIdentityFieldAaguid) {
        if (strlen(text) != 36U)
            return false;
        uint8_t uuid[ZF_IDENTITY_AAGUID_SIZE];
        size_t index = 0;
        for (size_t i = 0; i < 36U;) {
            if (i == 8U || i == 13U || i == 18U || i == 23U) {
                if (text[i++] != '-')
                    return false;
                continue;
            }
            int high = zf_identity_hex(text[i++]);
            int low = zf_identity_hex(text[i++]);
            if (high < 0 || low < 0)
                return false;
            uuid[index++] = (uint8_t)((high << 4) | low);
        }
        memcpy(identity->aaguid, uuid, sizeof(uuid));
        return true;
    }
    if (field == ZfIdentityFieldManufacturer || field == ZfIdentityFieldProduct) {
        if (!zf_identity_name_valid(text))
            return false;
        char *target =
            field == ZfIdentityFieldManufacturer ? identity->manufacturer : identity->product;
        memset(target, 0, ZF_IDENTITY_NAME_SIZE);
        memcpy(target, text, strlen(text));
        return true;
    }
    return false;
}

void zf_identity_get_text(const ZfIdentity *identity, ZfIdentityField field,
                          char out[ZF_IDENTITY_TEXT_SIZE]) {
    out[0] = '\0';
    if (!identity)
        return;
    if (field == ZfIdentityFieldVid || field == ZfIdentityFieldPid) {
        snprintf(out, ZF_IDENTITY_TEXT_SIZE, "%04x",
                 (unsigned)(field == ZfIdentityFieldVid ? identity->vid : identity->pid));
    } else if (field == ZfIdentityFieldAaguid) {
        static const char hex[] = "0123456789abcdef";
        size_t position = 0;
        for (size_t i = 0; i < ZF_IDENTITY_AAGUID_SIZE; ++i) {
            if (i == 4U || i == 6U || i == 8U || i == 10U)
                out[position++] = '-';
            out[position++] = hex[identity->aaguid[i] >> 4U];
            out[position++] = hex[identity->aaguid[i] & 15U];
        }
        out[position] = '\0';
    } else if (field == ZfIdentityFieldManufacturer || field == ZfIdentityFieldProduct) {
        snprintf(out, ZF_IDENTITY_TEXT_SIZE, "%s",
                 field == ZfIdentityFieldManufacturer ? identity->manufacturer : identity->product);
    }
}

void zf_identity_defaults(ZfIdentity *identity) {
    memset(identity, 0, sizeof(*identity));
    identity->vid = 0x1050;
    identity->pid = 0x0407;
    zf_identity_set_text(identity, ZfIdentityFieldAaguid, "2fc0579f-8113-47ea-b116-bb5a8db9202a");
    strcpy(identity->manufacturer, "Yubico");
    strcpy(identity->product, "YubiKey");
}

void zf_identity_original(ZfIdentity *identity) {
    zf_identity_defaults(identity);
    identity->vid = 0x0483;
    identity->pid = 0x5741;
    zf_identity_set_text(identity, ZfIdentityFieldAaguid, "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a");
    strcpy(identity->manufacturer, "Flipper Devices Inc.");
    strcpy(identity->product, "U2F Token");
}

void zf_identity_load(Storage *storage, ZfIdentity *identity) {
    uint8_t record[ZF_IDENTITY_RECORD_SIZE + 1U] = {0};
    size_t size = 0;
    zf_identity_defaults(identity);
    if (!storage)
        return;
    zf_storage_recover_atomic_file(storage, ZF_IDENTITY_PATH, ZF_IDENTITY_TEMP);
    if (!zf_storage_read_file(storage, ZF_IDENTITY_PATH, record, sizeof(record), &size) ||
        size != ZF_IDENTITY_RECORD_SIZE || memcmp(record, "ZFI\1", 4U) != 0)
        return;
    ZfIdentity candidate = {0};
    candidate.vid = (uint16_t)(((unsigned)record[4] << 8U) | record[5]);
    candidate.pid = (uint16_t)(((unsigned)record[6] << 8U) | record[7]);
    memcpy(candidate.aaguid, record + 8U, sizeof(candidate.aaguid));
    memcpy(candidate.manufacturer, record + 24U, ZF_IDENTITY_NAME_SIZE);
    memcpy(candidate.product, record + 24U + ZF_IDENTITY_NAME_SIZE, ZF_IDENTITY_NAME_SIZE);
    if (zf_identity_valid(&candidate))
        *identity = candidate;
}

bool zf_identity_save(Storage *storage, const ZfIdentity *identity) {
    uint8_t record[ZF_IDENTITY_RECORD_SIZE] = {'Z', 'F', 'I', 1};
    if (!storage || !zf_identity_valid(identity) || !zf_storage_ensure_app_data_dir(storage))
        return false;
    record[4] = (uint8_t)(identity->vid >> 8U);
    record[5] = (uint8_t)identity->vid;
    record[6] = (uint8_t)(identity->pid >> 8U);
    record[7] = (uint8_t)identity->pid;
    memcpy(record + 8U, identity->aaguid, sizeof(identity->aaguid));
    memcpy(record + 24U, identity->manufacturer, strlen(identity->manufacturer));
    memcpy(record + 24U + ZF_IDENTITY_NAME_SIZE, identity->product, strlen(identity->product));
    return zf_storage_write_file_atomic(storage, ZF_IDENTITY_PATH, ZF_IDENTITY_TEMP, record,
                                        sizeof(record));
}

void zf_identity_load_active(Storage *storage) {
    zf_identity_load(storage, &zf_active_identity);
    zf_active_identity_ready = true;
}

const ZfIdentity *zf_identity_active(void) {
    if (!zf_active_identity_ready)
        zf_identity_load_active(NULL);
    return &zf_active_identity;
}
