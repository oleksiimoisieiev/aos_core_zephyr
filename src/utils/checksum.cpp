/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <tinycrypt/constants.h>
#include <tinycrypt/sha256.h>

#include "checksum.hpp"

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

aos::RetWithError<aos::StaticArray<uint8_t, aos::cSHA256Size>> CalculateSha256(const aos::Array<uint8_t>& data)
{
    tc_sha256_state_struct                      s;
    aos::StaticArray<uint8_t, aos::cSHA256Size> digest;

    digest.Resize(aos::cSHA256Size);

    if (data.Size() == 0) {
        return digest;
    }

    auto ret = tc_sha256_init(&s);
    if (TC_CRYPTO_SUCCESS != ret) {
        return {digest, AOS_ERROR_WRAP(ret)};
    }

    ret = tc_sha256_update(&s, data.Get(), data.Size());
    if (TC_CRYPTO_SUCCESS != ret) {
        return {digest, AOS_ERROR_WRAP(ret)};
    }

    ret = tc_sha256_final(static_cast<uint8_t*>(digest.Get()), &s);
    if (TC_CRYPTO_SUCCESS != ret) {
        return {digest, AOS_ERROR_WRAP(ret)};
    }

    return digest;
}
