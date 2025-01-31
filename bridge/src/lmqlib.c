// Copyright (c) 2021-2022 Zebin Wu and homekit-bridge contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of homekit-bridge project authors.

#include <lauxlib.h>
#include <HAPLog.h>
#include <HAPBase.h>
#include "app_int.h"
#include "lc.h"

#define LUA_MQ_OBJ_NAME "MQ*"

static const HAPLogObject lmq_log = {
    .subsystem = APP_BRIDGE_LOG_SUBSYSTEM,
    .category = "lmq",
};

typedef struct {
    size_t first;
    size_t last;
    size_t size;
} lmq_obj;

static size_t lmq_size(lmq_obj *obj) {
    return obj->first > obj->last ? obj->size - obj->first + obj->last : obj->last - obj->first;
}

static int lmq_create(lua_State *L) {
    size_t size = luaL_checkinteger(L, 1);
    luaL_argcheck(L, size > 0, 1, "size out of range");
    lmq_obj *obj = lua_newuserdatauv(L, sizeof(*obj), 1);
    luaL_setmetatable(L, LUA_MQ_OBJ_NAME);
    obj->first = 1;
    obj->last = 1;
    obj->size = size;
    lua_createtable(L, 0, 1);
    lua_setuservalue(L, -2);
    return 1;
}

static int lmq_obj_send(lua_State *L) {
    lmq_obj *obj = luaL_checkudata(L, 1, LUA_MQ_OBJ_NAME);
    int narg = lua_gettop(L) - 1;
    int status, nres;

    lua_getuservalue(L, 1);

    if (lua_getfield(L, -1, "wait") == LUA_TTABLE) {
        lua_pushnil(L);
        lua_setfield(L, -3, "wait");  // que.wait = nil
        int waiting = luaL_len(L, -1);
        for (int i = 1; i <= waiting; i++) {
            HAPAssert(lua_geti(L, -1, i) == LUA_TTHREAD);
            lua_State *co = lua_tothread(L, -1);
            lua_pop(L, 1);
            int max = 1 + narg;
            for (int i = 2; i <= max; i++) {
                lua_pushvalue(L, i);
            }
            lua_xmove(L, co, narg);
            status = lc_resumethread(co, L, narg, &nres);
            if (status != LUA_OK && status != LUA_YIELD) {
                HAPLogError(&lmq_log, "%s: %s", __func__, lua_tostring(L, -1));
            }
        }
    } else {
        if (lmq_size(obj) == obj->size) {
            luaL_error(L, "the message queue is full");
        }
        lua_pop(L, 1);
        lua_insert(L, 2);
        lua_createtable(L, narg, 0);
        lua_insert(L, 3);
        for (int i = narg; i >= 1; i--) {
            lua_seti(L, 3, i);
        }
        lua_seti(L, 2, obj->last);
        obj->last++;
        if (obj->last > obj->size + 1) {
            obj->last = 1;
        }
    }
    return 0;
}

static int lmq_obj_recv(lua_State *L) {
    lmq_obj *obj = luaL_checkudata(L, 1, LUA_MQ_OBJ_NAME);
    if (lua_gettop(L) != 1) {
        luaL_error(L, "invalid arguements");
    }
    lua_getuservalue(L, 1);
    if (obj->last == obj->first) {
        int type = lua_getfield(L, 2, "wait");
        if (type == LUA_TNIL) {
            lua_pop(L, 1);
            lua_createtable(L, 1, 0);
            lua_pushthread(L);
            lua_seti(L, 3, 1);
            lua_setfield(L, 2, "wait");
        } else {
            HAPAssert(type == LUA_TTABLE);
            lua_pushthread(L);
            lua_seti(L, 3, luaL_len(L, 3) + 1);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return lua_yield(L, 0);
    } else {
        lua_geti(L, 2, obj->first);
        lua_pushnil(L);
        lua_seti(L, 2, obj->first);
        obj->first++;
        if (obj->first > obj->size + 1) {
            obj->first = 1;
        }
        int nargs = luaL_len(L, 3);
        for (int i = 1; i <= nargs; i++) {
            lua_geti(L, 3, i);
        }
        return nargs;
    }
}

static int lmq_obj_gc(lua_State *L) {
    return 0;
}

static int lmq_obj_tostring(lua_State *L) {
    return 0;
}

static const luaL_Reg lmq_funcs[] = {
    {"create", lmq_create},
    {NULL, NULL},
};

/*
 * metamethods for message queue object
 */
static const luaL_Reg lmq_obj_metameth[] = {
    {"__index", NULL},  /* place holder */
    {"__gc", lmq_obj_gc},
    {"__tostring", lmq_obj_tostring},
    {NULL, NULL}
};

/*
 * methods for MQ object
 */
static const luaL_Reg lmq_obj_meth[] = {
    {"send", lmq_obj_send},
    {"recv", lmq_obj_recv},
    {NULL, NULL},
};

static void lmq_createmeta(lua_State *L) {
    luaL_newmetatable(L, LUA_MQ_OBJ_NAME);  /* metatable for MQ object */
    luaL_setfuncs(L, lmq_obj_metameth, 0);  /* add metamethods to new metatable */
    luaL_newlibtable(L, lmq_obj_meth);  /* create method table */
    luaL_setfuncs(L, lmq_obj_meth, 0);  /* add MQ object methods to method table */
    lua_setfield(L, -2, "__index");  /* metatable.__index = method table */
    lua_pop(L, 1);  /* pop metatable */
}

LUAMOD_API int luaopen_mq(lua_State *L) {
    luaL_newlib(L, lmq_funcs);
    lmq_createmeta(L);
    return 1;
}
