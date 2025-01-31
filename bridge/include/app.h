// Copyright (c) 2021-2022 Zebin Wu and homekit-bridge contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of homekit-bridge project authors.

#ifndef BRIDGE_INCLUDE_APP_H_
#define BRIDGE_INCLUDE_APP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <HAP.h>

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

// Default lua entry script name.
#define BRIDGE_LUA_ENTRY_DEFAULT "main"

/**
 * Initialize App.
 *
 * @param platform The pointer to the HomeKit platform structure.
 * @param workdir The path of the working directory.
 * @param entry The name of the entry script.
 */
void app_init(HAPPlatform *platform, const char *dir, const char *entry);

/**
 * De-initialize App.
 */
void app_deinit();

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif  // BRIDGE_INCLUDE_APP_H_
