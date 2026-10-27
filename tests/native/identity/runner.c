/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../../src/zerofido_identity.h"
#include "../../../src/zerofido_storage.h"

static uint8_t disk[128];
static size_t disk_size;
static bool fail_write;
static unsigned recoveries;

bool zf_storage_ensure_app_data_dir(Storage *storage) {
    return storage != NULL;
}

bool zf_storage_recover_atomic_file(Storage *storage, const char *path, const char *temp_path) {
    (void)storage;
    (void)path;
    (void)temp_path;
    ++recoveries;
    return true;
}

bool zf_storage_read_file(Storage *storage, const char *path, uint8_t *data, size_t capacity,
                          size_t *size) {
    (void)storage;
    (void)path;
    if (!disk_size || disk_size > capacity)
        return false;
    memcpy(data, disk, disk_size);
    *size = disk_size;
    return true;
}

bool zf_storage_write_file_atomic(Storage *storage, const char *path, const char *temp_path,
                                  const uint8_t *data, size_t size) {
    (void)storage;
    (void)path;
    (void)temp_path;
    if (fail_write)
        return false;
    assert(size <= sizeof(disk));
    memcpy(disk, data, size);
    disk_size = size;
    return true;
}

int main(void) {
    Storage *storage = (Storage *)&disk;
    ZfIdentity identity, loaded, before;
    char text[ZF_IDENTITY_TEXT_SIZE];
    zf_identity_load(storage, &identity);
    assert(identity.vid == 0x1050 && identity.pid == 0x0407);
    assert(strcmp(identity.manufacturer, "Yubico") == 0);
    zf_identity_get_text(&identity, ZfIdentityFieldAaguid, text);
    assert(strcmp(text, "2fc0579f-8113-47ea-b116-bb5a8db9202a") == 0);

    assert(zf_identity_set_text(&identity, ZfIdentityFieldVid, "0XABCD"));
    assert(zf_identity_set_text(&identity, ZfIdentityFieldPid, "1234"));
    assert(zf_identity_set_text(&identity, ZfIdentityFieldAaguid,
                                "ABCDEF01-2345-6789-ABCD-EF0123456789"));
    assert(zf_identity_set_text(&identity, ZfIdentityFieldProduct, "My security key"));
    assert(zf_identity_save(storage, &identity));
    zf_identity_load(storage, &loaded);
    assert(memcmp(&identity, &loaded, sizeof(identity)) == 0);
    zf_identity_get_text(&loaded, ZfIdentityFieldAaguid, text);
    assert(strcmp(text, "abcdef01-2345-6789-abcd-ef0123456789") == 0);
    assert(recoveries == 2);

    before = identity;
    const char *bad_ids[] = {"", "123", "12345", "0x", "0000", "ffff", "12g4", " 1234", "1234 "};
    for (size_t i = 0; i < sizeof(bad_ids) / sizeof(bad_ids[0]); ++i)
        assert(!zf_identity_set_text(&identity, ZfIdentityFieldVid, bad_ids[i]));
    const char *bad_uuids[] = {
        "", "abcdef0123456789abcdef0123456789", "abcdef01_2345-6789-abcd-ef0123456789",
        "abcdef01-2345-6789-abcd-ef012345678g", "abcdef01-2345-6789-abcd-ef01234567890"};
    for (size_t i = 0; i < sizeof(bad_uuids) / sizeof(bad_uuids[0]); ++i)
        assert(!zf_identity_set_text(&identity, ZfIdentityFieldAaguid, bad_uuids[i]));
    assert(!zf_identity_set_text(&identity, ZfIdentityFieldProduct, ""));
    assert(!zf_identity_set_text(&identity, ZfIdentityFieldProduct, "bad\nname"));
    assert(!zf_identity_set_text(&identity, ZfIdentityFieldProduct,
                                 "12345678901234567890123456789012"));
    assert(memcmp(&before, &identity, sizeof(identity)) == 0);
    assert(
        zf_identity_set_text(&identity, ZfIdentityFieldProduct, "1234567890123456789012345678901"));
    assert(zf_identity_save(storage, &identity));
    zf_identity_load(storage, &loaded);
    assert(strcmp(loaded.product, identity.product) == 0);

    /* Active descriptors remain unchanged by saving a pending identity. */
    zf_identity_load_active(storage);
    before = *zf_identity_active();
    zf_identity_original(&identity);
    assert(zf_identity_save(storage, &identity));
    assert(memcmp(&before, zf_identity_active(), sizeof(before)) == 0);
    zf_identity_load_active(storage);
    assert(zf_identity_active()->vid == 0x0483 && zf_identity_active()->pid == 0x5741);
    zf_identity_get_text(zf_identity_active(), ZfIdentityFieldAaguid, text);
    assert(strcmp(text, "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a") == 0);
    assert(strcmp(zf_identity_active()->product, "U2F Token") == 0);

    fail_write = true;
    zf_identity_defaults(&identity);
    assert(!zf_identity_save(storage, &identity));
    zf_identity_load(storage, &loaded);
    assert(loaded.vid == 0x0483); /* previous successful save survives */
    fail_write = false;

    /* Reject unknown versions, truncation, oversized files and unterminated names. */
    assert(zf_identity_save(storage, &identity));
    disk[3] = 2;
    zf_identity_load(storage, &loaded);
    assert(loaded.vid == 0x1050);
    zf_identity_original(&identity);
    assert(zf_identity_save(storage, &identity));
    --disk_size;
    zf_identity_load(storage, &loaded);
    assert(loaded.vid == 0x1050);
    assert(zf_identity_save(storage, &identity));
    ++disk_size;
    zf_identity_load(storage, &loaded);
    assert(loaded.vid == 0x1050);
    assert(zf_identity_save(storage, &identity));
    memset(disk + 24, 'X', ZF_IDENTITY_NAME_SIZE);
    zf_identity_load(storage, &loaded);
    assert(loaded.vid == 0x1050);
    memset(identity.product, 'X', sizeof(identity.product));
    assert(!zf_identity_save(storage, &identity));
    assert(!zf_identity_save(NULL, &identity));
    /* Every registered preset must remain valid, unique and persistable as the list grows. */
    assert(zf_identity_preset_count() >= 2U);
    for (size_t i = 0; i < zf_identity_preset_count(); ++i) {
        const char *name = zf_identity_preset_name(i);
        const char *id = zf_identity_preset_id(i);
        assert(name && strlen(name) > 0U && strlen(name) < ZF_IDENTITY_NAME_SIZE);
        assert(id && strlen(id) > 0U);
        for (size_t j = 0; j < i; ++j)
            assert(strcmp(id, zf_identity_preset_id(j)) != 0);
        assert(zf_identity_preset_apply(i, &identity));
        assert(zf_identity_save(storage, &identity));
        zf_identity_load(storage, &loaded);
        assert(memcmp(&identity, &loaded, sizeof(identity)) == 0);
    }
    before = identity;
    assert(zf_identity_preset_name(zf_identity_preset_count()) == NULL);
    assert(zf_identity_preset_id(SIZE_MAX) == NULL);
    assert(!zf_identity_preset_apply(SIZE_MAX, &identity));
    assert(!zf_identity_preset_apply(0, NULL));
    assert(memcmp(&before, &identity, sizeof(identity)) == 0);
    puts("native identity regressions passed");
    return 0;
}
