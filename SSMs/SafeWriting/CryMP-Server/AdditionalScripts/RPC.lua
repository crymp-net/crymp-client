RPC = RPC or { ctr = 0; cb_storage = {}; spawnCounter = 0; spawnedEntities = {}; }

function lerp(a, b, t)
	if type(a) == "table" and type(b) == "table" then
		if a.x and a.y and b.x and b.y then
			if a.z and b.z then return lerp3(a, b, t) end
			return lerp2(a, b, t)
		end
	end
	t = clamp(t, 0, 1)
	return a + t*(b-a)
end

function _lerp(a, b, t)
	return a + t*(b-a)
end

function lerp2(a, b, t)
	t = clamp(t, 0, 1)
	return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); };
end

function lerp3(a, b, t)
	t = clamp(t, 0, 1)
	return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); z = _lerp(a.z, b.z, t); };
end


json = loadstring([[--
-- json.lua
--
-- Copyright (c) 2019 rxi
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy of
-- this software and associated documentation files (the "Software"), to deal in
-- the Software without restriction, including without limitation the rights to
-- use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
-- of the Software, and to permit persons to whom the Software is furnished to do
-- so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.
--

local json = { _version = "0.1.2" }

-------------------------------------------------------------------------------
-- Encode
-------------------------------------------------------------------------------

local encode

local escape_char_map = {
  [ "\\" ] = "\\\\",
  [ "\"" ] = "\\\"",
  [ "\b" ] = "\\b",
  [ "\f" ] = "\\f",
  [ "\n" ] = "\\n",
  [ "\r" ] = "\\r",
  [ "\t" ] = "\\t",
}

local escape_char_map_inv = { [ "\\/" ] = "/" }
for k, v in pairs(escape_char_map) do
  escape_char_map_inv[v] = k
end


local function escape_char(c)
  return escape_char_map[c] or string.format("\\u%04x", c:byte())
end


local function encode_nil(val)
  return "null"
end


local function encode_table(val, stack)
  local res = {}
  stack = stack or {}

  -- Circular reference?
  if stack[val] then error("circular reference") end

  stack[val] = true

  if rawget(val, 1) ~= nil or next(val) == nil then
    -- Treat as array -- check keys are valid and it is not sparse
    local n = 0
    for k in pairs(val) do
      if type(k) ~= "number" then
        error("invalid table: mixed or invalid key types")
      end
      n = n + 1
    end
    if n ~= #val then
      error("invalid table: sparse array")
    end
    -- Encode
    for i, v in ipairs(val) do
      table.insert(res, encode(v, stack))
    end
    stack[val] = nil
    return "[" .. table.concat(res, ",") .. "]"

  else
    -- Treat as an object
    for k, v in pairs(val) do
      if type(k) ~= "string" then
        error("invalid table: mixed or invalid key types")
      end
      table.insert(res, encode(k, stack) .. ":" .. encode(v, stack))
    end
    stack[val] = nil
    return "{" .. table.concat(res, ",") .. "}"
  end
end


local function encode_string(val)
  return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end


local function encode_number(val)
  -- Check for NaN, -inf and inf
  if val ~= val or val <= -math.huge or val >= math.huge then
    error("unexpected number value '" .. tostring(val) .. "'")
  end
  return string.format("%.14g", val)
end


local type_func_map = {
  [ "nil"     ] = encode_nil,
  [ "table"   ] = encode_table,
  [ "string"  ] = encode_string,
  [ "number"  ] = encode_number,
  [ "boolean" ] = tostring,
}


encode = function(val, stack)
  local t = type(val)
  local f = type_func_map[t]
  if f then
    return f(val, stack)
  end
  error("unexpected type '" .. t .. "'")
end


function json.encode(val)
  return ( encode(val) )
end


-------------------------------------------------------------------------------
-- Decode
-------------------------------------------------------------------------------

local parse

local function create_set(...)
  local res = {}
  for i = 1, select("#", ...) do
    res[ select(i, ...) ] = true
  end
  return res
end

local space_chars   = create_set(" ", "\t", "\r", "\n")
local delim_chars   = create_set(" ", "\t", "\r", "\n", "]", "}", ",")
local escape_chars  = create_set("\\", "/", '"', "b", "f", "n", "r", "t", "u")
local literals      = create_set("true", "false", "null")

local literal_map = {
  [ "true"  ] = true,
  [ "false" ] = false,
  [ "null"  ] = nil,
}


local function next_char(str, idx, set, negate)
  for i = idx, #str do
    if set[str:sub(i, i)] ~= negate then
      return i
    end
  end
  return #str + 1
end


local function decode_error(str, idx, msg)
  local line_count = 1
  local col_count = 1
  for i = 1, idx - 1 do
    col_count = col_count + 1
    if str:sub(i, i) == "\n" then
      line_count = line_count + 1
      col_count = 1
    end
  end
  error( string.format("%s at line %d col %d", msg, line_count, col_count) )
end


local function codepoint_to_utf8(n)
  -- http://scripts.sil.org/cms/scripts/page.php?site_id=nrsi&id=iws-appendixa
  local f = math.floor
  if n <= 0x7f then
    return string.char(n)
  elseif n <= 0x7ff then
    return string.char(f(n / 64) + 192, n % 64 + 128)
  elseif n <= 0xffff then
    return string.char(f(n / 4096) + 224, f(n % 4096 / 64) + 128, n % 64 + 128)
  elseif n <= 0x10ffff then
    return string.char(f(n / 262144) + 240, f(n % 262144 / 4096) + 128,
                       f(n % 4096 / 64) + 128, n % 64 + 128)
  end
  error( string.format("invalid unicode codepoint '%x'", n) )
end


local function parse_unicode_escape(s)
  local n1 = tonumber( s:sub(3, 6),  16 )
  local n2 = tonumber( s:sub(9, 12), 16 )
  -- Surrogate pair?
  if n2 then
    return codepoint_to_utf8((n1 - 0xd800) * 0x400 + (n2 - 0xdc00) + 0x10000)
  else
    return codepoint_to_utf8(n1)
  end
end


local function parse_string(str, i)
  local has_unicode_escape = false
  local has_surrogate_escape = false
  local has_escape = false
  local last
  for j = i + 1, #str do
    local x = str:byte(j)

    if x < 32 then
      decode_error(str, j, "control character in string")
    end

    if last == 92 then -- "\\" (escape char)
      if x == 117 then -- "u" (unicode escape sequence)
        local hex = str:sub(j + 1, j + 5)
        if not hex:find("%x%x%x%x") then
          decode_error(str, j, "invalid unicode escape in string")
        end
        if hex:find("^[dD][89aAbB]") then
          has_surrogate_escape = true
        else
          has_unicode_escape = true
        end
      else
        local c = string.char(x)
        if not escape_chars[c] then
          decode_error(str, j, "invalid escape char '" .. c .. "' in string")
        end
        has_escape = true
      end
      last = nil

    elseif x == 34 then -- '"' (end of string)
      local s = str:sub(i + 1, j - 1)
      if has_surrogate_escape then
        s = s:gsub("\\u[dD][89aAbB]..\\u....", parse_unicode_escape)
      end
      if has_unicode_escape then
        s = s:gsub("\\u....", parse_unicode_escape)
      end
      if has_escape then
        s = s:gsub("\\.", escape_char_map_inv)
      end
      return s, j + 1

    else
      last = x
    end
  end
  decode_error(str, i, "expected closing quote for string")
end


local function parse_number(str, i)
  local x = next_char(str, i, delim_chars)
  local s = str:sub(i, x - 1)
  local n = tonumber(s)
  if not n then
    decode_error(str, i, "invalid number '" .. s .. "'")
  end
  return n, x
end


local function parse_literal(str, i)
  local x = next_char(str, i, delim_chars)
  local word = str:sub(i, x - 1)
  if not literals[word] then
    decode_error(str, i, "invalid literal '" .. word .. "'")
  end
  return literal_map[word], x
end


local function parse_array(str, i)
  local res = {}
  local n = 1
  i = i + 1
  while 1 do
    local x
    i = next_char(str, i, space_chars, true)
    -- Empty / end of array?
    if str:sub(i, i) == "]" then
      i = i + 1
      break
    end
    -- Read token
    x, i = parse(str, i)
    res[n] = x
    n = n + 1
    -- Next token
    i = next_char(str, i, space_chars, true)
    local chr = str:sub(i, i)
    i = i + 1
    if chr == "]" then break end
    if chr ~= "," then decode_error(str, i, "expected ']' or ','") end
  end
  return res, i
end


local function parse_object(str, i)
  local res = {}
  i = i + 1
  while 1 do
    local key, val
    i = next_char(str, i, space_chars, true)
    -- Empty / end of object?
    if str:sub(i, i) == "}" then
      i = i + 1
      break
    end
    -- Read key
    if str:sub(i, i) ~= '"' then
      decode_error(str, i, "expected string for key")
    end
    key, i = parse(str, i)
    -- Read ':' delimiter
    i = next_char(str, i, space_chars, true)
    if str:sub(i, i) ~= ":" then
      decode_error(str, i, "expected ':' after key")
    end
    i = next_char(str, i + 1, space_chars, true)
    -- Read value
    val, i = parse(str, i)
    -- Set
    res[key] = val
    -- Next token
    i = next_char(str, i, space_chars, true)
    local chr = str:sub(i, i)
    i = i + 1
    if chr == "}" then break end
    if chr ~= "," then decode_error(str, i, "expected '}' or ','") end
  end
  return res, i
end


local char_func_map = {
  [ '"' ] = parse_string,
  [ "0" ] = parse_number,
  [ "1" ] = parse_number,
  [ "2" ] = parse_number,
  [ "3" ] = parse_number,
  [ "4" ] = parse_number,
  [ "5" ] = parse_number,
  [ "6" ] = parse_number,
  [ "7" ] = parse_number,
  [ "8" ] = parse_number,
  [ "9" ] = parse_number,
  [ "-" ] = parse_number,
  [ "t" ] = parse_literal,
  [ "f" ] = parse_literal,
  [ "n" ] = parse_literal,
  [ "[" ] = parse_array,
  [ "{" ] = parse_object,
}


parse = function(str, idx)
  local chr = str:sub(idx, idx)
  local f = char_func_map[chr]
  if f then
    return f(str, idx)
  end
  decode_error(str, idx, "unexpected character '" .. chr .. "'")
end


function json.decode(str)
  if type(str) ~= "string" then
    error("expected argument of type string, got " .. type(str))
  end
  local res, idx = parse(str, next_char(str, 1, space_chars, true))
  idx = next_char(str, idx, space_chars, true)
  if idx <= #str then
    decode_error(str, idx, "trailing garbage")
  end
  return res
end


return json
]])()

