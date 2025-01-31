local util = require "util"
local hap = require "hap"
local traceback = debug.traceback

local plugins = {}

local logger = log.getLogger("plugins")

---@class PluginConf:table Plugin configuration.

---@class Plugin:table Plugin.
---
---@field init fun(conf: PluginConf): HapAccessory[] Initialize plugin and generate accessories in initialization.
---@field handleState fun(state: HapServerState) Handle HAP server state.

local priv = {
    plugins = {},   ---@type table<string, Plugin>
}

---Load plugin.
---@param name string Plugin name.
local function loadPlugin(name, conf)
    local plugin = priv.plugins[name]
    if plugin then
        error("Plugin is already loaded.")
    end

    plugin = require(name .. ".plugin")
    if util.isEmptyTable(plugin) then
        error(("No fields in plugin '%s'."):format(name))
    end
    local fields = {
        init = "function",
        handleState = "function"
    }
    for k, t in pairs(fields) do
        if not plugin[k] then
            error(("No field '%s' in plugin '%s'."):format(k, name))
        end
        local _t = type(plugin[k])
        if _t ~= t then
            error(("%s.%s: type error, expected %s, got %s."):format(name, k, t, _t))
        end
    end
    plugin.init(conf)
    priv.plugins[name] = plugin
end

---Load plugins and generate bridged accessories.
---@param pluginConfs table<string, PluginConf> Plugin configurations.
function plugins.init(pluginConfs)
    if pluginConfs then
        for name, conf in pairs(pluginConfs) do
            local success, result = xpcall(loadPlugin, traceback, name, conf)
            if success == false then
                logger:error(result)
            end
        end
    end
end

---Handle HAP server state.
---@param state HapServerState
function plugins.handleState(state)
    for _, plugin in pairs(priv.plugins) do
        plugin.handleState(state)
    end
    if state == "Running" then
        local loaded = package.loaded
        for name, _ in pairs(loaded) do
            loaded[name] = nil
        end
    end
end

return plugins
