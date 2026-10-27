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
    uint8_t original[ZF_AAGUID_LEN] = {
        0xb5, 0x1a, 0x97, 0x6a, 0x0b, 0x02, 0x40, 0xaa,
        0x9d, 0x8a, 0x36, 0xc8, 0xb9, 0x1b, 0xbd, 0x1a,
    };
    expected = zf_attestation_get_aaguid();
    assert(zf_attestation_validate_consistency());
    /* GetInfo validation must not skip certificate identity checking later. */
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 1);
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 1);

    zf_attestation_set_aaguid(original, "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a");
    expected = original;
    assert(memcmp(zf_attestation_get_aaguid(), original, sizeof(original)) == 0);
    assert(strcmp(zf_attestation_get_aaguid_string(), "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a") == 0);
    assert(zf_attestation_validate_consistency());
    assert(zf_attestation_ensure_ready());
    assert(asset_checks == 2);
    puts("native attestation identity regressions passed");
    return 0;
}
