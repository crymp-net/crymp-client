System.LogAlways("$5[CryMP] Client loaded")

ALLOW_EXPERIMENTAL = true

function InitializeClient()
	local masters = nil
	local _L = {}
	local logAlways = System.LogAlways
	local EXPORT = false
	local EXPORTED = {}
	local RED, GREEN, YELLOW = "$4", "$3", "$6"
	local DEBUG_MODE = false
	local ACTIVE_RPC = nil
	local RPC_STATE  = true
	local MASK_FROZEN = 1
	local MASK_WET = 2
	local MASK_CLOAK = 4
	local MASK_DYNFROZEN = 8
	local DEFAULT_MASTER = "crymp.org"

	local pendingMasterResolves = {}
	local activeProfile = {
		static = {
			logged = false,
			name = nil,
			password = nil,
			id = nil,
			token = nil,
			display = nil,
			master = DEFAULT_MASTER
		},
		real = {
			logged = false,
			name = nil,
			password = nil,
			id = nil,
			token = nil,
			display = nil,
			master = DEFAULT_MASTER
		}
	};

	local localState = {};
	local addCCommand = System.AddCCommand;
	local getCVar = System.GetCVar;

	local function OnMasterResolved()
		masters = _L.CPPAPI.GetMasters()
		printf("$5[CryMP] Resolved masters:")
		for i, master in pairs(masters) do
			printf("$5[CryMP] - %s", master)
		end
		for i, resolve in pairs(pendingMasterResolves) do
			-- first master gets to be the default master
			resolve(masters[1])
		end
		pendingMasterResolves = {}
	end

	local function _pcall(func, ...)
		local status,err=pcall(func,...);
		if not status then
			_L.System.Log("$6Lua Error: " .. err);
		end
		return err;
	end

	local function lerp(a, b, t)
		if type(a) == "table" and type(b) == "table" then
			if a.x and a.y and b.x and b.y then
				if a.z and b.z then return lerp3(a, b, t) end
				return lerp2(a, b, t)
			end
		end
		t = clamp(t, 0, 1)
		return a + t*(b-a)
	end

	local function _lerp(a, b, t)
		return a + t*(b-a)
	end

	local function lerp2(a, b, t)
		t = clamp(t, 0, 1)
		return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); };
	end

	local function lerp3(a, b, t)
		t = clamp(t, 0, 1)
		return { x = _lerp(a.x, b.x, t); y = _lerp(a.y, b.y, t); z = _lerp(a.z, b.z, t); };
	end

	local function clamp(a, b, t)
		if a < b then return b end
		if a > t then return t end
		return a
	end

	local function printf(fmt, ...)
		if (#{...}) == 0 then return logAlways(fmt) end
		logAlways(fmt:format(...))
	end

	local function SafeAddCCommand(name, ...)
		if name == "login" or name == "simple_login" or name == "secu_login" or name == "auth_login" then
			System.LogAlways("$4[error] Cannot override these!")
			return nil
		end
		return addCCommand(name, ...)
	end

	local function FixFlaws()
		System.AddCCommand = SafeAddCCommand
	end

	local function RemoveFlaws()
		for i, v in pairs(io) do
			io[i] = nil
		end
		for i, v in pairs(os) do
			if not (i == "clock" or i == "time") then
				os[i] = nil
			end
		end
		System.ScanDirectory = nil
		System.BrowseURL = nil
		FixFlaws()
	end

	local function RestoreEnvironment()
		for i, v in pairs(EXPORTED) do
			_G[i] = v
		end
		for i, v in pairs(_L) do
			if type(_G[i]) ~= "table" then
				_L.System.Log(RED .. "[CryMP] Server changed global " .. YELLOW .. i .. RED.. ", reverting back!")
				_G[i] = {}
				v = _G[i]
			end
			for j, w in pairs(v) do
				if _G[i][j] ~= w then
					_L.System.Log(RED .. "[CryMP] Server changed global function " .. YELLOW .. i .. "." .. j .. RED .. ", reverting back!")
				end
				_G[i][j] = w
			end
		end
		RemoveFlaws()
	end

	local function ResetState()
		RestoreEnvironment()
		localState = {
			ACTIVE_LAYERS = 0,
			ACTIVE_ANIMATIONS = {},
			ACTIVE_EFFECTS = {},
			STATIC_ID = activeProfile.static.id or "Unknown",
			STATIC_HASH = activeProfile.static.token or "Unknown",
			KEY_BINDINGS = {},
			SPAWNED_FOG_VOLUMES = {},
			SPAWNED_LIGHT_ENTITIES = {},
			HOOKS = {
				Counter = 0,
				OnUpdate = {},
				OnSpawn = {}
			}
		}
	end

	ResetState()

	local function CallHooks(name, ...)
		for i, v in pairs(localState.HOOKS[name]) do
			if type(v) == "function" then
				local ok, err = pcall(v, dt)
				if not ok then
					_L.System.LogAlways("$4 [hook] Error during "..name.." hook (id=" .. tostring(i) .. "): " .. tostring(err))
				end
			end
		end
	end

	local function UpdateWorld(localState, dt)
		for i,params in pairs(localState.ACTIVE_ANIMATIONS) do
			if params then
				local ent = params.entity;
				if not ent then params.entity = _L.System.GetEntityByName(params.name); ent = params.entity; end
				if ent then
					local dur = _time - params.start;

					if params.pos then
						ent:SetWorldPos(lerp(params.pos.from, params.pos.to, dur / params.duration))
					end

					if params.scale then
						ent:SetScale(lerp(params.scale.from, params.scale.to, dur / params.duration))
					end

					if dur >= params.duration then
						localState.ACTIVE_ANIMATIONS[i] = nil
					end
				end
			end
		end

		for i,v in pairs(localState.ACTIVE_EFFECTS) do
			if v~=nil then
				_L.System.SetScreenFx(i, v)
			end
		end

		CallHooks("OnUpdate", dt)
	end

	local function PerformKeyBind(key)
		if localState.KEY_BINDINGS[key] then
			g_gameRules.game:SendChatMessage(ChatToTarget, g_localActor.id, g_localActor.id, localState.KEY_BINDINGS[key]);
		end
	end

	local function CreateBinding(key, action)
		if not localState.KEY_BINDINGS[key] then
			_L.CPPAPI.AddCCommand("bind_action_"..key, function()
				PerformKeyBind(key)
			end)
			_L.System.ExecuteCommand("bind " .. key .. " bind_action_"..key)
		end
		localState.KEY_BINDINGS[key] = action
	end

	local function AddHook(hookType, name, fn)
		localState.HOOKS.Counter = localState.HOOKS.Counter + 1
		if type(name) == "function" then
			fn = name
			name = tostring(localState.HOOKS.Counter)
		end
		localState.HOOKS[hookType] = localState.HOOKS[hookType] or {}
		localState.HOOKS[hookType][name] = fn
		return name
	end

	local function RemoveHook(hookType, name)
		localState.HOOKS[hookType] = localState.HOOKS[hookType] or {}
		localState.HOOKS[hookType][name] = nil
	end

	local function GetProfile()
		if activeProfile.real.logged then
			return activeProfile.real
		elseif activeProfile.static.logged then
			return activeProfile.static
		else
			return nil
		end
	end

	local function Promise(callback)
		local Ancestor = {
			_PrematureResolveFlag = false,
			_PrematureRejectFlag = false,
			_PrematureResolve = nil,
			_PrematureReject = nil,
			_Handler = nil,
			_Catch = nil,
			_Promise = false,
			Then = function(self, handler)
				self._Handler = handler
				if self._PrematureResolveFlag then
					self:CallThen(self._PrematureResolve)
				end
				return self
			end,
			Catch = function(self, handler)
				self._Catch = handler
				if self._PrematureRejectFlag then
					self:CallCatch(self._PrematureReject)
				end
				return self
			end,
			CallThen = function(self, response)
				if self._Handler ~= nil then
					self._Handler(response)
					self._Handler = nil
				else
					self._PrematureResolveFlag = true
					self._PrematureResolve = response
				end
			end,
			CallCatch = function(self, response)
				if self._Catch ~= nil then
					self._Catch(response)
					self._Catch = nil
				else
					self._PrematureRejectFlag = true
					self._PrematureReject = response
				end
			end,
		}
		local Resolver = function(response)
			Ancestor:CallThen(response)
		end
		local Rejector = function(response)
			Ancestor:CallCatch(response)
		end
		callback(Resolver, Rejector)
		return Ancestor
	end

	local function Request(params)
		return Promise(function(resolve, reject)
			CPPAPI.Request(params, function(error, response, code)
				if error then return reject(error) end
				return resolve(response)
			end)
		end)
	end

	local function GET(url)
		return Request({
			url = url,
			method = "GET"
		})
	end

	local function POST(url, data, contentType)
		contentType = contentType or "application/x-www-form-urlencoded; charset=utf-8"
		local body = ""
		if type(data) == "table" then
			local kv = {}
			for i, v in pairs(data) do
				table.insert(kv, tostring(i) .. "=" .. CPPAPI.URLEncode(v))
			end
			body = table.concat(kv, "&")
		elseif type(data) == "string" then
			body = data
		end
		return Request({
			url = url,
			body = body,
			method = "POST",
			headers = {
				["Content-type"] = contentType
			}
		})
	end

	local function GetMaster()
		return Promise(function(resolve)
			if masters ~= nil then
				return resolve(masters[1])
			end
			table.insert(pendingMasterResolves, resolve)
		end)
	end

	local function ToEndpoint(master)
		local a, b, c, d = master:match("(%d+)%.(%d+)%.(%d+)%.(%d+)")
		if a or master:find("localhost") == 1 then
			return "http://" .. master .. "/api"
		end
		return "https://" .. master .. "/api"
	end

	local function GetStaticID()
		return Promise(function(resolve, reject)
			GetMaster()
			:Then(function(defaultMaster)
				local endpoint = ToEndpoint(defaultMaster)
				GET(endpoint .. "/id?export=json")
				:Then(function(response)
					local obj = json.decode(response)
					local id, token = obj.id, obj.token
					if id and token then
						return resolve({id = id, token = token, name = "::tr:" .. id, password = token, logged = true, master = defaultMaster})
					end
					reject("Couldn't obtain static profile ID")
				end)
				:Catch(function(error)
					reject("Couldn't obtain static profile ID, error: " .. tostring(error))
				end)
			end)
		end)
	end

	local function ObtainStaticID()
		GetStaticID()
		:Then(function(profile)
			printf(GREEN .. "[CryMP] Successfully obtained static profile: %s", profile.id)
			activeProfile.static = profile
			localState.STATIC_ID = profile.id
			localState.STATIC_HASH = profile.token
			_L.CPPAPI.SetProfile("static", profile.id, profile.token, profile.time or 0)
		end)
		:Catch(function(error)
			printf(RED .. "[CryMP] " .. error)
		end)
	end

	local function Login(name, password, profileType)
		profileType = profileType or "real"
		return Promise(function(resolve, reject)
			GetMaster():Then(function(defaultMaster)
				local endpoint = ToEndpoint(defaultMaster)
				local authHost = defaultMaster
				local authEndpoint = endpoint
				local prefix, postfix = name:match("([^@]+)@(.+)")
				if prefix and postfix then
					name = prefix
					authHost = postfix
					authEndpoint = ToEndpoint(postfix)
				else
					authHost = defaultMaster
				end
				POST(authEndpoint .. "/login?export=json", {
					a = name,
					b = password
				})
				:Then(function(response)
					local obj = json.decode(response)
					if obj.error ~= nil then
						reject("Invalid credentials")
					else
						local id, token, display = obj.id, obj.token, obj.name
						activeProfile[profileType] = {
							logged = true,
							name = name,
							password = password,
							id = id,
							token = token,
							display = display,
							master = authHost
						}
						_L.CPPAPI.SetProfile(profileType, id, token, obj.time or 0)
						resolve(activeProfile[profileType])
					end
				end)
				:Catch(reject)
			end)
		end)
	end

	local function RefreshSession()
		return Promise(function(resolve, reject)
			local profile = GetProfile()
			if not profile then return reject("Couldn't find any profile") end
			local profileName = profile.name
			if profile.master ~= DEFAULT_MASTER then
				profileName = profileName .. "@" .. profile.master
			end
			Login(profileName, profile.password)
			:Then(function(session)
				resolve(session)
			end)
			:Catch(function(error)
				reject(error)
			end)
		end)
	end

	local function LoginCCommandHandler(name, password)
		Login(name, password)
		:Then(function(response)
			printf(GREEN .. "[CryMP] Successfully logged in, profile ID: %s", activeProfile.real.id)
		end)
		:Catch(function(error)
			printf(RED .. "[CryMP] Log-in failed")
		end)
	end

	local function GetValidateCommand(profile)
		local id = profile.id
		-- FIXME: Disable this for now as no server implements multimaster !validate
		--if profile.master ~= DEFAULT_MASTER then
		--	id = id .. "@" .. profile.master
		--end
		local command = string.format("!validate %s %s %s", id, profile.token, profile.display or "Nomad")
		return command
	end

	local function Authenticate(chat, refresh, localActorId)
		if chat == nil then chat = false end
		if refresh == nil then refresh = false end
		if localActorId == nil then localActorId = g_localActor.id end

		local function MainResolver(resolve, profile)
			local profile = GetProfile()
			local command = GetValidateCommand(profile)
			local retval = {
				master = profile.master,
				id = profile.id,
				token = profile.token,
				display = profile.display
			}
			if chat then
				g_gameRules.game:SendChatMessage(ChatToTarget, localActorId, localActorId, command)
				resolve(retval)
			else
				resolve(retval)
			end
		end

		return Promise(function(resolve, reject)
			if not System.IsMultiplayer() then return resolve(false) end
			local profile = GetProfile()
			if profile then
				if refresh then
					RefreshSession()
					:Then(function(session)
						MainResolver(resolve)
					end)
					:Catch(function(error)
						printf(RED .. "[CryMP] Failed to reactivate session, using old profile")
						MainResolver(resolve)
					end)
				else
					MainResolver(resolve)
				end
			else
				printf(RED .. "[CryMP] Cannot authenticate due to missing profile")
				return resolve(false)
			end
		end)
	end

	local function HookedStartWorking(self, entityId, workName)
		if workName:sub(1, 1) == "@" and ACTIVE_RPC ~= nil then
			if ALLOW_EXPERIMENTAL then
				_L.System.Log("Received RPC call: " .. workName)
			end
			local what = json.decode(workName:sub(2))
			if what and what.method then
				if ALLOW_EXPERIMENTAL then
					_L.System.Log("Resolved RPC method: " .. what.method)
				end
				local method = ACTIVE_RPC[what.method]
				if RPC_STATE then
					if what.class then
						method = ACTIVE_RPC[what.class].method
						if method then
							_pcall(method, ACTIVE_RPC[what.class], what.params, what.id)
						end
					elseif method then
						_pcall(method, what.params, what.id)
					end
				end
			end
		else
			self.work_type=workName;
			self.work_name="@ui_work_"..workName;
			HUD.SetProgressBar(true, 0, self.work_name);
		end
	end

	local function ShowConsoleServerList(data)
		-- taken from sfwcl
		printf("$0 ")
		printf("$0+$8%-3s$0+$8%-40s$0+$8%-8s$0+$8%-24s$0+$8%-14s$0+$8%-8s$0+$8%-5s$0+", "ID", "Name", "Players", "Map", "Game rules", "Version", "Flags")
		printf("$0 ")
		for i, v in pairs(data) do
			local rules = v.map:find("/ia/", nil, true) and "InstantAction" or "PowerStruggle"
			local flags = (tonumber(v.trusted) == 1 and "*" or " ") .. " " .. (tonumber(v.ranked) == 1 and "^" or " ")
			if tonumber(v.ver) ~= 6156 then
				printf("$0|$4%-3s$0|$9%-40s$0|$9%-8s$0|$9%-24s$0|$9%-14s$0|$9%-8s$0|$9%-5s$0|", i, v.name, v.numpl .. "/" .. v.maxpl, v.mapnm, rules, v.ver, flags)
			elseif v.pass ~= "0" then
				printf("$0|$1%-3s$0|$1%-40s$0|$1%-8s$0|$1%-24s$0|$1%-14s$0|$1%-8s$0|$1%-5s$0|", i, v.name, v.numpl .. "/" .. v.maxpl, v.mapnm, rules, v.ver, flags)
			else
				printf("$0|$3%-3s$0|$3%-40s$0|$3%-8s$0|$3%-24s$0|$3%-14s$0|$3%-8s$0|$3%-5s$0|", i, v.name, v.numpl .. "/" .. v.maxpl, v.mapnm, rules, v.ver, flags)
			end
		end
		printf("$0 ")
	end

	local function RequestConsoleServerList()
		GetMaster()
		:Then(function(defaultMaster)
			local endpoint = ToEndpoint(defaultMaster)
			GET(endpoint .. "/servers?export=json")
			:Then(function(response)
				ShowConsoleServerList(json.decode(response))
			end)
			:Catch(function(error)
				reject("Failed to obtain server list, error: " .. tostring(error))
			end)
		end)
	end

	local function OnUpdate(dt)
		if g_gameRules ~= nil then
			if g_gameRules.Client.ClStartWorking ~= HookedStartWorking then
				g_gameRules.Client.ClStartWorking = HookedStartWorking
			end

			UpdateWorld(localState, dt)
		end
	end

	local function OnLoadingStart()
		printf(YELLOW .. "[CryMP] Resetting local state")
		ResetState()
	end

	local function OnDisconnect(reason, message)
		printf(YELLOW .. "[CryMP] Disconnect: %d %s", reason, message)
		ResetState()
	end

	local function OnGameRulesCreated(gameRulesId)
		local gameRules = System.GetEntity(gameRulesId)

		gameRules.Client.ClSetupPlayer = function(self, playerId)
			-- original code
			self:SetupPlayer(System.GetEntity(playerId))
			-- second !validate
			Authenticate(true, false, g_localActor.id)
		end
	end

	local function OnBecomeLocalActor(localActorId)
		-- first !validate
		Authenticate(true, true, localActorId)
	end

	local function OnSpawn(entity)
		local name = entity:GetName();
		if localState.ACTIVE_LAYERS > 0 then
			local mask = localState.ACTIVE_LAYERS
			-- vehicles cannot be frozen, only dynfrozen, otherwise they are impossible to enter / drive
			if entity.vehicle then
				if band(mask, MASK_FROZEN) > 0 then
					mask = bor(mask, MASK_DYNFROZEN) - MASK_FROZEN
				end
			end
			_L.CPPAPI.ApplyMaskOne(entity.id, mask, 1)
		end
		if entity.class == "CustomAmmoPickup" then
			entity:SetFlags(ENTITY_FLAG_CASTSHADOW, 0)
		end
		if entity.class == "Player" then return; end
		if name == "frozen:all" then
			localState.ACTIVE_LAYERS = bor(localState.ACTIVE_LAYERS, MASK_FROZEN)
			_L.CPPAPI.ApplyMaskAll(MASK_FROZEN, 1)
			entity:SetPos({x=256; y=256; z=4096;})
		elseif name == "dynfrozen:all" then
			localState.ACTIVE_LAYERS = bor(localState.ACTIVE_LAYERS, MASK_DYNFROZEN)
			_L.CPPAPI.ApplyMaskAll(MASK_DYNFROZEN, 1)
			entity:SetPos({x=256; y=256; z=4096;})
		elseif name == "wet:all" then
			localState.ACTIVE_LAYERS = bor(localState.ACTIVE_LAYERS, MASK_WET)
			_L.CPPAPI.ApplyMaskAll(MASK_WET, 1)
			entity:SetPos({x=256; y=256; z=4096;})
		elseif name == "cloak:all" then
			localState.ACTIVE_LAYERS = bor(localState.ACTIVE_LAYERS, MASK_CLOAK)
			_L.CPPAPI.ApplyMaskAll(MASK_CLOAK, 1)
			entity:SetPos({x=256; y=256; z=4096;})
		elseif name:sub(1,3) == "fx:" then
			Particle.SpawnEffect(name:sub(4), entity:GetPos(), entity:GetDirectionVector(1), 1)
			entity:SetPos({x=256; y=256; z=4096;})
		elseif name:sub(1,7) == "frozen:" then
			_L.CPPAPI.ApplyMaskOne(entity.id, MASK_FROZEN, 1)
		elseif name:sub(1,10) == "dynfrozen:" then
			_L.CPPAPI.ApplyMaskOne(entity.id, MASK_DYNFROZEN, 1)
		elseif name:sub(1,4) == "wet:" then
			_L.CPPAPI.ApplyMaskOne(entity.id, MASK_WET, 1)
		elseif name:sub(1,6) == "cloak:" then
			_L.CPPAPI.ApplyMaskOne(entity.id, MASK_CLOAK, 1)
		end
	end

	System.SetCVar("con_restricted", 0)

	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_UPDATE, OnUpdate)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_DISCONNECT, OnDisconnect)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_SPAWN, OnSpawn)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_MASTER_RESOLVED, OnMasterResolved)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_LOADING_START, OnLoadingStart)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_GAME_RULES_CREATED, OnGameRulesCreated)
	CPPAPI.SetCallback(SCRIPT_CALLBACK_ON_BECOME_LOCAL_ACTOR, OnBecomeLocalActor)

	CPPAPI.AddCCommand("secu_login", LoginCCommandHandler)
	CPPAPI.AddCCommand("simple_login", LoginCCommandHandler)
	CPPAPI.AddCCommand("login", LoginCCommandHandler)
	CPPAPI.AddCCommand("static_id", ObtainStaticID)
	CPPAPI.AddCCommand("sfwcl_refresh", function()
		RefreshSession()
		:Then(function(session)
			printf(GREEN .. "[CryMP] Successfully refreshed session: %s", session.id)
		end)
		:Catch(function(error)
			printf(RED .. "[CryMP] Failed to refresh session: " .. tostring(error))
		end)
	end)
	CPPAPI.AddCCommand("auth_login", function()
		if g_localActor then
			Authenticate(true, true, g_localActor.id)
		else
			printf(RED .. "[CryMP] You are not in-game")
		end
	end)
	CPPAPI.AddCCommand("sfwcl_debug", function()
		DEBUG_MODE = not DEBUG_MODE
		if DEBUG_MODE then
			printf(GREEN .. "[CryMP] Debug mode active")
		else
			printf(RED .. "[CryMP] Debug mode disabled")
		end
	end)

	CPPAPI.AddCCommand("say", function(...)
		local text = table.concat(arg, " ")

		if g_gameRules then
			g_gameRules.game:SendChatMessage(ChatToTarget, g_localActor.id, g_localActor.id, text)
		else
			printf(RED .. "Chat is not available")
		end
	end)

	CPPAPI.AddCCommand("0", RequestConsoleServerList)

	ObtainStaticID()
	RemoveFlaws()

	ACTIVE_RPC = CreateRPC(localState, _L, { Authenticate = Authenticate })
	EXPORTED.RPC = ACTIVE_RPC
	EXPORTED.ActiveAnims = localState.ACTIVE_ANIMATIONS
	EXPORTED._pcall = _pcall
	EXPORTED.lerp = lerp
	EXPORTED.lerp2 = lerp2
	EXPORTED.lerp3 = lerp3
	EXPORTED.clamp = clamp
	EXPORTED.printf = printf
	EXPORTED.AddHook = AddHook
	EXPORTED.RemoveHook = RemoveHook
	EXPORTED.CreateBinding = CreateBinding

	if EXPORT then
		EXPORTED.Request = Request
		EXPORTED.Promise = Promise
		EXPORTED.GET = GET
		EXPORTED.POST = POST
	end

	for i,v in pairs(EXPORTED) do
		_G[i] = v
	end

	for i,v in pairs(_G) do
		if type(v) == "table" and i ~= "_G" then
			_L[i] = {}
			for j, w in pairs(v) do
				if type(w) == "function" then
					_L[i][j] = w
				end
			end
		end
	end
	return true
end

local ok, res = pcall(InitializeClient)
if not ok then
	System.LogAlways("$4[CryMP] Failed to InitializeClient: " .. tostring(res))
end
