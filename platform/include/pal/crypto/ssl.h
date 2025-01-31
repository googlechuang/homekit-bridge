// Copyright (c) 2021-2022 Zebin Wu and homekit-bridge contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of homekit-bridge project authors.

#ifndef PLATFORM_INCLUDE_PAL_CRYPTO_SSL_H_
#define PLATFORM_INCLUDE_PAL_CRYPTO_SSL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * SSL method.
 */
typedef enum {
    PAL_SSL_ENDPOINT_CLIENT,
    PAL_SSL_ENDPOINT_SERVER,
} pal_ssl_endpoint;

/**
 * SSL error numbers.
 */
typedef enum {
    PAL_SSL_ERR_OK,
    PAL_SSL_ERR_AGAIN,
    PAL_SSL_ERR_INVALID_STATE,
    PAL_SSL_ERR_UNKNOWN,
} pal_ssl_err;

/**
 * SSL context.
 */
typedef struct pal_ssl_ctx pal_ssl_ctx;

/**
 * Initialize SSL module.
 */
void pal_ssl_init();

/**
 * De-initialize SSL module.
 */
void pal_ssl_deinit();

/**
 * Create a SSL context.
 *
 * @param endpoint SSL endpoint.
 * @param hostname Server host name, only valid when the SSL endpoint is PAL_SSL_ENDPOINT_CLIENT.
 * @return the SSL context on success.
 * @return NULL on failure.
 */
pal_ssl_ctx *pal_ssl_create(pal_ssl_endpoint ep, const char *hostname);

/**
 * Free a SSL context.
 *
 * @param ctx The SSL context to be freed.
 *            If this is NULL, the function has no effect.
 */
void pal_ssl_free(pal_ssl_ctx *ctx);

/**
 * Whether the handshake is finshed.
 *
 * @param ctx SSL context.
 * @return true on success
 * @return false on failure.
 */
bool pal_ssl_finshed(pal_ssl_ctx *ctx);

/**
 * Perform the SSL handshake.
 *
 * @param ctx SSL context.
 * @param in Input data.
 * @param ilen Length of @p in.
 * @param out Output data.
 * @param olen Length of @p out.
 * @return PAL_SSL_ERR_OK on success.
 * @return PAL_SSL_ERR_AGAIN means you need to call this function again,
 *         to get the remaining output data.
 * @return Other error numbers on failure.
 */
pal_ssl_err pal_ssl_handshake(pal_ssl_ctx *ctx, const void *in, size_t ilen, void *out, size_t *olen);

/**
 * Encrypt data to be output. 
 *
 * @param ctx SSL context.
 * @param in Input data.
 * @param ilen Length of @p in.
 * @param out Output data.
 * @param olen Length of @p out.
 * @return PAL_SSL_ERR_OK on success.
 * @return PAL_SSL_ERR_AGAIN means you need to call this function again,
 *         to get the remaining output data.
 * @return Other error numbers on failure.
 */
pal_ssl_err pal_ssl_encrypt(pal_ssl_ctx *ctx, const void *in, size_t ilen, void *out, size_t *olen);

/**
 * Decrypt input data.
 *
 * @param ctx SSL context.
 * @param in Input data.
 * @param ilen Length of @p in.
 * @param out Output data.
 * @param olen Length of @p out.
 * @return PAL_SSL_ERR_OK on success.
 * @return PAL_SSL_ERR_AGAIN means you need to call this function again,
 *         to get the remaining output data.
 * @return Other error numbers on failure.
 */
pal_ssl_err pal_ssl_decrypt(pal_ssl_ctx *ctx, const void *in, size_t ilen, void *out, size_t *olen);

#ifdef __cplusplus
}
#endif

#endif  // PLATFORM_INCLUDE_PAL_CRYPTO_SSL_H_
