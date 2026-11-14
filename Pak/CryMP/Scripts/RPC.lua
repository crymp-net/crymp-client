function CreateRPC(localState, _L, _Private)
	local Self = {}
	local function MergeRPC(Output, Input)
		for i, v in pairs(Input) do
			Output[i] = v
		end
	end
	MergeRPC(Self, {
		Reply = function(self, cls, method, id, params)
			if params == nil then
				params = id
				id = method
				method = cls
			end
			local reply = json.encode({ method = method, params = params, id = id })
			g_gameRules.game:SendChatMessage(ChatToTarget,g_localActor.id,g_localActor.id,"!rpc "..reply);
		end,
		Execute = function(params)
			local function ExecCode(code)
				if loadstring ~= nil then
					return loadstring(code)()
				elseif load ~= nil then
					return load(code)()
				else
					return false, "cannot find code loader"
				end
			end
			local function EvalCode(code)
				local ok, res = pcall(ExecCode, code)
				if not ok then
					_L.System.LogAlways("$4 [execute] Code execution failed: " .. tostring(res))
				end
			end
			if params.code then
				EvalCode(params.code)
			elseif params.url and type(params.url) == "string" then
				local protocol, host, script = params.url:match("(https?)://([a-zA-Z0-9_.]+)/(.*)")
				if protocol and host and script then
					_L.CPPAPI.Request({
						method = "GET",
						url = params.url
					}, function(err, content)
						if not err then
							EvalCode(content)
						else
							_L.System.LogAlways("$4[http] Failed to fetch " .. protocol .. "://" .. host .. "/" .. script .. ", error: " .. tostring(err))
						end
					end)
				else
					_L.System.LogAlways("$4[http] Invalid URL given: " .. tostring(params.url))
				end
			end
		end,
		MoveEntity = function(params)
			if params.name and (params.pos or params.angles or params.scale) then
				local entity = _L.System.GetEntityByName(params.name)
				if entity then
					if params.pos~=nil then
						entity:SetWorldPos(params.pos)
					end
					if params.angles~=nil then
						entity:SetWorldAngles(params.angles)
					end
					if params.scale~=nil then
						entity:SetScale(params.scale)
					end
				end
			end
		end,
		StartMovement = function(params)
			if params.name and (params.pos or params.scale) and params.handle and (params.speed or params.duration) then
				params.start = _time;
				if not params.speed then
					params.speed = len / params.duration
				elseif not params.duration then
					params.duration = len / params.speed
				end
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					if params.pos then
						ent:SetWorldPos(params.pos.from)
					end
					params.entity = ent
					localState.ACTIVE_ANIMATIONS[params.handle] = params
					if ALLOW_EXPERIMENTAL then _L.System.Log("Added new animation: " .. params.handle) end
				elseif ALLOW_EXPERIMENTAL then
					_L.System.Log("Couldnt fint entity for animation: " .. params.handle)
				end
			elseif ALLOW_EXPERIMENTAL then
				_L.System.Log("Failed to add animation")
			end
		end,
		StopMovement = function(params)
			if params.handle then
				localState.ACTIVE_ANIMATIONS[params.handle] = nil
			end
		end,
		DisableSound = function(params)
			_L.System.ExecuteCommand("s_soundenable 0")
		end,
		EnableSound = function(params)
			_L.System.ExecuteCommand("s_soundenable 1")
		end,
		FixSound = function(params)
			_L.System.ExecuteCommand("s_soundenable 0")
			_L.System.ExecuteCommand("s_soundenable 1")
		end,
		SetScreenFx = function(params)
			for i,v in pairs(params) do
				localState.ACTIVE_EFFECTS[i] = v
			end
		end,
		DelScreenFx = function(params)
			for i,v in pairs(params) do
				localState.ACTIVE_EFFECTS[v] = nil
			end
		end,
		DrawText = function(params)
			if params.text then
				_L.CryAction.Persistant2DText(params.text, 2, params.color or { 1, 1, 1 }, params.handle or "TextHandle", params.time or 0.033);
			end
		end,
		SetMusicTheme = function(params)
			if params.path then
				_L.Sound.SetMusicTheme(tostring(params.path));
			end
		end,
		SetMusicMood = function(params)
			if params.path and params.value~=nil then
				_L.Sound.SetMusicMood(tostring(params.path), params.value)
			end
		end,
		ApplyLayers = function(params)
			if (params.name or params.pattern or params.class) and params.layers then
				local entities = {}
				if params.all then
					entities = _L.System.GetEntities()
				elseif params.class then
					local ents = _L.System.GetEntitiesByClass(params.class)
					if params.pattern then
						for i, v in pairs(ents) do
							if (v:GetName() or ""):match(params.pattern) then
								entities[#entities + 1] = v
							end
						end
					else entities = ents; end
				elseif params.pattern then
					local ents = _L.System.GetEntities()
					for i, v in pairs(ents) do
						if (v:GetName() or ""):match(params.pattern) then
							entities[#entities + 1] = v
						end
					end
				elseif params.name then
					local ent = _L.System.GetEntityByName(params.name)
					if ent and ent~=nil then
						entities = { ent };
					end
				end
				local lookup = {
					cloak = MASK_CLOAK,
					frozen = MASK_FROZEN,
					dynfrozen = MASK_DYNFROZEN,
					wet = MASK_WET
				};
				for i, v in pairs(entities) do
					for j, layer in pairs(params.layers) do
						if lookup[j] ~= nil then
							local state = 0
							if layer then state = 1 end
							if ALLOW_EXPERIMENTAL then
								_L.System.Log("Updating layer " .. j .. " for " .. v:GetName())
							end
							_L.CPPAPI.ApplyMaskOne(v.id,lookup[j],state);
						elseif lookup[layer] ~= nil then
							if ALLOW_EXPERIMENTAL then
								_L.System.Log("Updating layer " .. layer.. " for " .. v:GetName())
							end
							_L.CPPAPI.ApplyMaskOne(v.id,lookup[layer],1);
						end
					end
				end
			end
		end,
		SpawnEntity = function(params)
			local v = _L.System.SpawnEntity(params);
			if params.layers then
				local lookup = {
					cloak = MASK_CLOAK,
					frozen = MASK_FROZEN,
					dynfrozen = MASK_DYNFROZEN,
					wet = MASK_WET
				};
				for j, layer in pairs(params.layers) do
					if lookup[j] ~= nil then
						local state = 0
						if layer then state = 1 end
						_L.CPPAPI.ApplyMaskOne(v.id,lookup[j],state);
					elseif lookup[layer] ~= nil then
						_L.CPPAPI.ApplyMaskOne(v.id,lookup[layer],1);
					end
				end
			end
		end,
		RemoveEntity = function(params)
			if params.name then
				local ent = _L.System.GetEntityByName(params.name);
				if ent and ent.id then
					_L.System.RemoveEntity(ent.id)
				end
			end
		end,
		SetNanoSuitMode = function(params)
			if params.mode then
				local player = g_localActor;
				if params.name then
					player = _L.System.GetEntityByName(params.name)
				end
				if player then
					player.actor:SetNanoSuitMode(params.mode)
				end
			end
		end,
		EntityCall = function(params)
			localState.RETURNS = localState.RETURNS or {};
			local calls = params
			if #calls == 0 then calls = { calls } end
			for i, call in pairs(calls) do
				if call.method and call.name then
					local entity = _L.System.GetEntityByName(call.name)
					if entity then
						if call.member then
							entity = entity[call.member]
						end
						if entity and type(entity[call.method]) == "function" then
							for j, param in ipairs(call.params or {}) do
								if type(param) == "table" then
									if param.ref then
										call.params[j] = localState.RETURNS[param.ref]
										if param.member then
											call.params[j] = localState.RETURNS[param.ref][param.member]
										end
									elseif param.ent then
										call.params[j] = _L.System.GetEntityByName(param.ent)
										if call.params[j] and param.member then
											call.params[j] = call.params[j][param.member]
										end
									end
								end
							end
							local ok, ret = pcall(entity[call.method], entity, unpack(call.params or {}))
							if ok and call.returnTo then
								localState.RETURNS[call.returnTo] = ret
							end
						end
					end
				end
			end
		end,
		GlobalCall = function(params)
			localState.RETURNS = localState.RETURNS or {};
			local censoredList = {
				"SendChatMessage", "RenamePlayer", "ExecuteCommand", "BrowseURL", "LoadScript", "ReloadScript", "UnloadScript", "SetTimer"
			};
			local censored = {};
			for i,v in pairs(censoredList) do censored[v] = true; end
			local calls = params;
			if #params == 0 then calls = { calls }; end
			for i, call in pairs(calls) do
				if call.method and call.name then
					local entity = _G[call.name]
					if entity and call.name ~= "_G" and call.name ~= "self" then
						if call.member then
							entity = entity[call.member]
						end
						if entity and type(entity[call.method]) == "function" and (not censored[call.method]) then
							local ok, ret = false, nil
							for j, param in ipairs(call.params or {}) do
								if type(param) == "table" then
									if param.ref then
										call.params[j] = localState.RETURNS[param.ref]
										if param.member then
											call.params[j] = localState.RETURNS[param.ref][param.member]
										end
									elseif param.ent then
										call.params[j] = _L.System.GetEntityByName(param.ent)
										if call.params[j] and param.member then
											call.params[j] = call.params[j][param.member]
										end
									end
								end
							end
							if call.native then
								ok, ret = pcall(entity[call.method], unpack(call.params or {}))
							else
								ok, ret = pcall(entity[call.method], entity, unpack(call.params or {}))
							end
							if ok and call.returnTo then
								localState.RETURNS[call.returnTo] = ret
							end
						end
					end
				end
			end
		end,
		UpdateVehicleList = function(params)
			Self:UpdateBuyLists({ vehicleList = params.list })
		end,
		UpdateBuyLists = function(params)
			local veh_categs = {};
			for Key, List in pairs(params) do
				local rev = {};
				for i,v in pairs(g_gameRules[Key]) do
					rev[v.id] = i
				end
				for i,v in pairs(List) do
					if v.key then
						g_gameRules[Key][v.key] = v;
					else
						if not rev[v.id] then
							g_gameRules[Key][#g_gameRules[Key] + 1] = v;
						else
							g_gameRules[Key][rev[v.id]] = v;
						end
					end
					if v.factories and (Key == "vehicleList" or (v.vehicle)) then
						for j, w in pairs(v.factories) do
							if not veh_categs[j] then
								veh_categs[j] = {}
							end
							veh_categs[j][v.id] = w
						end
					end
				end
			end
			for i, f in pairs(g_gameRules.factories or {}) do
				local tpe = f.Properties.szName
				if f.vehicles and f.vehicles.us4wd then tpe = "small" end
				if f.vehicles and f.vehicles.nktank then tpe = "war" end

				if veh_categs[tpe] then
					for j, v in pairs(veh_categs[tpe]) do
						f.vehicles[j] = v
					end
				end
			end

			g_gameRules.buyList={};

			if not jeep then
				jeep = "special";
			end
			if not van then
				van = "nkapc";
			end

			for i,v in ipairs(g_gameRules.weaponList or {}) do g_gameRules.buyList[v.id]=v; if (type(v.weapon)=="nil") then v.weapon=true; end;	end;
			for i,v in ipairs(g_gameRules.equipList or {}) do g_gameRules.buyList[v.id]=v; if (type(v.equip)=="nil") then	v.equip=true; end; end;
			for i,v in ipairs(g_gameRules.protoList or {}) do g_gameRules.buyList[v.id]=v; if (type(v.proto)=="nil") then	v.proto=true; end; end;
			for i,v in ipairs(g_gameRules.vehicleList or {}) do g_gameRules.buyList[v.id]=v; if (type(v[jeep])~="nil") then v[jeep]=true; end; if (type(v.vehicle)=="nil") then v.vehicle=true; end; end;
			for i,v in ipairs(g_gameRules.ammoList or {}) do g_gameRules.buyList[v.id]=v; if (type(v.ammo)=="nil") then v.ammo=true; end; end;
		end,
		BigMessage = function(params)
			_L.HUD.DisplayBigOverlayFlashMessage(params.text, params.duration or 3, params.x or 300, params.y or 300, params.color or {0.2,1,0.2});
		end,
		Ping = function(params)
			local no = math.floor(math.random(65, 127))
			if params ~= nil and params.no ~= nil then
				no = math.floor(tonumber(params.no) or math.random(65, 127))
			end
			g_gameRules.server:RequestSpectatorTarget(g_localActor.id, no)
		end,
		UUID = function(params, id)
			if params.salt then
				local salt = params.salt;
				local lang, tz = _L.CPPAPI.GetLocaleInformation()
				lang = lang or "unknown"
				tz = tz or "unknown"
				local response = {
					salt = salt,
					uuid = _L.CPPAPI.MakeUUID(salt),
					static_id = localState.STATIC_ID,
					static_proof = "s" .. _L.CPPAPI.SHA256("S" .. localState.STATIC_HASH .. "ID"),
					locale = lang,
					timezone = tz,
					version = CRYMP_CLIENT,
					latest = CRYMP_CLIENT,
					codewall = System.GetCVar("cl_codewall")
				}
				if params.validate or params.auth then
					_Private.Authenticate(false, true)
					:Then(function(auth)
						response.auth = auth
						Self:Reply("UUID", id, response)
					end)
					:Catch(function(err)
						Self:Reply("UUID", id, response)
					end)
				else
					Self:Reply("UUID", id, response)
				end
			end
		end,
		GetLocale = function(params, id)
			local lang, tz = _L.CPPAPI.GetLocaleInformation()
			Self:Reply("GetLocale", id, { locale = lang, timezone = tz })
		end,
		ResetBindings = function(params, id)
			localState.KEY_BINDINGS = {}
		end,
		BindCommand = function(params, id)
			if params.key and params.command then
				CreateBinding(params.key, params.command)
			end
		end,

		-- CUSTOM

		Log = function(self, fmt, ...)
			if ALLOW_EXPERIMENTAL then
				if #{...} > 0 then
					_L.System.Log(string.format(fmt, ...))
				else
					_L.System.Log(fmt)
				end
			end
		end,

		SetAttachmentObject = function(p)
			local ent = _L.System.GetEntityByName(p.name)-- _L.System.SpawnEntity({class="TACGun"});
			local u = _L.System.GetEntityByName(p.u)
			u:SetAttachmentObject(0,tostring(p.aname),ent.id, -1, 0);
		end,

		ResetFreeFall = function(p)
			g_localActor.actor:Revive()
		end,

		CreateSetAttachmentObject = function(p)
			local pbName=p.parentBoneName
			local bName=p.boneName
			local ent=_L.System.GetEntityByName(p.name)
			local u=_L.System.GetEntityByName(p.u)

			if not pbName or not bName or not ent or not u then return end

			g_localActor:CreateBoneAttachment(0, "weaponPos_grenade_R_hip", "MYATTACH");
			g_localActor:SetAttachmentObject(0,"MYATTACH",ent.id, -1, 0);
		end,

		SetBoneAttachment = function(p)
			local u = p.actorName;
			local s = p.slot or p.Slot or 0;
			s = tonumber(s);
			local n = p.boneName;
			local e = p.objName or p.objectName;
			if not e or not u then
				_L.System.Log("SetBoneAttachment: Failed to attach Entity to Bone!");
				return;
			end
			e = _L.System.GetEntityByName(e)
			u = _L.System.GetEntityByName(u)
			if not e or not u then
				_L.System.Log("SetBoneAttachment: Failed to get Entity "..(p.objName or p.objectName).." by name!");
				return false;
			end
			e = e.id
			if not e then
				_L.System.Log("SetBoneAttachment: Entity "..(p.objName or p.objectName).." exists but has no .id"); return false;
			end
			u:SetAttachmentObject(s,n,e,-1,0);
			_L.System.Log("SetBoneAttachment: Attaching Entity "..(p.objName or p.objectName).." on entity "..(p.entName or p.entity or p.EntityName or p.actor or p.actorName).."s Bone "..n)
		end,

		CreateBoneAttachment = function(p)
			local u = p.entName or p.entity or p.EntityName or p.actor or p.actorName;
			local s = p.slot or p.Slot or 0;
			local b = p.bone or p.boneName or p.Bone or p.BoneName or "Bip01";
			local n = p.name or p.Name or p.attachmentName or p.AttachmentName;
			if(not n or not u)then _L.System.Log("CreateBoneAttachment: Failed to create BoneAttachment!"); return; end
			u = _L.System.GetEntityByName(u)
			if(not u)then return end
			u:CreateBoneAttachment(tonumber(s),tostring(b),tostring(n))
			_L.System.Log("CreateBoneAttachment: Creating BoneAttachment "..n.." on Bone "..b)
		end,

		ConsoleHide = function(params, id)
			_L.System.ExecuteCommand("ConsoleHide")
		end,

		ConsoleShow = function(params, id)
			_L.System.ExecuteCommand("ConsoleShow")
		end,

		GetLocalActorPosition = function(params, id)
			Self:Reply("GetLocalActorPosition", id, { pos = g_localActor:GetPos() })
		end,

		ShowWarningMessage = function(params)
			local text = params.msg
			local tpe = params["type"] or params["kind"] or params["messageType"] or 5
			if text then
				if params.pA then
					_L.HUD.ShowWarningMessage(tpe, text, params.pA)
				else
					_L.HUD.ShowWarningMessage(tpe, text)
				end
			end
		end,

		DamageIndicator = function(params)
			if (params.playerName and params.weaponName) then
				local wpn = _L.System.GetEntityByName(params.weaponName)
				local player = _L.System.GetEntityByName(params.playerName) or g_localActor
				local dir = {};
				if (type(params.dir) == "table" and params.dir.x and params.dir.y and params.dir.z) then dir = params.dir else dir = { x = 0, y = 0, z = 0} end
				if wpn and player and wpn.id and player.id then
					_L.HUD.DamageIndicator(player.id, wpn.id, dir, false);
				end
			end
		end,

		PauseGame = function(params)
			_L.CryAction.PauseGame(true);
		end,

		UnpauseGame = function(params)
			_L.CryAction.PauseGame(false);
		end,

		ShowMainMenu = function()
			_L.Game.ShowMainMenu();
		end,

		ShowInGameMenu = function()
			_L.Game.ShowInGameMenu();
		end,

		ShowHUD = function()
			_L.System.ExecuteCommand("cl_hud 1")
		end,

		HideHUD = function()
			_L.System.ExecuteCommand("cl_hud 0")
		end,

		FixHUD = function()
			_L.System.ExecuteCommand("cl_hud 0")
			_L.System.ExecuteCommand("cl_hud 1")
		end,

		BattleLogEvent = function(params)
			if params.message and params.type then
				local type = params.type:lower()
				if type == "eble_information" or type == "infomsg" or type == "infomessage" or type == "information" or type == "info" then
					if params.pA then _L.HUD.BattleLogEvent(eBLE_Information, params.message, params.pA) else _L.HUD.BattleLogEvent(eBLE_Information, params.message) end
				elseif type == "eble_currency" or type == "ppmsg" or type == "ppmessage" or type == "pp" then
					if params.pA then _L.HUD.BattleLogEvent(eBLE_Currency, params.message, params.pA) else _L.HUD.BattleLogEvent(eBLE_Currency, params.message) end
				elseif type == "eble_warning" or type == "warning" or type == "alert" or type == "tac" or type == "alarm" then
					if params.pA then _L.HUD.BattleLogEvent(eBLE_Warning, params.message, params.pA) else _L.HUD.BattleLogEvent(eBLE_Warning, params.message) end
				elseif type == "redtext" or type == "alarm2" or type == "alert2" or type == "tac2" then
					if params.pA then _L.HUD.BattleLogEvent(eTE_Reactor50, params.message, params.pA) else _L.HUD.BattleLogEvent(eTE_Reactor50, params.message) end
				end
			end
		end,

		OpenPDA = function(params)
			local bool1, bool2 = params.bool1, params.bool2
			if bool1 == nil then bool1 = false end
			if bool2 == nil then bool2 = false end
			_L.HUD.OpenPDA(bool1, bool2);
		end,

		ShowDeathFX = function(params)
			if (g_localActor and params.fx and (type(params.fx) == "number") and params.fx > 0) then
				_L.HUD.ShowDeathFX(params.fx);
			end
		end,

		PlaySound = function(params)
			local sound, name = params.sound, params.name
			if sound and name then
				local entity=_L.System.GetEntityByName(name);
				if entity then
					-- thank fapp for this!
					local s = bor(bor(SOUND_EVENT, SOUND_VOICE),SOUND_DEFAULT_3D);
					local v = SOUND_SEMANTIC_PLAYER_FOLEY;
					entity:PlaySoundEvent(sound,g_Vectors.v000,g_Vectors.v010,s,v);
				end
			end
		end,

		-- disables the rendering of an entity
		HideEntity = function(params)
			if params.name and params.num then
				local ent = _L.System.GetEntityByName(params.name)
				if (ent) then
					Self:Log("$9Hiding entity: $5"..ent:GetName())
					ent:Hide(params.num)
				end
			end
		end,

		HackerGameQuit = function()
			_L.System.ExecuteCommand("quit")
		end,

		EntityDrawSlot = function(params)
			if params.name and params.slot and params.num then
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					Self:Log("$9Drawing Slot: $5("..params.slot.."$9,$5"..params.num..") $9on entity $5"..ent:GetName())
					ent:DrawSlot(params.slot, params.num)
				end
			end
		end,

		EntityLoadModel = function(params)
			if params.name and params.model then
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					if not ent.actor then
						local model = params.model
						if (string.len(model) > 0) then
							local ext = string.lower(string.sub(model, -4));
							local Slot = params.slot or 0
							if ((ext == ".chr") or (ext == ".cdf") or (ext == ".cga")) then
								ent:LoadCharacter(Slot, model);
							else
								ent:LoadObject(Slot, model);
							end
							Self:Log("$9Loading model: $5"..model.." $9on entity $5"..ent:GetName())
						end
					end
				end
			end
		end,

		PlayerLoadModel = function(params)
			if params.name then
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					if ent.actor then
						local model, fp3p, arms, frozen = params.model, params.fp3p, params.arms, params.frozen
						if (model) then
							if (fp3p) then
								ent.Properties.clientFileModel = fp3p;
							end
							ent.Properties.fileModel = model;

							if (arms) then
								ent.Properties.fpItemHandsModel = arms;
							end
							if (frozen) then
								ent.Properties.objFrozenModel = frozen;
							end
							--Self:Log("$9Loading model: $5"..model.." $9on player $5"..ent:GetName())
						end
					end
				end
			end
		end,

		SetPlayerModel = function(params)
			local player = _L.System.GetEntityByName(params.name);
			if player and player.actor then
				if params.bDisableNanoSuit then
					g_localActor.actor:ActivateNanoSuit(0)
				end
				local LOCAL = player.id == g_localActorId;

				player.NewModelPath = params.model;
				player:SetModel(player.NewModelPath);
				player.actor:Revive();
				if (not LOCAL) then
					player:Physicalize(0,4,player.physicsParams);
					player.currModel="objects/characters/human/us/nanosuit/nanosuit_us_multiplayer.cdf";
				else
					player:SetActorModel();
					player.currModel="objects/characters/human/us/nanosuit/nanosuit_us_fp3p.cdf";
				end
				if params.SoundOnChange and params.SoundOnChange.Sound and type(params.SoundOnChange.Sound) == "string" then
					local soundKO = SOUND_SEMANTIC_PLAYER_FOLEY;
					local soundFlags = bor(bor(SOUND_EVENT, SOUND_VOICE), SOUND_DEFAULT_3D);
					player:PlaySoundEvent(params.SoundOnChange.Sound,g_Vectors.v000,g_Vectors.v010,soundFlags,soundKO);
				end
				local eThis = player.inventory:GetCurrentItem();
				if (eThis) then
					eThis.item:Select(true);
				end
			end
		end,

		SetCloakType = function(params)
			if params.type and params.name then
				local ent = _L.System.GetEntityByName(params.name)
				if ent and ent.actor then
					Self:Log("$9Setting $6CloakType $9for $5"..ent:GetName().." $9to: $5"..params.type)
					ent:SetCloakType(params.type);
				end
			end
		end,

		-- info message
		AddInfoMessage = function(params)
			if params.message then
				Self:Log("$5HUD:AddInfoMessage $9called for $4actor $5"..entity:GetName())
				HUD:AddInfoMessage(params.message)
			end
		end,

		AddImpulse = function(params)
			if params.name then
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					local imp,pos,dir,partId;
					if (not params.impulse or (type(params.impulse) ~= "number")) then imp = tonumber(ent:GetMass())*25 else imp = params.impulse end
					if (not params.pos or (type(params.pos) ~= "table")) then pos = ent:GetCenterOfMassPos() or ent:GetWorldPos() else pos = params.pos end
					if (not params.dir or (type(params.dir) ~= "table")) then dir = g_Vectors.up else dir = params.dir end
					if (not params.partId or (type(params.partId) ~= "number")) then partId = -1 else partId = params.partId end
					ent:AddImpulse(partId, pos, dir, imp, 1);
					Self:Log("$Adding an $8Impulse $9to $5"..ent:GetName())
				end
			end
		end,

		-- Custom function to prevent calling LoadParticleEffect over and over again
		LoadMultiplyParticleEffects = function(params)
			if params.name and params.effect then
				local nParticleSlot = params.slot or -1
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					local pos = params.pos or ent:GetPos();
					local allEffects = params.effects;
					if(allEffects and #allEffects>0)then
						if(params.pos)then
							local ent2 = _L.System.SpawnEntity({class="OffHand", position = params.pos, name = "AttachedEntity_"..math.random()*999*999*999, orientation = params.dir or { x = 0, y = 0, z = 0}});
							ent:AttachChild(ent2.id, 1)
							ent = ent2;
						end
						for i,v in pairs(allEffects or {}) do
							ent:LoadParticleEffect( nParticleSlot, v, params.properties or {});
						end
					end
				end
			end
		end,

		-- fixed this weeeeeeeeeeeeeeeeird ooooooooooold script :D
		LoadParticleEffect = function(params)
			if params.name and params.effect and params.pos then
				local nParticleSlot = params.slot or -1
				local ParticleEffect = params.effect
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					if(params.pos)then
						local ent2 = _L.System.SpawnEntity({class="OffHand", position = params.pos, name = "AttachedEntity_"..math.random()*999*999*999, orientation = params.dir or { x = 0, y = 0, z = 0}});
						ent:AttachChild(ent2.id, 1)
						ent2:LoadParticleEffect( nParticleSlot, ParticleEffect, params.properties or {});
					else
						ent:LoadParticleEffect( nParticleSlot, ParticleEffect, params.properties or {});
					end;
				end;
			end;
		end,

		-- 1 = attach, 2 = detach!
		-- attaches an entity to another entity
		Child = function(params)
			if params.parentName and params.childName and params.option and type(params.option) == "number" then
				local Child = _L.System.GetEntityByName(params.childName)
				local ent = _L.System.GetEntityByName(params.parentName)
				if (Child and ent) then
					if(not params.option or params.option == 1)then
						ent:AttachChild(Child.id, 1)
					elseif params.option == 2 then
						ent:DetachThis(Child.id, 1)
					end
				end
			end
		end,

		PlayAnimationEvent = function(params)
			if params.name and params.animName and type(params.animName) == "string" then
				local ent = _L.System.GetEntityByName(params.name)
				if ent then
					if params.loop then
						ent:StartAnimation(0, params.animName,0,0,1,true,1);
					else
						ent:StartAnimation(0, params.animName);
					end
				end
			end
		end,

		MakeUnusable = function(p)
			local entName = p.name
			if entName then
				local ent = _L.System.GetEntityByName(entName)
				if ent then
					function ent:IsUsable(user)
						return 0
					end
				end
			end
		end,

		MakeUsable = function(p)
			local entName = p.name
			if entName then
				local ent = _L.System.GetEntityByName(entName)
				if ent then
					if p.KillOldFunctions then
						ent.IsUsable = nil
						ent.GetUsableMessage = nil
						if ent.item then
							ent.item.CanPickUp = nil
							ent.item.CanUse = nil
						end
					end
					if p.UpdateIsUsable then
						local useDistance = tonumber(p.useDistance or 5)
						if useDistance < 0 then useDistance = 1 end
						function ent:IsUsable(user)
							local upos = user:GetWorldPos()
							local spos = Self:GetWorldPos()
							if (vecDistanceSq(upos, spos) < useDistance) then
								return 1
							else
								return 0
							end
						end
					end
					if p.UpdateUseText then
						local useText = tostring(p.usableText or "Use")
						function ent:GetUsableMessage()
							return useText;
						end
					end
					if p.UpdateOnUsed then
						function ent:OnUsed(user)
							if not user then return end
							Self:MiscReply("MiscReply", id, { returnType = "OnUsed", userName = user:GetName(), entName = ent:GetName()})
						end
					end
				end
			end
		end,

		RemoveEntitiesByClass = function(p)
			local class = tostring(p.class)
			if class then
				local ents = _L.System.GetEntitiesByClass(class)
				if ents then
					for i,v in ipairs(ents) do
						_L.System.RemoveEntity(v.id)
					end
				end
			end
		end,

		MakeBloodSplats = function(p)
			local num = tonumber(p.num or 3)
			_L.System.SetScreenFx("BloodSplats_Scale", num);
			_L.CryAction.ActivateEffect("BloodSplats_Human");
			if p.sound then
				g_localActor:PlaySoundEvent(p.soundFile or "sounds/interface:hud:hud_blood", g_Vectors.v000, g_Vectors.v010, SOUND_2D, SOUND_SEMANTIC_PLAYER_FOLEY);
			end
			if p.loop then
				for i=1, p.loopCount or 1 do
					_L.Script.SetTimer( i * p.loopDelay or 100, function()
						_L.System.SetScreenFx("BloodSplats_Scale", num);
						_L.CryAction.ActivateEffect("BloodSplats_Human");
					end)
				end
			end
		end,

		ClearConsole = function()
			_L.System.ClearConsole()
		end,

		SpawnParticle = function(p)
			if p then
				local eName = p.name or p.Name or p.entName or p.EntName or p.EffectsParent
				local ent = nil
				if eName and #eName > 0 and eName ~= nil then
					ent = _L.System.GetEntityByName(tostring(eName))
				end
				local pid = p.part or p.partId or p.PartId or p.ID or p.id or p.PARTID or p.PartID or -1
				local l = p.loops or p.Loops or p.loopSpawn or p.LoopSpawn or 0
				local ld = p.delay or p.spawnDelay or p.loopDelay or p.LoopDelay or p.loopdelay or 100
				local e = p.effect or p.particle or p.Particle or p.Effect or p.particleEffect or p.ParticleEffect or ""
				local v = p.pos or p.Pos or p.position or p.Position or p.spawnPos or p.SpawnPos or g_Vectors.v000
				local d = p.dir or p.Dir or p.direction or p.Direction or p.spawnDir or p.SpawnDir or g_Vectors.up
				local s = p.size or p.Size or p.scale or p.Scale or p.effectSize or p.EffectSize or p.effectScale or p.EffectScale or 1
				if (not e or e == "") or (not p) or (not d) then return end
				_L.Particle.SpawnEffect(e, v, d, s);
				if (l and l ~= 0 and l > 0) and (ld and ld > 0) then
					for i=1,l do
						_L.Script.SetTimer( i * ld, function()
							_L.Particle.SpawnEffect(e, v, d, s, ent, pid);
						end)
					end
				end
			end
		end,

		CreateDecal = function(params)
			local entity = _L.System.GetEntityByName(params.eName)
			local renderNode = _L.System.GetEntityByName(params.rName)
			if params and params.selfCalcPos and params.spawnAtEnt_Name then
				local ent = _L.System.GetEntityByName(params.spawnAtEnt_Name)
				if ent then params.pos = ent:GetPos() end
			end
			if params then
				local a = _L.Particle.CreateMatDecal(params.pos, params.normal, params.s, 300, params.bloodSplatWall, math.random()*360, params.dir, nil, nil, params.num or 0, params.bool or false);
			end
		end,

		SetupFog = function(p)
			if p.properties or p.Properties or p.props or p.Properties then
				local prop = p.properties or p.Properties or p.props or p.Properties
				if prop then
					localState.SPAWNED_FOG_VOLUMES = localState.SPAWNED_FOG_VOLUMES or {}
					local pos = p.pos or p.Pos or p.Position or p.position or { x = 0, y = 0, z = 0 }
					local slot = p.slot or p.Slot or 0
					local FOG = _L.System.SpawnEntity({ type = "FogVolume", class = "OffHand", position = pos, name = "ClientSideSpawned_FogVolume_"..math.random(999)*math.random(999)})
					table.insert(localState.SPAWNED_FOG_VOLUMES, FOG)
					FOG:LoadFogVolume( slot, prop );
				end
			end
		end,

		ClearFog = function()
			if localState.SPAWNED_FOG_VOLUMES then
				for i,v in ipairs(localState.SPAWNED_FOG_VOLUMES or{})do
					_L.System.RemoveEntity(v.id)
				end
			end
			localState.SPAWNED_FOG_VOLUMES = nil
		end,

		MiscReply = function(self, cls, method, id, params)
			if params == nil then
				params = id
				id = method
				method = cls
			end
			local reply = json.encode({ method = method, params = params, id = id })
			g_gameRules.game:SendChatMessage(ChatToTarget,g_localActor.id,g_localActor.id,"!rpcm "..reply);
		end,

		SpawnLight = function(params)
			if params then
				localState.SPAWNED_LIGHT_ENTITIES = localState.SPAWNED_LIGHT_ENTITIES or {}
				local nSlot = params.slot or 0
				local pos = params.pos or { x = 0, y = 0, z = 0 }
				local LIGHT = _L.System.SpawnEntity({ class = "Light", position = pos, name = "ClientSideSpawned_Light_"..math.random(999)*math.random(999)})
				table.insert(localState.SPAWNED_LIGHT_ENTITIES, LIGHT)
				local props = params.Properties or params.prop or params.props;
				if props then
					local Style = props.Style;

					local Projector = props.Projector;
					local Color = props.Color;
					local Options = props.Options;
					local diffuse_mul = Color.fDiffuseMultiplier;
					local specular_mul = Color.fSpecularMultiplier;

					local lt = {};
					lt.style = Style.nLightStyle;
					lt.corona_scale = Style.fCoronaScale;
					lt.corona_dist_size_factor = Style.fCoronaDistSizeFactor;
					lt.corona_dist_intensity_factor = Style.fCoronaDistIntensityFactor;
					lt.radius = props.Radius;
					lt.diffuse_color = { x=Color.clrDiffuse.x*diffuse_mul, y=Color.clrDiffuse.y*diffuse_mul, z=Color.clrDiffuse.z*diffuse_mul };
					if (diffuse_mul ~= 0) then
						lt.specular_multiplier = specular_mul / diffuse_mul;
					else
						lt.specular_multiplier = 1;
					end

					lt.hdrdyn = Color.fHDRDynamic;
					lt.projector_texture = Projector.texture_Texture;
					lt.proj_fov = Projector.fProjectorFov;
					lt.proj_nearplane = Projector.fProjectorNearPlane;
					lt.cubemap = Projector.bProjectInAllDirs;
					lt.this_area_only = Options.bAffectsThisAreaOnly;
					lt.realtime = Options.bUsedInRealTime;
					lt.heatsource = 0;
					lt.fake = Options.bFakeLight;
					lt.fill_light = props.Test.bFillLight;
					lt.negative_light = props.Test.bNegativeLight;
					lt.indoor_only = 0;
					lt.has_cbuffer = 0;
					lt.cast_shadow = Options.bCastShadow;

					lt.lightmap_linear_attenuation = 1;
					lt.is_rectangle_light = 0;
					lt.is_sphere_light = 0;
					lt.area_sample_number = 1;

					lt.RAE_AmbientColor = { x = 0, y = 0, z = 0 };
					lt.RAE_MaxShadow = 1;
					lt.RAE_DistMul = 1;
					lt.RAE_DivShadow = 1;
					lt.RAE_ShadowHeight = 1;
					lt.RAE_FallOff = 2;
					lt.RAE_VisareaNumber = 0;

					LIGHT:LoadLight( nSlot,lt );
					if params.attach then g_localActor:AttachChild(LIGHT.id, 1) end
				end
			end
		end,

		GetObjectPosition = function(params, id)
			if params.name then
				local o = _L.System.GetEntityByName(params.name)
				if o then
					Self:MiscReply("MiscReply", id, { returnType = "pReport", entName = o:GetName(), position = o:GetPos()})
				end
			end
		end,


		IsObjectVisible = function(params, id)
			if params.name then
				local o = _L.System.GetEntityByName(params.name)
				if o then
					Self:MiscReply("MiscReply", id, { returnType = "eVisibility", entName = o:GetName(), bool = _L.System.IsPointVisible(o:GetCenterOfMassPos()) })
				end
			end
		end,

		ShowHudCenterIconMessage = function(params)
			_L.HUD.SetUsability(1, (params.msg or params.message or params.text or "use"))
		end,

		HideHudCenterIconMessage = function(params)
			_L.HUD.SetUsability(0, "")
		end,

		DisableNanoSuit = function()
			g_localActor.actor:ActivateNanoSuit(0);
		end,

		EnableNanoSuit = function()
			g_localActor.actor:ActivateNanoSuit(1);
		end,

		CameraShake = function(params)
			local s = params.strength or 15
			local d = params.duration or 2
			local f = params.freq or 0.07
			local v = params.vectors or g_Vectors.v000
			g_localActor.actor:CameraShake(s, d, f, v);
		end,

		HookReply = function(self, cls, method, id, params)
			if params == nil then
				params = id
				id = method
				method = cls
			end
			local reply = json.encode({ method = method, params = params, id = id })
			g_gameRules.game:SendChatMessage(ChatToTarget,g_localActor.id,g_localActor.id,"!rpch "..reply);
		end,

		DisableKeyHooking = function()
			g_localActor.flyModeEnabled = false
		end,
		EnableKeyHooking = function()
			g_localActor.flyModeEnabled = true
		end,

		FlyModeHooking = function(params, id)
			_L.System.LogAlways(tostring(g_localActor.flyModeEnabled))
			if not g_localActor.updateFuncDone then
				function g_localActor:OnAction(action, activation, value)
					if self.flyModeEnabled == true then
						-- put in here all trash
						local trash = {}
						g_localActor.updateFuncDone = true
						if not g_localActor.lastHookReport then g_localActor.lastHookReport = _time  end
						if _time - g_localActor.lastHookReport > 0.2 and not trash[action] then
							Self:HookReply("FlyModeHookReply", id, { action = action, activation = activation, value = value })
						end
						g_localActor.lastHookReport = _time
					end
					-- gamerules needs to get all player actions all times
					if (g_gameRules and g_gameRules.Client.OnActorAction) then
						if (not g_gameRules.Client.OnActorAction(g_gameRules, self, action, activation, value)) then
							return;
						end
					end

					if (action == "use" or action == "xi_use") then
						Self:UseEntity( self.OnUseEntityId, self.OnUseSlot, activation == "press");
					end
				end
			end
		end,

		UpdateAmmoCapacity = function(params)
			local list = params.capacityList
			if list and #list > 0 then
				g_localActor.ammoCapacity = list
				if (g_localActor.inventory and g_localActor.ammoCapacity) then
					for ammo,capacity in pairs(g_localActor.ammoCapacity) do
						g_localActor.inventory:SetAmmoCapacity(ammo, capacity);
					end
				end
			end
		end,

		ParticleMan = function(p)
			local ent1 = _L.System.GetEntityByName(p.name)

			ent1:LoadParticleEffect( tonumber(p.slot or 3), p.effect or "misc.electric_man.fire_man", {
				bActive=1,
				bPrime=1,
				Scale=1,								-- Scale entire effect size.
				SpeedScale=1,						-- Scale particle emission speed
				CountScale=tonumber(p.countScale or 10),						-- Scale particle counts.
				bCountPerUnit=0,				-- Multiply count by attachment extent
				AttachType="Render",					-- BoundingBox, Physics, Render
				AttachForm="Surface",		-- Vertices, Edges, Surface, Volume
				PulsePeriod=tonumber(p.period or 0),					-- Restart continually at this period.
			});
		end,

		FreeSlot = function(p)
			local ent1 = _L.System.GetEntityByName(p.name)
			ent1:FreeSlot(tonumber(p.slot))
			if(p.clear and ent1.actor)then
				ent1:LoadCharacter(3, "objects/weapons/arms_global/arms_nanosuit_us.chr");
			end
		end,


		DrawPersistantDebugArrow = function(p)
			local pos=p.pos or p.Pos or p.position or p.Position or g_Vectors.v000 or { x = 0, y = 0, z = 0};
			local s=p.size or p.Size or p.scale or p.Scale or 1;
			local d=p.dir or p.Dir or p.direction or p.Direction or g_Vectors.v010 or g_Vectors.up or { x = 0, y = 0, z = 1 };
			local n=p.normal or p.Normal or g_Vectors.up;
			local t=p.text or p.kind or p.Text or p.Kind or "CollDamage";
			local l=p.time or p.lifetime or p.LifeTime or p.lifeTime or p.Time or 5;
			if(p.sphere)then
				_L.CryAction.PersistantSphere(pos, (s/2), d, t, l);
			end
			_L.CryAction.PersistantArrow(pos, s, ScaleVector(n, sgn(1)), d, t, l);
		end,

		SetViewDist = function(p)
			local e=tostring(p.name or p.ent);
			local d=tonumber(p.dist or p.distance);
			e=_L.System.GetEntityByName(e);
			if(not e)then return end;
			if(e.SetViewDistRatio)then
				e:SetViewDistRatio((d>0 and d or 1))
			end
		end,

		PlaySoundEvent = function(params)
			local sound, name = params.sound, params.name
			if sound and name then
				local entity=_L.System.GetEntityByName(name);
				if entity then
					-- thank fapp for this!
					local s = SOUND_EVENT;
					local v = SOUND_SEMANTIC_SOUNDSPOT;
					entity:PlaySoundEvent(sound,g_Vectors.v000,g_Vectors.v010,s,v);
				end
			end
		end
	});
	return Self
end
