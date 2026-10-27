/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Add one entry per preset. Keep IDs unique and stable; order controls the menu. */
{
    .id = "zerofido",
    .name = "ZeroFIDO",
    .vid = 0x0483,
    .pid = 0x5741,
    .aaguid = "b51a976a-0b02-40aa-9d8a-36c8b91bbd1a",
    .manufacturer = "Flipper Devices Inc.",
    .product = "U2F Token",
},
{
    .id = "yubikey",
    .name = "YubiKey",
    .vid = 0x1050,
    .pid = 0x0407,
    .aaguid = "2fc0579f-8113-47ea-b116-bb5a8db9202a",
    .manufacturer = "Yubico",
    .product = "YubiKey",
},
