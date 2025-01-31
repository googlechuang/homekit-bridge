// Copyright (c) 2021-2022 Zebin Wu and homekit-bridge contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of homekit-bridge project authors.

#include <lauxlib.h>
#include <pal/net/dns.h>

#include "app_int.h"
#include "lc.h"

static const char *ldns_family_strs[] = {
    "",
    "IPV4",
    "IPV6",
    NULL
};

static const HAPLogObject ldns_log = {
    .subsystem = APP_BRIDGE_LOG_SUBSYSTEM,
    .category = "ldns",
};

void ldns_response_cb(const char *addr, void *arg) {
    lua_State *L = app_get_lua_main_thread();
    lua_State *co = arg;
    int status, nres;
    int narg = 0;
    if (addr) {
        narg = 1;
        lua_pushstring(co, addr);
    }
    status = lc_resumethread(co, L, narg, &nres);
    if (status != LUA_OK && status != LUA_YIELD) {
        HAPLogError(&ldns_log, "%s: %s", __func__, lua_tostring(L, -1));
    }

    lua_settop(L, 0);
    lc_collectgarbage(L);
}

static int finshresolve(lua_State *L, int status, lua_KContext extra) {
    if (status != 1 || !lua_isstring(L, -1)) {
        luaL_error(L, "failed to resolve");
    }
    return 1;
}

static int ldns_resolve(lua_State *L) {
    const char *hostname = luaL_checkstring(L, 1);
    pal_addr_family af = luaL_checkoption(L, 2, "", ldns_family_strs);

    if (!pal_dns_start_request(hostname, af, ldns_response_cb, L)) {
        luaL_error(L, "failed to start DNS resolution request");
    }
    return lua_yieldk(L, 0, 0, finshresolve);
}

static const luaL_Reg ldns_funcs[] = {
    {"resolve", ldns_resolve},
    {NULL, NULL},
};

LUAMOD_API int luaopen_dns(lua_State *L) {
    luaL_newlib(L, ldns_funcs);
    return 1;
}