function RPC:Await(id, cb, timeo)
	timeo = timeo or 5
	local players = { }
	if id[3] then
		local pl = g_gameRules.game:GetPlayers() or {};
		for i, v in pairs(pl) do
			if v.id ~= (id[2] or {}).id then
				players[#players + 1] = v
				if not v.rpcFinished then
					v.rpcFinished = {}
				end
				v.rpcFinished[id[1]] = false
			end
		end
	else
		players = { id[2] }
		if not id[2].rpcFinished then
			id[2].rpcFinished = {}
		end
		id[2].rpcFinished[id[1]] = false
	end
	self.cb_storage[id[1]] = {
		players = players,
		t = _time,
		expire = _time + timeo,
		fn = cb
	}
end

function RPC:Call(a, b, c)
	local payload = {}
	payload.id = self.ctr
	self.ctr = self.ctr + 1
	if type(b) == "string" then
		payload.class = a
		payload.method = b
		payload.params = c or {}
	else
		payload.method = a
		payload.params = b or {}
	end
	g_gameRules.allClients:ClStartWorking(SafeWriting.ChatEntity.id, "@" .. json.encode(payload))
	return { payload.id, nil, true }
end

function RPC:CallOne(player, a, b, c)
	local payload = {}
	local channelId=player.actor:GetChannel();
	payload.id = self.ctr
	self.ctr = self.ctr + 1
	if type(b) == "string" then
		payload.class = a
		payload.method = b
		payload.params = c or {}
	else
		payload.method = a
		payload.params = b or {}
	end
	g_gameRules.onClient:ClStartWorking(channelId, SafeWriting.ChatEntity.id, "@" .. json.encode(payload))
	return { payload.id, player, false }
end

function RPC:StreamEntitiesForPlayer(player)
	self.spawnCounter = self.spawnCounter or 0
	player.rpcSpawnCounter = player.rpcSpawnCounter or 0
	--printf("Player: %d, Server: %d", player.rpcSpawnCounter, self.spawnCounter)
	if player.rpcSpawnCounter < self.spawnCounter then
		local mn = math.min(player.rpcSpawnCounter + 16, self.spawnCounter)
		for i=player.rpcSpawnCounter, mn - 1 do
			local j = i + 1
			if self.spawnedEntities[i] then
				local ent = self.spawnedEntities[i][2]
				local par = self.spawnedEntities[i][1]
				if ent then
					if not (ent.vehicle or ent.weapon or ent.class == "Player") then
						self:CallOne(player, "SpawnEntity", par)
					end
				end
			end
			player.rpcSpawnCounter = i
		end
	end
end

function RPC:SpawnEntity(params)
	self.spawnCounter = self.spawnCounter or 0
	self.spawnedEntities = self.spawnedEntities or {}
	params.name = "RS#"..self.spawnCounter
	local ent = System.SpawnEntity(params)
	self.spawnedEntities[#self.spawnedEntities + 1] = {params, ent}
	self.spawnCounter = #self.spawnedEntities + 1
	local wr = {
		_ent = ent;
		_params = params;
		id = ent.id;
		actor = ent.actor;
		vehicle = ent.vehicle;
		item = ent.item;
		weapon = ent.weapon;
		class = ent.class;
		GetName = function(self)
			return self._ent:GetName()
		end,
		SetPos = function(self, pos, dur)
			dur = dur or 0
			self._params.position = pos;
			self._ent:SetPos(pos)
			if dur == 0 then
				RPC:Call("MoveEntity", { name = self._ent:GetName(); pos = pos; })
			else
				RPC:Call("StartMovement", { name = self._ent:GetName(); pos = pos; duration = dur; handle = "P" .. self._ent:GetName() })
			end
		end,
		GetPos = function()
			return self._ent:GetPos()
		end,
		SetScale = function(self, scale, dur)
			dur = dur or 0
			self._params.scale = scale;
			self._ent:SetScale(scale)
			if dur == 0 then
				RPC:Call("MoveEntity", { name = self._ent:GetName(); scale = scale; })
			else
				RPC:Call("StartMovement", { name = self._ent:GetName(); scale = scale; duration = dur; handle = "S" .. self._ent:GetName() })
			end
		end,
		GetScale = function()
			return self._ent:GetScale()
		end,
		SetAngles = function(self, angles)
			dur = dur or 0
			self._params.orientation = angles;
			self._ent:SetAngles(angles)
			RPC:Call("MoveEntity", { name = self._ent:GetName(); angles = angles });
		end,
		GetAngles = function(self)
			return self._ent:GetAngles()
		end,
		GetBase = function(self)
			return self._ent
		end,
		GetSpawnParams = function(self)
			return self._params;
		end
	};
	return wr;
end

function RPC:CallOther(player, a, b, c)
	local payload = {}
	local channelId=player.actor:GetChannel();
	payload.id = self.ctr
	self.ctr = self.ctr + 1
	if type(b) == "string" then
		payload.class = a
		payload.method = b
		payload.params = c or {}
	else
		payload.method = a
		payload.params = b or {}
	end
	g_gameRules.otherClients:ClStartWorking(channelId, SafeWriting.ChatEntity.id, "@" .. json.encode(payload))
	return { payload.id, player, true }
end

AddChatCommand("rpc", function(self, player, msg, cmd)
  System.LogAlways("Received RPC")
	if cmd then
		local obj = json.decode(cmd)
		if obj~=nil then
      System.LogAlways("1," .. tostring(obj.id))
			if obj.id ~= nil and RPC.cb_storage[obj.id] then
        System.LogAlways("2")
				if not player.rpcFinished then
					player.rpcFinished = {}
				end
				player.rpcFinished[obj.id] = true
				local fn = RPC.cb_storage[obj.id].fn
				pcall(fn, player, obj.params)
			end
		end
	end
end, {TEXT})

AddFunc(function()
	for i,v in pairs(RPC.cb_storage) do
		if v and _time > v.expire then
			local fn = v.fn
			for j, w in pairs(v.players) do
				if not v.rpcFinished then v.rpcFinished = {} end
				if not w.rpcFinished then w.rpcFinished = {} end
				if not w.rpcFinished[i] then
					w.rpcFinished[i] = nil
					pcall(fn, w, nil)
				end
			end
			RPC.cb_storage[i] = nil
		end
	end
end, "OnTimerTick")

AddFunc(function(player)
	if player.hwid~= nil then
		RPC:StreamEntitiesForPlayer(player)
	end
end, "UpdatePlayer")

AddFunc(function()
	RPC.spawnedEntities = {}
	RPC.spawnCounter = 0
end, "PrepareAll")