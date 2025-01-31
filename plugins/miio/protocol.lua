local socket = require "socket"
local hash = require "hash"
local json = require "cjson"

local assert = assert
local type = type
local error = error

local protocol = {}
local logger = log.getLogger("miio.protocol")

---
--- Message format
---
--- 0                   1                   2                   3
--- 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
--- +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
--- | Magic number = 0x2131         | Packet Length (incl. header)  |
--- |-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
--- | Unknown                                                       |
--- |-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
--- | Device ID ("did")                                             |
--- |-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
--- | Stamp                                                         |
--- |-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
--- | MD5 checksum                                                  |
--- | ... or Device Token in response to the "Hello" packet         |
--- |-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
--- | optional variable-sized data (encrypted)                      |
--- |...............................................................|
---
---                 Mi Home Binary Protocol header
---        Note that one tick mark represents one bit position.
---
---  Magic number: 16 bits
---      Always 0x2131
---
---  Packet length: 16 bits unsigned int
---      Length in bytes of the whole packet, including the header.
---
---  Unknown: 32 bits
---      This value is always 0,
---      except in the "Hello" packet, when it's 0xFFFFFFFF
---
---  Device ID: 32 bits
---      Unique number. Possibly derived from the MAC address.
---      except in the "Hello" packet, when it's 0xFFFFFFFF
---
---  Stamp: 32 bit unsigned int
---      Unix style epoch time
---
---  MD5 checksum:
---      calculated for the whole packet including the MD5 field itself,
---      which must be initialized with device token.
---
---      In the special case of the response to the "Hello" packet,
---      this field contains the 128-bit device token instead.
---
---  optional variable-sized data:
---      encrypted with AES-128: see below.
---      length = packet_length - 0x20
---
---@class MiioMessage
---
---@field unknown integer
---@field did integer
---@field stamp integer
---@field data string

---
--- miIO Encryption.
---
--- The variable-sized data payload is encrypted with the Advanced Encryption Standard (AES).
---
--- A 128-bit key and Initialization Vector are both derived from the Token as follows:
---   Key = MD5(Token)
---   IV  = MD5(Key + Token)
--- PKCS#7 padding is used prior to encryption.
---
--- The mode of operation is Cipher Block Chaining (CBC).
---
---@class MiioEncryption: MiioEncryptionPriv
local encryption = {}

---Encrypt data.
---@param input string
---@return string output
function encryption:encrypt(input)
    local cipher = self.cipher
    cipher:begin("encrypt", self.key, self.iv)
    return cipher:update(input) .. cipher:finsh()
end

---Decrypt data.
---@param input string
---@return string output
function encryption:decrypt(input)
    local cipher = self.cipher
    cipher:begin("decrypt", self.key, self.iv)
    return cipher:update(input) .. cipher:finsh()
end

---Calculates a MD5 checksum for the given data.
---@param data string
---@return string digest
local function md5(data)
    local m = hash.create("MD5")
    m:update(data)
    return m:digest()
end

---Create an encryption.
---@param token string Device token.
---@return MiioEncryption encryption A new encryption.
---@nodiscard
local function createEncryption(token)
    local cipher = require("cipher").create("AES-128-CBC")
    cipher:setPadding("PKCS7")

    local key = md5(token)
    local iv = md5(key .. token)

    ---@class MiioEncryptionPriv
    local o = {
        cipher = cipher,
        key = key,
        iv = iv,
    }

    setmetatable(o, {
        __index = encryption
    })

    return o
end

