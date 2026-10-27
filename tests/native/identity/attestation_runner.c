/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../../src/attestation/local.h"
#include "../../../src/zerofido_attestation.h"

static unsigned asset_checks;
static const uint8_t *expected;

bool zf_local_attestation_ensure_assets(const ZfLocalAttestationProfile *profile) {
    ++asset_checks;
    assert(profile->identity_len == ZF_AAGUID_LEN);
    assert(memcmp(profile->identity, expected, ZF_AAGUID_LEN) == 0);
    assert(memcmp(profile->extensions_der + profile->extensions_der_len - ZF_AAGUID_LEN, expected,
                  ZF_AAGUID_LEN) == 0);
    return true;
}

bool zf_local_attestation_get_cert_size(const ZfLocalAttestationProfile *profile, size_t *size) {
    (void)profile;
    *size = 321;
    return true;
}

int main(void) {
    uint8_t yubikey[ZF_AAGUID_LEN] = {
        0x2f, 0xc0, 0x57, 0x9f, 0x81, 0x13, 0x47, 0xea,
        0xb1, 0x16, 0xbb, 0x5a, 0x8d, 0xb9, 0x20, 0x2a,
    };
    assert(strcmp(zf_attestation_get_aaguid_string(), "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a") == 0);
    expected = zf_attestation_get_aaguid();
    assert(zf_attestation_validate_consistency());
    /* GetInfo validation must not skip certificate identity checking later. */
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 1);
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 1);

    zf_attestation_set_aaguid(yubikey, "2fc0579f-8113-47ea-b116-bb5a8db9202a");
    expected = yubikey;
    assert(memcmp(zf_attestation_get_aaguid(), yubikey, sizeof(yubikey)) == 0);
    assert(strcmp(zf_attestation_get_aaguid_string(), "2fc0579f-8113-47ea-b116-bb5a8db9202a") == 0);
    assert(zf_attestation_validate_consistency());
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 2);
    puts("native attestation identity regressions passed");
    return 0;
}
