/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define ZF_IDENTITY_NAME_SIZE 32U
#define ZF_IDENTITY_AAGUID_SIZE 16U
#define ZF_IDENTITY_TEXT_SIZE 37U

typedef enum {
    ZfIdentityFieldVid = 1,
    ZfIdentityFieldPid,
    ZfIdentityFieldAaguid,
    ZfIdentityFieldManufacturer,
    ZfIdentityFieldProduct,
} ZfIdentityField;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    uint8_t aaguid[ZF_IDENTITY_AAGUID_SIZE];
    char manufacturer[ZF_IDENTITY_NAME_SIZE];
    char product[ZF_IDENTITY_NAME_SIZE];
} ZfIdentity;

/* Historical identity claim, captured once for a newly registered credential. */
typedef struct {
    bool known;
    char preset[ZF_IDENTITY_NAME_SIZE];
    uint16_t vid;
    uint16_t pid;
    uint8_t aaguid[ZF_IDENTITY_AAGUID_SIZE];
} ZfRegistrationIdentity;

void zf_identity_snapshot_active(ZfRegistrationIdentity *snapshot);

/* Menu order comes from identity_presets.inc. Invalid indices leave output unchanged. */
size_t zf_identity_preset_count(void);
const char *zf_identity_preset_name(size_t index);
const char *zf_identity_preset_id(size_t index);
bool zf_identity_preset_apply(size_t index, ZfIdentity *identity);
void zf_identity_defaults(ZfIdentity *identity);
void zf_identity_original(ZfIdentity *identity);
void zf_identity_load(Storage *storage, ZfIdentity *identity);
bool zf_identity_save(Storage *storage, const ZfIdentity *identity);
void zf_identity_load_active(Storage *storage);
const ZfIdentity *zf_identity_active(void);
bool zf_identity_set_text(ZfIdentity *identity, ZfIdentityField field, const char *text);
void zf_identity_get_text(const ZfIdentity *identity, ZfIdentityField field,
                          char out[ZF_IDENTITY_TEXT_SIZE]);