---Pack a message to a binary package.
---@param unknown integer Unknown: 32-bit.
---@param did integer Device ID: 32-bit.
---@param stamp integer Stamp: 32 bit unsigned int.
---@param token? string Device token: 128-bit.
---@param data? string Optional variable-sized data.
---@return string package
---@nodiscard
local function pack(unknown, did, stamp, token, data)
    local len = 32
    if data then
        len = len + #data
    end

    local header = string.pack(">I2>I2>I4>I4>I4",
        0x2131, len, unknown, did, stamp)
    local checksum = nil
    if token then
        checksum = md5(header .. token .. (data or ""))
    else
        checksum = string.pack(">I4>I4>I4>I4",
            0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff)
    end
    assert(#checksum == 16)

    return header .. checksum .. (data or "")
end

---Unpack a message from a binary package.
---@param package string A binary package.
---@param token? string Device token: 128-bit.
---@return MiioMessage message
---@nodiscard
local function unpack(package, token)
    if string.unpack(">I2", package, 1) ~= 0x2131 then
        error("Invalid magic number.")
    end

    local len = string.unpack(">I2", package, 3)
    if len < 32 or len ~= #package then
        error("Invalid package length.")
    end

    local data = nil
    if len > 32 then
        data = string.unpack("c" .. len - 32, package, 33)
    end

    if token then
        local checksum = md5(string.unpack("c16", package, 1) .. token .. (data or ""))
        assert(#checksum == 16)
        if checksum ~= string.unpack("c16", package, 17) then
            error("Got checksum error which indicates use of an invalid token.")
        end
    end

    return {
        unknown = string.unpack(">I4", package, 5),
        did = string.unpack(">I4", package, 9),
        stamp = string.unpack(">I4", package, 13),
        data = data
    }
end

---@class ScanResult Scan Result.
---
---@field addr string Device address.
---@field devid integer Device ID: 32-bit.
---@field stamp integer Device time stamp.

---Scan for devices in the local network.
---
---This method is used to discover supported devices by sending
---a handshake message to the broadcast address on port 54321.
---If the target IP address is given, the handshake will be send as an unicast packet.
---@param timeout integer Timeout period (in milliseconds).
---@param addr? string Target Address.
---@return ScanResult[] results A array of scan results.
---@nodiscard
function protocol.scan(timeout, addr)
    assert(timeout > 0, "timeout must be greater then 0")

    local sock <close> = socket.create("UDP", "IPV4")
    sock:settimeout(timeout)

    if not addr then
        sock:enablebroadcast()
    end

    local hello = pack(0xffffffff, 0xffffffff, 0xffffffff)
    for i = 1, 3, 1 do
        assert(sock:sendto(hello, addr or "255.255.255.255", 54321), "Failed to send hello message.")
    end

    local seen = {}
    local results = {}

    while true do
        local success, result, fromAddr, _ = pcall(sock.recvfrom, sock, 1024)
        if success == false then
            if addr == nil and result:find("timeout") then
                return results
            end
            error(result)
        end
        local m = unpack(result)
        if m == nil or m.unknown ~= 0 or m.data then
            error("Got a invalid miIO protocol packet.")
        end
        table.insert(results, {
            addr = fromAddr,
            devid = m.did,
            stamp = m.stamp
        })
        if addr then
            assert(addr == fromAddr)
            return results
        end
        if seen[fromAddr] == false then
            seen[fromAddr] = true
        end
    end
end

---@class MiioPcb: MiioPcbPriv miio protocol control block.
local pcb = {}

---@class MiioError miIO error.
---
---@field code integer Error code.
---@field message string Error message.

---Handshake.
---@param timeout integer Timeout period (in milliseconds).
function pcb:handshake(timeout)
    logger:debug("Handshake ...")
    local results = protocol.scan(timeout, self.addr)
    local result = results[1]
    logger:debug("Handshake done.")
    self.devid = result.devid
    self.stampDiff = os.time() - result.stamp
end

---Start a request.
---@param timeout integer Timeout period (in milliseconds).
---@param method string The request method.
---@param params? table Array of parameters.
---@return any result
function pcb:request(timeout, method, params)
    assert(timeout > 0, "timeout must be greater then 0")
    assert(type(method) == "string")

    if params then
        assert(type(params) == "table")
    end

    if self.stampDiff == nil then
        self:handshake(timeout)
    end

    local sock <close> = socket.create("UDP", "IPV4")
    sock:settimeout(timeout)
    sock:connect(self.addr, 54321)

    local reqid = self.reqid + 1
    if reqid == 9999 then
        reqid = 1
    end
    self.reqid = reqid
    do
        local data = json.encode({
            id = reqid,
            method = method,
            params = params or nil
        })

        sock:send(pack(0, self.devid, os.time() - self.stampDiff,
            self.token, self.encryption:encrypt(data)))

        logger:debug(("%s => %s"):format(data, self.addr))
    end

    local success, result = pcall(sock.recv, sock, 1024)
    if success == false then
        if result:find("timeout") then
            self.stampDiff = nil
        end
        error(result)
    end
    self.errCnt = 0
    local msg = unpack(result, self.token)

    if msg == nil or msg.did ~= self.devid or msg.data == nil then
        error("Receive a invalid message.")
    end
    local s = self.encryption:decrypt(msg.data)
    if not s then
        error("Failed to decrypt the message.")
    end
    logger:debug(("%s => %s"):format(self.addr, s))
    local payload =  json.decode(s)
    if not payload then
        error("Failed to parse the JSON string.")
    end
    if payload.id ~= reqid then
        error("response id ~= request id")
    end
    ---@class MiioError
    local err = payload.error
    if err then
        error(err)
    end

    return payload.result
end

---Create a PCB(protocol control block).
---@param addr string Device address.
---@param token string Device token: 128-bit.
---@return MiioPcb pcb Protocol control block.
---@nodiscard
function protocol.create(addr, token)
    assert(type(addr) == "string")
    assert(type(token) == "string")
    assert(#token == 16)

    ---@class MiioPcbPriv
    local o = {
        addr = addr,
        token = token,
        reqid = 0,
    }

    o.encryption = createEncryption(token)

    setmetatable(o, {
        __index = pcb
    })

    return o
end

return protocol
