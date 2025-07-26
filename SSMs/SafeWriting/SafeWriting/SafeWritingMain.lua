--Created 8.9.2012 as part of SSM SafeWriting project by 'Zi;'
SSMCMDS = SSCMDS or {} --Array for commands
SafeWriting =
    SafeWriting or
    {
        GlobalStorage = {},
        GlobalData = {},
        --FuncContainer={};
        --Schedule={};
        Version = "3.0.0",
        NumVersion = 300, --format abc.d, 2.1.6 = 216, 2.1.6.1 = 216.1
        GameVersion = "<unknown>",
        NanosuitModes = {
            ["speed"] = 0,
            ["strength"] = 1,
            ["cloak"] = 2,
            ["armor"] = 3,
            [0] = "speed",
            [1] = "strength",
            [2] = "cloak",
            [3] = "armor"
        },
        GeneratedCCommands = {},
        Bans = {},
        TempVersion = "1.9.10",
        ScriptsLoaded = false,
        VehiclesAllowed = true,
        Faster = false,
        TempEntity = nil,
        ChatEntity = nil,
        ProbableMap = nil,
        AsyncAwait = {},
        TellScript = false,
        SigQueue = {},
        MsgQueue = {}
    }
AntiCheat = AntiCheat or {}
Console = {}
Chat = {}
CHAT = 0
Msg = {}
MESSAGE = 1
Spawn = {}
Out = {}
SessionFlags = {
    All = 1,
    AdminsModerators = 2
}
Yes = true
No = false
Maybe = 2
PLAYER = "player"
PLAYERS = "players"
INT = "int"
DOUBLE = "double"
NUMBER = "number"
WORD = "word"
TEXT = "text"
TIME = "time"
--CF compat layer:
ADMIN = {AdminOnly = true}
MODERATOR = {AdminModOnly = true}
PREMIUM = {PremiumOnly = true}
STANDARD = {}
ALL = 0
CENTER = 0
CONSOLE = 1
ERROR = 2
INFO = 3
SERVER = 4
CHAT = 5
IMPORTANT = 6
SPAM = 7

TypeRev = {"console", "error", "info", "server", "chat", "error", "server"}
TypeRev[0] = "center"

CmdResponse = CHAT
teamNames = {"nk", "us", [0] = "all"}
teamIDs = {all = 0, nk = 1, us = 2}
--~CF compat layer
System.LogAlways("$1[SafeWriting] SafeWritingMain.lua loaded, root directory: " .. SAFEWRITING_ROOTDIR)

Script.LoadScript(SAFEWRITING_ROOTDIR:gsub("%\\", "/") ..  "/SafeWritingUtilities.lua", 1, 1)
Script.LoadScript(SAFEWRITING_ROOTDIR:gsub("%\\", "/") ..  "/SafeWritingUTF8.lua", 1, 1)
Script.LoadScript(SAFEWRITING_ROOTDIR:gsub("%\\", "/") ..  "/SafeWritingUpdater.lua", 1, 1)

--CPPAPI.LoadSSMScript("Files/SafeWritingUtilities.lua");
--CPPAPI.LoadSSMScript("Files/SafeWritingUpdater.lua");
SafeWriting.FuncContainer = SafeWriting.FuncContainer or FunctionsContainer:Create()
SafeWriting.FuncContainer:AddCategs(
    {
        "OnTimerTick",
        "PrepareAll",
        "CheckPlayer",
        "OnKill",
        "ProcessBulletDamage",
        "OnClientConnect",
        "OnClientDisconnect",
        "OnChangeSpectatorMode",
        "OnPlayerRevive",
        "OnActorHit",
        "OnItemPickedUp",
        "OnItemDropped",
        "EquipPlayer",
        "CanEnterVehicle",
        "OnRename",
        "OnLeaveVehicleSeat",
        "UpdatePlayer",
        "OnEnterBuyZone",
        "OnEnterServiceZone"
    }
)
Out = Queue:Create()
SafeWriting.Translator = SafeWriting.Translator or Translator:Create()
SafeWriting.JL1 = JL1Hash:Create(0x3f0)
SafeWriting.Schedule = SafeWriting.Schedule or ScheduleBase:New()
System.LogAlways("$6[SafeWriting] Successfuly initialized FunctionsContainer")

StartupTime = os.time()

function Time(future)
    return CPPAPI.GetTime(future or 0)
end

function EncryptFile(file, out)
    CPPAPI.FileEncrypt(file, out or file:gsub(".lua", ".bin"))
end

function GenerateUUID()
    local id = ""
    for i = 1, 16 do
        id = id .. string.char(Random(33, 115))
    end
    --printf("UUID: %s", id);
    return id
end

function BanChecker(player)
    if SafeWriting.Settings then
        --printf("BanChecker: player client: %s, profile: %s, gsprofile: %s", (player.isSfwCl and "sfwcl" or "else"), tostring(player.profile), tostring(player.gsprofile))
        if SafeWriting.Settings.BannedProviders then
            for i, v in pairs(SafeWriting.Settings.BannedProviders) do
                if player.host:find(v) then
                    KickPlayer(player, "your provider is banned here")
                    return
                end
            end
        end
        local pid = tonumber(player.profile)
        if player.isSfwCl and (SafeWriting.Settings.StrictProfilePolicy and pid >= 800000 and pid <= 1000000) then
            KickPlayer(player, "please, update your client")
            return
        end
    end

    if ReversedIPBans and ReversedIPBans[player.host] then
        KickPlayer(player, "you are black-listed")
        return
    end

    if IsPermabanned(player) then
        KickPlayer(player, "you are permabanned here")
        return
    end
end

function LinkToRules(name)
    local states = {"Reset", "PreGame", "InGame", "PostGame"}
    local rules = {"InstantAction", "TeamInstantAction", "PowerStruggle", "g_gameRules"}
    for j, w in pairs(rules) do
        for i, v in pairs(states) do
            if _G[w] and _G[w].Server[v] and g_gameRules.Server[name] then
                _G[w].Server[v][name] = g_gameRules.Server[name]
            end
        end
    end
end
function SfwLog(text)
    if SafeWriting.Faster then
        return
    end
    System.LogAlways("$6[SafeWriting] " .. text)
end
function printf(text, ...)
    if SafeWriting.Faster then
        return
    end
    System.LogAlways("$6[SafeWriting] " .. string.format(text, ...))
end
function print(...)
    if SafeWriting.Faster then
        return
    end
    local f = ""
    for i, v in ipairs({...}) do
        f = f .. tostring(v) .. "\t"
    end
    f = f:sub(1, f:len() - 1)
    System.LogAlways(f)
end
function SetError(err, quit)
    SafeWriting.LastError = err
    printf(err)
    if quit then
        System.Quit()
    end
end
function GetLastError()
    return SafeWriting.LastError
end
function SfwSetTempVersion(ver)
    SafeWriting.TempVersion = ver
end
function _pcall(func, ...)
    local status, err = pcall(func, ...)
    if not status then
        printf("%s", err)
    end
    return err
end
function MakePluginEvent(name, ...) --this is safer
    local funcs = SafeWriting.FuncContainer:GetFuncs(name)
    local ret = {}
    if (funcs) then
        for i, v in pairs(funcs) do
            local f, t = unpack(v)
            local r = {}
            if t then
                r = {_pcall(f, t, ...)}
            else
                r = {_pcall(f, ...)}
            end
            if #r > 0 then
                ret = r
            end
        end
    end
    if #ret > 0 then
        return unpack(ret)
    end
end
function PluginSafeCall(tbl, ...)
    local f, t = unpack(tbl)
    if t then
        return _pcall(f, t, ...)
    else
        return _pcall(f, ...)
    end
end
function SetGameVersion(...)
    local ver = table.concat({...}, " ")
    --SafeWriting.GameVersion=ver;	--do not use anymore!
end
function SetReloadFlag()
    SafeWriting.GlobalData.Reload = true
end
function UnsetReloadFlag()
    SafeWriting.GlobalData.Reload = false
end
function ReloadAllScripts()
    SetReloadFlag()
    System.ExecuteCommand("exec sfw.cfg")
    UnsetReloadFlag()
end
function UnloadAllScripts()
    _G["LoadedScripts"] = {}
    UP_TO_DATE = 0
    LOADED_SCRIPTS = 0
    TOTAL_SCRIPTS = 0
    ERROR_SCRIPTS = {}
end
function spamBegin()
    SPAMMSG = ""
end
function spam(fmt, ...)
    if not SPAMMSG then
        SPAMMSG = ""
    end
    local text = fmt
    if ... then
        text = string.format(text, ...)
    end
    SPAMMSG = SPAMMSG .. text .. "\n"
end
function spamEnd()
    System.LogAlways(SPAMMSG)
    SPAMMSG = ""
end
function include(file)
    local res = loadfile(file)
    if res then
        assert(res)()
        return true
    else
        return false
    end
end
function LoadScript(location, force)
    if not _G["LoadedScripts"] then
        _G["LoadedScripts"] = {}
    end
    if not SafeWriting["LoadedScriptContents"] then
        SafeWriting["LoadedScriptContents"] = {}
    end
    local gd = SafeWriting.GlobalData
    if (not gd.Reload) then
        gd.Reload = false
    end
    local isActual = false
    local f, err = io.open(location, "r")
    --if not force then
    if f then
        local con = f:read("*all")
        if not SafeWriting["LoadedScriptContents"][location] then
            SafeWriting["LoadedScriptContents"][location] = con
            isActual = false
        else
            if con == SafeWriting["LoadedScriptContents"][location] then
                isActual = true
            end
        end
        f:close()
    end
    --end
    if (not isActual) or force then
        Script.UnloadScript(location)
        local res = Script.LoadScript(location)
        if not res then
            if not ERROR_SCRIPTS then
                ERROR_SCRIPTS = {}
            end
            local res, err = loadfile(location)
            ERROR_SCRIPTS[#ERROR_SCRIPTS + 1] = {location, err}
        else
            if SafeWriting.TellScript then
                printf("Successfuly loaded %s", location)
            end
        end
        if not isActual then
            LOADED_SCRIPTS = (LOADED_SCRIPTS or 0) + 1
        end
    else
        UP_TO_DATE = (UP_TO_DATE or 0) + 1
    end
    TOTAL_SCRIPTS = (TOTAL_SCRIPTS or 0) + 1
    _G["LoadedScripts"][#_G["LoadedScripts"] + 1] = location
    return (not isActual)
end
function DetectGameVer()
    if IsDllLoaded100() then
        SafeWriting.GameVersion = 5767
        if IS121THOUGH then
            SafeWriting.GameVersion = 6156
            return 6156
        end
        return 5767
    else
        if IsDllLoaded() then
            SafeWriting.GameVersion = 6156
            return 6156
        end
    end
    local is121 = false
    local before = tonumber(System.GetCVar("g_pp_scale_price"))
    SafeWriting.PricesBefore = before
    System.SetCVar("g_pp_scale_price", before * 2)
    if (tonumber(System.GetCVar("g_pp_scale_price")) ~= before) then
        is121 = true
        System.SetCVar("g_pp_scale_price", before)
    end
    System.SetCVar("g_pp_scale_price", before)
    SafeWriting.GameVersion = is121 and 6156 or 4767
    return SafeWriting.GameVersion
end
function MapChanged()
    if not _G["KnownMap"] then
        _G["KnownMap"] = GetMapName()
        return false
    else
        local map = GetMapName()
        if _G["KnownMap"] ~= map then
            System.SetCVar("sv_map", map)
            _G["KnownMap"] = map
            return true
        end
    end
    return false
end
function InitializeFolders(...)
    DetectGameVer()
    folder = table.concat({...}, " ")
    folder = folder:gsub("%\\", "/")
    if folder:sub(-1) == "/" then
        folder = folder:sub(1, folder:len() - 1)
    end
    SafeWriting.__MainFolder = folder
    SafeWriting.MainFolder = folder .. "/"
    SafeWriting.SettingsFolder = folder .. "/Settings/"
    SafeWriting.ChatCommandsFolder = folder .. "/ChatCommands/"
    SafeWriting.AntiCheatFolder = folder .. "/AntiCheat/"
    SafeWriting.AdditionalScriptsFolder = folder .. "/AdditionalScripts/"
    SafeWriting.GlobalStorageFolder = folder .. "/Storage/"
    SafeWriting.ExtensionsFolder = folder .. "/Extensions/"
    if not SafeWriting.GlobalData.Reload then
        SfwLog("MainFolder: " .. SafeWriting.MainFolder)
        SfwLog("SettingsFolder: " .. SafeWriting.SettingsFolder)
        SfwLog("ExtensionsFolder: " .. SafeWriting.ExtensionsFolder)
        SfwLog("ChatCommandsFolder: " .. SafeWriting.ChatCommandsFolder)
        SfwLog("AntiCheatFolder: " .. SafeWriting.AntiCheatFolder)
        SfwLog("AdditionalScriptsFolder: " .. SafeWriting.AdditionalScriptsFolder)
        SfwLog("GlobalStorageFolder: " .. SafeWriting.GlobalStorageFolder)
    end
    SfwLog("Initializing scripts")
    InitializeAllScripts()
    SafeWriting.ScriptsLoaded = true
end
function InitializeAllScripts(hookFunc)
    UnloadAllScripts()
    SafeWriting.FuncContainer = nil
    SafeWriting.FuncContainer = FunctionsContainer:Create()
    SafeWriting.Settings = nil
    LoadScript(SafeWriting.SettingsFolder .. "Settings.lua", true)
    if IsDllLoaded() or IsDllLoaded100() then
        LoadExtensions(hookFunc)
    end
    LoadChatCommands(hookFunc)
    LoadAntiCheatScripts(hookFunc)
    LoadAdditionalScripts(hookFunc)
    printf(
        "Loaded %d new scripts (actual scripts: %d, error scripts: %d, total scripts: %d)",
        LOADED_SCRIPTS,
        TOTAL_SCRIPTS - LOADED_SCRIPTS,
        #ERROR_SCRIPTS,
        TOTAL_SCRIPTS
    )
    if #ERROR_SCRIPTS > 0 then
        for i, v in ipairs(ERROR_SCRIPTS) do
            printf("Error while loading %s", v[1])
            if v[2] then
                printf("    %s", v[2])
            end
        end
    end
    if (SafeWriting.Settings.AutoGenerateCCommands) then
        if (SSMCMDS) then
            for i, v in pairs(SSMCMDS) do
                if (not SafeWriting.GeneratedCCommands[i]) then
                    SafeWriting.GeneratedCCommands[i] = true
                    local description = v.info or "no description"
                    local name = string.format("sfw_%s", i)
                    local func = string.format('ExecuteCCommandAsChat("%s",%%line)', i)
                    System.AddCCommand(name, func, description)
                end
            end
        end
    end
    SafeWriting.JL1:SetSeed(SafeWriting.Settings.HashSeed or 0x3f0)
end
function ExecuteCCommandAsChat(command, text)
    local msg = string.format("!%s %s", command, text)
    local cmd = SSMCMDS[command]
    _pcall(cmd.func, cmd, SafeWriting.ChatEntity, msg)
end
function ChatEntityExists()
    if SafeWriting.ChatEntity then
        if not System.GetEntity(SafeWriting.ChatEntity.id) then
            return false
        end
        if not System.GetEntityByName(SafeWriting.Settings.ChatEntityName or "[SafeWriting]") then
            return false
        end
    else
        return false
    end
    return true
end
function AddFunc(f, n)
    SafeWriting.FuncContainer:AddFunc(f, n)
end
function LoadPlugin(pl)
    SafeWriting.FuncContainer:LoadPlugin(pl)
end
function PrepareAll()
    SafeWriting.Schedule.Events = {}
    local se = SafeWriting.Settings
    printf("Starting migrations...")
    BeginUpdates(GetTempVer())
    printf("Migrations complete")
    if not SafeWriting.ScriptsLoaded then
        printf("Loading sfw.cfg")
        System.ExecuteCommand("exec sfw.cfg")
    end
    if not SafeWriting.ScriptsLoaded then
        SetError("Failed to load files of whole mod", true)
        System.Quit()
    end
    if not se then
        SetError("Failed to load the settings", true)
        System.Quit()
    end
    if se.OptimizeSpeed then
        printf("Enabling optimizations")
    --System.SetCVar("log_verbosity",0);
    --System.SetCVar("log_fileverbosity",0);
    end
    Out:Limit(0x7FFFFFFF) --Out:Limit(se.ConsoleQueueLimit or (se.OutQueueLimit or 0xFFFFFFFFFFFFFFFF));
    if not ChatEntityExists() then
        printf("Spawning chat entity")
        CreateChatEntity(nil, nil, true)
    end
    CreateChatEntity("TempEntity", (se.TempEntityName or ">>"), true)
    math.randomseed(Random(0, 100000))
    math.random()
    math.random()
    math.random()
    math.random()
    math.random()
    if (IsDllLoaded() or IsDllLoaded100()) then
        if (se.UseRealTime) then
            local tspeed = 1 / 3600
            local t = os.date("*t")
            local tminutes = t.min
            local thours = t.hour
            local settime = 0
            if (tminutes > 0) then
                settime = thours + 1 / (60 / (tminutes))
            else
                settime = thours
            end
            ForceSet("e_time_of_day", tostring(settime))
            ForceSet("e_time_of_day_speed", tostring(tspeed))
        end
        if (se.UseCustomTime) then
            ForceSet("e_time_of_day", tostring(se.TimeOfDayStart))
            ForceSet("e_time_of_day_speed", tostring(se.TimeOfDaySpeed))
        end
        if (se.Gravitation ~= nil) then
            ForceSet("p_gravity_z", tostring(se.Gravitation))
        end
        for i, v in pairs(se.CustomSettingsAtLoad) do
            local params = fsplit(trim_from(v, {"= ", "  ", "="}), " ")
            --Log("Setting: "..params[0].." to "..params[1]);
            if (params[1] and params[2]) then
                ForceSet((params[1]), tostring(params[2]))
            end
        end
    end
    if (se.EnableCrews) then
        LoadCrews()
    end
    if (se.EnableStatistics) then
        LoadPlayerInfo()
    end
    if (se.ForbiddenAreaDisabled) then
        local ForbiddenAreas = System.GetEntitiesByClass("ForbiddenArea")
        if (ForbiddenAreas) then
            for i, v in pairs(ForbiddenAreas) do
                System.RemoveEntity(v.id)
            end
        end
    end
    if (se.UsePersistantScores) then
        SafeWriting.GlobalData.Scores = {}
    end
    if (IsDllLoaded()) then
        SfwLog("Successfuly loaded SafeWriting.dll")
    else
        if IsDllLoaded100() then
            SfwLog("Successfuly loaded multi-versal version of SafeWriting.dll")
        else
            SfwLog("Failed to load SafeWriting.dll")
        end
    end
    local hres = HookVehicles()
    --printf("Created OnHit hook in %d vehicles",hres);
    if (se.InitJailOnStart) then
        local ents = System.GetEntities()
        if (ents) then
            while true do
                local ent = ents[math.random(1, #ents)]
                if (ent) then
                    if (ent ~= SafeWriting.ChatEntity and ent.class ~= "Cloud") then
                        CreateJail(ent:GetPos(), 500)
                        break
                    end
                end
            end
        end
    end
    local bans = loadfile(SafeWriting.GlobalStorageFolder .. "Bans.lua")
    if bans then
        assert(bans)()
        printf("Successfuly loaded %d bans", #SafeWriting.Bans)
    end
    printf("Detected game version: %s", SafeWriting.GameVersion)
    if IsDllLoaded100() then
        System.ExecuteCommand("dohooks")
    end
    if IsDllLoaded() and g_gameRules and g_gameRules.CanAllSeeChat then
        if se.CanAllSeeChat then
            System.ExecuteCommand("sfw_seechatofall 1")
        else
            System.ExecuteCommand("sfw_seechatofall 0")
        end
    end
    if SafeWriting.Settings.RankedAPIAuth then
    --
    end
    if SafeWriting.Settings.ExplosiveRemovalTime then
        if CPPAPI and CPPAPI.SetExplosiveRemovalTime then
            CPPAPI.SetExplosiveRemovalTime(SafeWriting.Settings.ExplosiveRemovalTime)
        end
    end
    MakePluginEvent("PrepareAll")
    printf("Mod was successfuly loaded")
end
function VehHookFunc(self, hit)
    local explosion = hit.explosion or false
    local targetId = (explosion and hit.impact) and hit.impact_targetId or hit.targetId
    local hitType = (explosion and hit.type == "") and "explosion" or hit.type
    local direction = hit.dir
    local shooterId = 0
    if (hit.shooter) then
        if (hit.shooter.id) then
            shooterId = hit.shooter.id
        end
    else
        shooterId = targetId
    end
    if (hit.type ~= "fire") then
        if (hit.shooter ~= nil) then
            if (hit.shooter.id ~= nil) then
                g_gameRules.game:SendHitIndicator(hit.shooter.id, hit.explosion or false)
            end
        end
    end
    if (hit.type == "collision") then
        direction.x = -direction.x
        direction.y = -direction.y
        direction.z = -direction.z
    end
    if (g_localActorId and self:GetSeat(g_localActorId)) then
        HUD.DamageIndicator(hit.weaponId, shooterId, direction, true)
    end
    local dmg = ProcessDamageOfBullet(hit, true)
    self.vehicle:OnHit(targetId, shooterId, dmg, hit.pos, hit.radius, hitType, explosion)
    if (AI and hit.type ~= "collision") then
        if (hit.shooter) then
            g_SignalData.id = hit.shooterId
        else
            g_SignalData.id = NULL_ENTITY
        end
        g_SignalData.fValue = dmg
        if (hit.shooter and self.Properties.species ~= hit.shooter.Properties.species) then
            CopyVector(g_SignalData.point, hit.shooter:GetWorldPos())
            AI.Signal(SIGNALFILTER_SENDER, 0, "OnEnemyDamage", self.id, g_SignalData)
        elseif (self.Behaviour and self.Behaviour.OnFriendlyDamage ~= nil) then
            AI.Signal(SIGNALFILTER_SENDER, 0, "OnFriendlyDamage", self.id, g_SignalData)
        else
            AI.Signal(SIGNALFILTER_SENDER, 0, "OnDamage", self.id, g_SignalData)
        end
    end
    return self.vehicle:IsDestroyed()
end
function HookVehicles()
    local ents = System.GetEntities()
    local c = 0
    if ents then
        for i, v in pairs(ents) do
            if v.vehicle then
                if v.Server then
                    v.Server.OnHit = VehHookFunc
                    c = c + 1
                end
            end
        end
    end
    VehicleBase.Server.OnHit = VehHookFunc
    return c
end
function urlfmt(fmt, ...)
    local args = {}
    for i, v in pairs({...}) do
        if type(v) == "string" then
            args[i] =
                v:gsub(
                "[^a-zA-Z0-9]",
                function(c)
                    return string.format("%%%02X", string.byte(c))
                end
            )
        else
            args[i] = v
        end
    end
    return string.format(fmt, unpack(args))
end
function SafeWriting:OnTimerTick()
    local mapch = MapChanged()
    local se = self.Settings
    if not se then
        SetError("Failed to load Settings.lua", true)
    end
    local last = GARBAGELAST or (_time - 150)

    local ver = DetectGameVer()

    if _time - last >= 60 then
        local before = collectgarbage("count")
        collectgarbage("collect")
        local removed = before - collectgarbage("count")
        TOTALREMOVED = (TOTALREMOVED or 0) + removed
        TIMESREMOVED = (TIMESREMOVED or 0) + 1
        GARBAGELAST = _time
    end
    local master = se.AllowMasterServer
    if type(master) == "number" then
        if master == Yes or master == Maybe then
            master = true
        else
            master = false
        end
    end
    if se and master then
        self.LAST_MASTERSERVER = self.LAST_MASTERSERVER or (_time - 50)
        if (_time - self.LAST_MASTERSERVER >= 15) or mapch then
            local svn = System.GetCVar("sv_servername")
            local svp = System.GetCVar("sv_password")
            local port = System.GetCVar("sv_port")
            local maxpl = System.GetCVar("sv_maxplayers")
            local numpl = 0
            local pls = g_gameRules.game:GetPlayers()
            local map = GetMapName()
            local mapdl = se.Maps and (se.Maps[map] or "") or ""
            local rnk = tonumber(System.GetCVar("sv_ranked") or 0)
            if pls then
                numpl = count(pls)
            end
            local mapver = 0
            if mapdl then
                if type(mapdl) ~= "string" then
                    mapver = tonumber(mapdl[2])
                    mapdl = mapdl[1]
                else
                    local vr = string.match(mapdl, "^%[([0-9.]+)%]")
                    if vr then
                        mapver = tonumber(vr)
                        mapdl = string.match(mapdl, "^%[[0-9.]+%](.*)")
                    end
                end
                if mapdl and type(mapdl) == "string" then
                    if mapdl:sub(1, 7) == "http://" then
                        mapdl = mapdl:sub(8)
                    end
                end
            end
            if mapver ~= 0 then
                map = map .. "|" .. mapver
            end
            local desc = se.ServerDescription or ""
            local mappic = se.MapPictures and (se.MapPictures[GetMapName()] or "") or ""
            local local_ip = se.LocalIP and se.LocalIP or (CPPAPI and CPPAPI.GetLocalIP() or "localhost")
            --printf("local ip: %s",local_ip);
            local pak = se.PAK or ""
            if not TOLD_EXISTENTION and (not MASTER_COOKIE) then
                local page =
                    urlfmt(
                    "/api/reg.php?port=%d&maxpl=%d&numpl=%d&name=%s&pass=%s&map=%s&timel=%d&mapdl=%s&ver=%d&ranked=%d&desc=%s&mappic=%s&local=%s&pak=%s",
                    port,
                    maxpl,
                    numpl,
                    svn,
                    svp,
                    map,
                    g_gameRules.game:GetRemainingGameTime(),
                    mapdl,
                    ver,
                    rnk,
                    desc,
                    mappic,
                    local_ip,
                    pak
                )
                AsyncConnectHTTP(
                    se.MasterHost or "crymp.org",
                    page,
                    se.ForceGET and "GET" or "POST",
                    443,
                    true,
                    15,
                    function(c)
                        local content, hdr, error = ParseHTTP(c)
                        if not error then
                            MASTER_COOKIE = string.match(content, "<<Cookie>>(.-)<<")
                            if MASTER_COOKIE and MASTER_COOKIE:len() > 30 then
                                TOLD_EXISTENTION = true
                                printf("Cookie: %s", MASTER_COOKIE)
                            else
                                MASTER_COOKIE = nil
                            end
                        else
                            printf("Connection error: %s, retrying", error)
                            self.LAST_MASTERSERVER = _time - 120
                        end
                    end
                )
            else
                local plstring = ""
                if pls then
                    for i, v in pairs(pls) do
                        if v and v.GetName then
                            plstring = plstring .. "@"
                            local name = v:GetName()
                            local k, d = GetPlayerScore(v)
                            local rank = 0
                            local team = "0"
                            if g_gameRules.class == "PowerStruggle" and g_gameRules.GetPlayerRank and v and v.id then
                                rank = g_gameRules:GetPlayerRank(v.id) or 1
                                team = g_gameRules.game:GetTeam(v.id)
                            end
                            local id = v.profile
                            local str = name .. "%" .. rank .. "%" .. k .. "%" .. d .. "%" .. id .. "%" .. team
                            plstring = plstring .. str
                        end
                    end
                end
                map = map or "unknown"
                local page =
                    urlfmt(
                    "/api/up.php?port=%d&numpl=%d&name=%s&pass=%s&cookie=%s&map=%s&timel=%d&mapdl=%s&ver=%d&ranked=%d&players=%s&desc=%s&mappic=%s&local=%s&pak=%s",
                    port,
                    numpl,
                    svn,
                    svp,
                    MASTER_COOKIE,
                    map,
                    g_gameRules.game:GetRemainingGameTime(),
                    mapdl,
                    ver,
                    rnk,
                    plstring,
                    desc,
                    mappic,
                    local_ip,
                    pak
                )
                AsyncConnectHTTP(
                    se.MasterHost or "crymp.org",
                    page,
                    se.ForceGET and "GET" or "POST",
                    443,
                    true,
                    15,
                    function(c)
                        local content, hdr, error = ParseHTTP(c)
                        if error then
                            printf("Connection error: %s, retrying", error)
                            self.LAST_MASTERSERVER = _time - 120
                        end
                    end
                )
            end
            self.LAST_MASTERSERVER = _time
        end
    end
    local gd = self.GlobalData
    local gs = self.GlobalStorage
    local t = _time
    if (not IsDllLoaded() and not IsDllLoaded100()) then
        ReadServerLog()
    end
    if not ChatEntityExists() then
        CreateChatEntity(nil, nil, nil)
    end
    if (se.SpamMessages) then
        local messagecount = #se.SpammyMessages
        if (gd.LastMessageTime == nil) then
            gd.LastMessageTime = t
        end
        if (gd.NextMessageID == nil) then
            gd.NextMessageID = 1
        end
        if (tonumber(t - gd.LastMessageTime) > se.SpammyInterval) then
            gd.LastMessageTime = t
            Msg:SendToAll(se.SpammyMessages[gd.NextMessageID], se.SpammyMessagesType)
            gd.NextMessageID = gd.NextMessageID + 1
            if (gd.NextMessageID > messagecount) then
                gd.NextMessageID = 1
            end
        end
    end
    if (gs.MapVotingInProgress and gs.StartedMapVoting) then
        if ((t - gs.MapVotingStartTime) > se.MapVotingTimeout) then
            gs.MapVotingInProgress = false
            gs.StartedMapVoting = false
            local votedfor = 0
            local _max = 0
            local maxmap = nil
            local equal = 0
            for n, u in pairs(gs.MapVotes) do
                votedfor = 0
                for i, v in pairs(u) do
                    if (v == true) then
                        votedfor = votedfor + 1
                    end
                end
                if (votedfor > _max) then
                    _max = votedfor
                    maxmap = n
                elseif (votedfor == _max) then
                    equal = votedfor
                end
            end
            if (_max == equal) then
                if (equal == 0) then
                    Chat:SendToAll(nil, "Map voting was not successful, noone was voting")
                else
                    Chat:SendToAll(nil, "Map voting was not successful, two maps had same count of votes")
                end
            else
                gs.ForceNextMap = true
                local nextmap = nil
                for i, v in pairs(se.GameModes) do
                    for k, j in pairs(v) do
                        for n, u in pairs(j) do
                            if (n == maxmap) then
                                nextmap = u
                            end
                        end
                    end
                end
                gs.NextMap = nextmap
                Chat:SendToAll(
                    nil,
                    SpecialFormat("Map voting was successful, next map: %s (%s votes)", maxmap, tostring(_max))
                )
            end
        end
    end
    if (gd.__VoteInProgress) then
        if (_time - gd.__VoteStart > gd.__VoteTimeout) then
            local y = count(gd.__YesVotes)
            local n = count(gd.__NoVotes)
            if gd.__VoteCallbackYes then
                gd.__VoteCallbackYes(y, y > n)
            end
            if gd.__VoteCallbackNo then
                gd.__VoteCallbackNo(n, y <= n)
            end
            if gd.__VoteCallbackYesNo then
                gd.__VoteCallbackYesNo(y, n)
            end
            gd.__VoteInProgress = false
        end
    end
    if (se.EnableStatistics and se.StatisticsAutoSave) then
        if (not gd.LastAutoSave) then
            gd.LastAutoSave = _time
        end
        if ((_time - gd.LastAutoSave) > ((se.AutoSaveInterval or 10) * 60)) then
            SaveAllPlayersInfo()
            gd.LastAutoSave = _time
        end
    end
    self.Schedule:Update()
    MakePluginEvent("OnTimerTick", frameTime)
end
function OnUpdate()
    for i, v in pairs(SafeWriting.AsyncAwait) do
        if v ~= nil then
            local idx = v[1]
            local func = v[2]
            local ret = _G["AsyncRet" .. idx]
            if ret ~= nil then
                --printf("Async got return for "..idx..": "..ret);
                pcall(func, ret)
                SafeWriting.AsyncAwait[i] = nil
                _G["AsyncRet" .. idx] = nil
            end
        end
    end
    return 1
end
function LoadExtensions(hf)
    local files = System.ScanDirectory(SafeWriting.ExtensionsFolder, 0, 1)
    if (files) then
        for i, file in pairs(files) do
            if (file:sub(-3) == "lua" or file:sub(-4) == "sfwc") then
                LoadScript(SafeWriting.ExtensionsFolder .. file, true)
                if hf then
                    hf(SafeWriting.ExtensionsFolder .. file)
                end
            elseif file:sub(-3) == "bin" then
                CPPAPI.LoadScript(SafeWriting.ExtensionsFolder .. file)
                if hf then
                    hf(SafeWriting.ExtensionsFolder .. file)
                end
            end
        end
    end
end
function LoadChatCommands(hf)
    local files = System.ScanDirectory(SafeWriting.ChatCommandsFolder, 0, 1)
    if (files) then
        for i, file in pairs(files) do
            if (file:sub(-3) == "lua" or file:sub(-4) == "sfwc") then
                LoadScript(SafeWriting.ChatCommandsFolder .. file, true)
                if hf then
                    hf(SafeWriting.ChatCommandsFolder .. file)
                end
            elseif file:sub(-3) == "bin" then
                CPPAPI.LoadScript(SafeWriting.ChatCommandsFolder .. file)
                if hf then
                    hf(SafeWriting.ChatCommandsFolder .. file)
                end
            end
        end
    end
end
function LoadAntiCheatScripts(hf)
    local files = System.ScanDirectory(SafeWriting.AntiCheatFolder, 0, 1)
    if (files) then
        for i, file in pairs(files) do
            if (file:sub(-3) == "lua" or file:sub(-4) == "sfwc") then
                LoadScript(SafeWriting.AntiCheatFolder .. file, true)
                if hf then
                    hf(SafeWriting.AntiCheatFolder .. file)
                end
            elseif file:sub(-3) == "bin" then
                CPPAPI.LoadScript(SafeWriting.AntiCheatFolder .. file)
                if hf then
                    hf(SafeWriting.AntiCheatFolder .. file)
                end
            end
        end
    end
end
function LoadAdditionalScripts(hf)
    local files = System.ScanDirectory(SafeWriting.AdditionalScriptsFolder, 0, 1)
    if (files) then
        for i, file in pairs(files) do
            if (file:sub(-3) == "lua" or file:sub(-4) == "sfwc") then
                LoadScript(SafeWriting.AdditionalScriptsFolder .. file, true)
                if hf then
                    hf(SafeWriting.AdditionalScriptsFolder .. file)
                end
            elseif file:sub(-3) == "bin" then
                CPPAPI.LoadScript(SafeWriting.AdditionalScriptsFolder .. file)
                if hf then
                    hf(SafeWriting.AdditionalScriptsFolder .. file)
                end
            end
        end
    end
end
function IsAdmin(player)
    if (SafeWriting.Settings.Admins[player.profile]) then
        if (player.IsAdminLogged) then
            return true
        end
    end
    return false
end
function IsModerator(player)
    if (SafeWriting.Settings.Moderators[player.profile]) then
        if (player.IsModeratorLogged) then
            return true
        end
    end
    return false
end
function IsAdminOrMod(player)
    if (IsAdmin(player) or IsModerator(player)) then
        return true
    end
    return false
end
function IsPremium(player)
    if (SafeWriting.Settings.Premiums[player.profile]) then
        if (player.IsPremiumLogged) then
            return true
        end
    end
    return false
end
function ScanForChatCommand(player, msg)
    if (SSMCMDS ~= nil) then
        local se = SafeWriting.Settings
        local ce = se.CommandsExtension or {"!"}
        if type(ce) ~= "table" then
            ce = {tostring(ce)}
        end
        local cmdname = string.match(msg, "^[" .. table.concat(ce) .. "](%w+).*$")
        if player.isTempEntity then
            local cmdname = string.match(msg, "^.-%] [" .. table.concat(ce) .. "](%w+).*$")
            if cmdname then
                return nil
            end
        end
        if (cmdname ~= nil) then
            cmdname = string.lower(cmdname)
            if (SSMCMDS[cmdname] ~= nil) then
                if (SSMCMDS[cmdname].IsDisabled) then
                    Chat:SendToTarget(nil, player, "[[COMMAND_BLOCKED]]")
                else
                    if (SSMCMDS[cmdname].AdminOnly == true) then
                        if (IsAdmin(player)) then
                            _pcall(ExecuteCommand, cmdname, player, msg)
                        else
                            Chat:SendToTarget(nil, player, "[[THIS_COMMAND_IS]] [[ADMIN_ONLY]]")
                        end
                    elseif (SSMCMDS[cmdname].AdminModOnly == true) then
                        if (IsAdminOrMod(player)) then
                            _pcall(ExecuteCommand, cmdname, player, msg)
                        else
                            Chat:SendToTarget(nil, player, "[[THIS_COMMAND_IS]] [[ADMIN_MOD_ONLY]]")
                        end
                    elseif (SSMCMDS[cmdname].ModOnly == true) then
                        if (IsModerator(player)) then
                            _pcall(ExecuteCommand, cmdname, player, msg)
                        else
                            Chat:SendToTarget(nil, player, "[[THIS_COMMAND_IS]] [[MOD_ONLY]]")
                        end
                    elseif (SSMCMDS[cmdname].PremiumOnly == true) then
                        if (IsPremium(player)) then
                            _pcall(ExecuteCommand, cmdname, player, msg)
                        else
                            Chat:SendToTarget(nil, player, "[[THIS_COMMAND_IS]] [[PREMIUM_ONLY]]")
                        end
                    else
                        _pcall(ExecuteCommand, cmdname, player, msg)
                    end
                end
            else
                Chat:SendToTarget(nil, player, "[[COMMAND_DOESNT_EXIST]]", cmdname)
                if se.UseDidYouMeanFeature then
                    local fcmd = nil
                    local maxi = 0
                    for i, v in pairs(SSMCMDS) do
                        if HavePrivileges(player, i) then
                            local r = similartext(i, cmdname)
                            if r > maxi then
                                fcmd = i
                                maxi = r
                            end
                        end
                    end
                    local thr = se.DidYouMeanLimit or 30
                    thr = math.min(thr, 99)
                    if maxi > thr then
                        local tr = __qt(player.lang, R.DID_YOU_MEAN)
                        if tr and tr == "" then
                            tr = "Did you mean ,!%s'?"
                        end
                        Chat:SendToTarget(nil, player, tr, fcmd)
                    end
                end
            end
            return true
        end
    end
    return false
end
function ExecuteCommand(cmdname, player, msg)
    local se = SafeWriting.Settings
    local flags = se.RequireFlags
    local cmd = SSMCMDS[cmdname]
    local staff = false
    if (se.UseCommandsSession and g_gameRules.class == "PowerStruggle") then
        staff = cmd.AdminOnly or cmd.ModeratorOnly or cmd.AdminModOnly
        if ((staff) and se.CommandsSessionFlags == SessionFlags.AdminsModerators) then
            if ((_time - (player.LastSession or (se.SessionExpiry + 1))) > se.SessionExpiry) then
                local msg = "    Please, update your session by writing buy %s to console    "
                local pass = "Session"
                if (se.UseAuthentificationPassword) then
                    if (IsAdmin(player)) then
                        pass = se.AdminAuthPassword .. "Session"
                    elseif (IsModerator(player)) then
                        pass = se.ModeratorAuthPassword .. "Session"
                    end
                end
                if (se.UseSessionSalt) then
                    pass = SafeWriting.JL1:Hash(pass .. player.profile)
                end
                msg = string.format(msg, pass)
                Msg:SendToTarget(player, msg)
                return
            end
        else
            if
                ((_time - (player.LastSession or (se.SessionExpiry + 1))) > se.SessionExpiry and
                    se.CommandsSessionFlags == SessionFlags.All)
             then
                local pass = "Session"
                if (se.UseSessionSalt) then
                    pass = SafeWriting.JL1:Hash(pass .. player.profile)
                end
                Msg:SendToTarget(player, "    Please, update your session by writing buy %s to console    ", pass)
                return
            end
        end
        if (se.ImmediateExpire) then
            player.LastSession = -(se.SessionExpiry + 1)
        end
    end
    if (cmd.DotFunc == true) then
        cmd.func(player, msg)
    else
        cmd:func(player, msg)
    end
end
function DisableChatCommand(cmdname)
    if (SSMCMDS[cmdname]) then
        SSMCMDS[cmdname].IsDisabled = true
    end
end
function EnableChatCommand(cmdname)
    if (SSMCMDS[cmdname]) then
        SSMCMDS[cmdname].IsDisabled = false
    end
end
function FloodCheck(player, flood, _max, timeo)
    local flfield, cfield = "___last" .. flood, "___checks" .. flood
    timeo = timeo or 0.125
    player[flfield] = player[flfield] or (_time - 1)
    if _time - player[flfield] <= timeo then
        player[cfield] = (player[cfield] or 0) + 1
        if player[cfield] > _max then
            AntiCheat:DealWithPlayer(player, flood .. " flooding")
        end
    else
        player[cfield] = 0
    end
    player[flfield] = _time
end
function CreateAppCall(folder, app, ...)
    if (not SafeWriting.Settings.EnableSafeWritingExe) then
        return ""
    end
    math.randomseed(Random(0, 100000))
    local out = "tmp_" .. app .. "_" .. (_time % 127 + math.random())
    out = SafeWriting.GlobalStorageFolder .. out:gsub("%.", "_") .. ".tmp"
    out = out:gsub("%/", "\\")
    app = folder .. app
    app = app:gsub("%/", "\\")
    local cmd = app .. " " .. requestencode(table.concat({...}, " ")) .. " > " .. out
    os.execute(cmd)
    local f, err = io.open(out, "r")
    if f then
        local c = f:read("*all")
        f:close()
        os.remove(out)
        return c
    else
        printf(
            "Error: Unable to start-up %s, probably because you have Server folder in protected folder like Program Files",
            app
        )
        printf("Error: Setting SafeWriting.Settings.EnableSafeWritingExe to false to prevent next errors")
        SafeWriting.Settings.EnableSafeWritingExe = false
        return ""
    end
    return nil
end
function SafeWritingCall(method, ...)
    return CreateAppCall(SafeWriting.MainFolder, "SSMSafeWriting.exe", method, ...)
end
function LoadExtension(name)
    local path = SafeWriting.ExtensionsFolder
    local bin64 = DLLAPI.Is64Bit()
    if name:sub(-4) ~= ".dll" then
        name = name .. ".dll"
    end
    if not bin64 then
        path = path .. "Bin32/"
    else
        path = path .. "Bin64/"
    end
    path = path .. name
    loaddll(path)
end
function loaddll(path)
    local loaded = false
    for i, v in pairs(DLLAPI) do
        if type(v) == "string" and v == path then
            loaded = true
        end
    end
    if not loaded then
        path = path:gsub("[/]", "\\")
        local res = DLLAPI.LoadDLL(path)
        return res
    end
end
function ImportFunc(name)
    _G[name] = function(...)
        if ... then
            for i, v in ipairs({...}) do
                _G["__CPP__ARG__" .. i] = v
            end
        end
        local exist = DLLAPI.RunFunc(name)
        if not exist then
            System.LogAlways("$6[SafeWritingAPI] Function " .. name .. " does not exist!")
            return nil
        end
        local ret = {}
        local cnt = _G["__CPP__CNT__"] or 0
        if cnt > 0 then
            for i = 0, cnt - 1 do
                ret[#ret + 1] = _G["__CPP__RET__" .. i]
            end
        end
        return unpack(ret)
    end
end
importfunc = ImportFunc
function IsCommandUsableForPlayer(cmdname, player, timeout, sendtext, health)
    if (player) then
        if (player.CommandsInfo == nil) then
            player.CommandsInfo = {}
        end
        timeout = tonumber(timeout)
        cmdname = tostring(cmdname)
        local lastuse = 0
        if (player.CommandsInfo[cmdname] ~= nil) then
            lastuse = player.CommandsInfo[cmdname]
        end
        lastuse = tonumber(lastuse)
        if health and player.actor then
            local tgth = player.actor:GetHealth()
            if tgth <= health then
                if sendtext then
                    Chat:SendToTarget(nil, player, "You can't use this command right now, you are being attacked")
                end
                return false
            end
        end
        if (_time - lastuse < timeout and lastuse ~= 0) then
            if (sendtext == true) then
                Chat:SendToTarget(
                    nil,
                    player,
                    "[[USABLE_ONLY_N_SECONDS]]",
                    cmdname,
                    tostring(timeout),
                    tostring(math.ceil(timeout - (_time - lastuse)))
                )
            end
            return false, (timeout - (_time - lastuse))
        else
            player.CommandsInfo[cmdname] = _time
        end
        return true
    end
end
function AttachAttachments(wpnid, attachments, player)
    local wpn = System.GetEntity(wpnid)
    if (attachments) then
        for i, v in ipairs(attachments) do
            if (wpn.weapon:SupportsAccessory(v)) then
                if player then
                    ItemSystem.GiveItem(v, player.id, false)
                end
                wpn.weapon:AttachAccessory(v, true, true)
            end
        end
    end
end
function TeleportPlayer(player, pos, angle)
    player.portalTime = _time
    if (player:IsOnVehicle()) then
        g_gameRules.game:SetInvulnerability(player.id, true, 2)
        local veh = System.GetEntity(player.actor:GetLinkedVehicleId())
        veh.vehicle:ExitVehicle(player.id)
        Script.SetTimer(
            750,
            function()
                player.portalTime = _time
                g_gameRules.game:SetInvulnerability(player.id, true, 2)
                g_gameRules.game:MovePlayer(player.id, pos, angle or player:GetWorldAngles())
            end
        )
    else
        g_gameRules.game:SetInvulnerability(player.id, true, 2)
        g_gameRules.game:MovePlayer(player.id, pos, angle or player:GetWorldAngles())
    end
end
function TeleportPlayerToXYZ(player, nx, ny, nz, angle)
    player.portalTime = _time
    local pos = {
        x = nx,
        y = ny,
        z = nz
    }
    if (player:IsOnVehicle()) then
        g_gameRules.game:SetInvulnerability(player.id, true, 2)
        local veh = System.GetEntity(player.actor:GetLinkedVehicleId())
        veh.vehicle:ExitVehicle(player.id)
        Script.SetTimer(
            750,
            function()
                player.portalTime = _time
                g_gameRules.game:SetInvulnerability(player.id, true, 2)
                g_gameRules.game:MovePlayer(player.id, pos, angle or player:GetWorldAngles())
            end
        )
    else
        g_gameRules.game:SetInvulnerability(player.id, true, 2)
        g_gameRules.game:MovePlayer(player.id, pos, angle or player:GetWorldAngles())
    end
end
function canSee(player, target, bone, thr)
    thr = thr or 2
    bone = bone or "Bip01 head"
    local pos = player:GetBonePos(bone)
    local dir = player:GetBoneDir(bone)
    local skip = player.id
    local hitData = {}
    local hits =
        Physics.RayWorldIntersection(
        pos,
        dir,
        CalcDistance3D(pos, target:GetWorldPos()) + 10,
        ent_all,
        skip,
        nil,
        hitData
    )
    local entsBefore = 0
    if (hits > 0) then
        for i, v in pairs(hitData) do
            if v.entity then
                if v.entity.class == "Player" and v.entity.id == target.id then
                    if entsBefore < thr then
                        return true
                    else
                        return false
                    end
                end
            end
            entsBefore = entsBefore + 1
        end
    else
        return false
    end
end
function RenamePlayer(player, newname, ignore)
    newname = newname:gsub(" ", "-")
    newname = newname:gsub("%%", "_")
    if (SafeWriting.Settings.UseClearNames) then
        newname = ClearString(newname, true)
    end
    if not ignore then
        g_gameRules.game:RenamePlayer(player.id, newname)
    else
        if not IsDllLoaded() and not IsDllLoaded100() then
            player.exceptRename = (player.exceptRename or 0) + 1
        end
        g_gameRules.game:RenamePlayer(player.id, newname)
    end
end
function GetPlayerPP(player)
    if not g_gameRules.GetPlayerPP then
        return 0
    end
    return g_gameRules:GetPlayerPP(player.id)
end
function GetPlayerCP(player)
    if not g_gameRules.GetPlayerCP then
        return 0
    end
    return g_gameRules:GetPlayerCP(player.id)
end
function AwardPlayer(player, pp, cp)
    if g_gameRules.class == "PowerStruggle" then
        if (pp) then
            pp = tonumber(pp)
            if pp > 0 then
                pp = pp / tonumber(System.GetCVar("g_pp_scale_income"))
            end
            g_gameRules:AwardPPCount(player.id, math.floor(pp))
        end
        if (cp) then
            cp = tonumber(cp)
            g_gameRules:AwardCPCount(player.id, cp)
        end
    end
end
function HasEnoughPP(player, value)
    if g_gameRules.class ~= "PowerStruggle" then
        return true
    end
    return GetPlayerPP(player) >= value
end
function GivePoints(player, pp, cp)
    AwardPlayer(player, pp, cp)
end
function GiveItem(player, item, attachments, unforce)
    local realClass = item
    if KnownClasses and KnownClasses[item] then
        realClass = KnownClasses[item]
    end
    local force = true
    if unforce ~= nil then
        force = unforce
    end
    local wpnid = ItemSystem.GiveItem(realClass, player.id, force)
    if (wpnid) then
        if (attachments) then
            AttachAttachments(wpnid, attachments, player)
        end
        local wpn = System.GetEntity(wpnid)
        if wpn then
            wpn["is" .. item] = true
        end
    end
    return wpnid
end
function GiveAmmo(player, name, count)
    if (not player) then
        return
    end
    if (not name) then
        local wpnId = player.inventory:GetCurrentItemId()
        if (wpnId) then
            local wpn = System.GetEntity(wpnId)
            if (wpn) then
                name = wpn.class or ""
            end
        end
        if (not name) then
            return
        end
    end
    if (g_gameRules.class == "PowerStruggle") then
        g_gameRules:DoBuyAmmo(player.id, name, 0, true)
    else
        local amount = player.ammoCapacity[name] or 0
        player.actor:SetInventoryAmmo(name, amount)
    end
end
function SetupPlayerScore(player)
    if (not SafeWriting.GlobalData.Scores) then
        SafeWriting.GlobalData.Scores = {}
        return
    end
    if (not SafeWriting.GlobalData.Scores[player.profile]) then
        return
    end
    local se = SafeWriting.GlobalData.Scores[player.profile]
    local kills = se.kills or 0
    local deaths = se.deaths or 0
    local headshots = se.headshots or 0
    local pp = se.pp or 0
    local cp = se.cp or 0
    local rank = se.rank or 1
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_KILLS_KEY, kills)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_DEATHS_KEY, deaths)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_HEADSHOTS_KEY, headshots)
    if (g_gameRules.class == "PowerStruggle") then
        g_gameRules:SetPlayerCP(player.id, cp)
        g_gameRules:SetPlayerPP(player.id, pp)
        g_gameRules:SetPlayerRank(player.id, rank)
    end
end
function SavePlayerPersistantScore(player)
    if (not player.profile) then
        return
    end
    if (not SafeWriting.GlobalData.Scores) then
        SafeWriting.GlobalData.Scores = {}
    end
    SafeWriting.GlobalData.Scores[player.profile] = {}
    local se = SafeWriting.GlobalData.Scores[player.profile]
    se.kills = g_gameRules.game:GetSynchedEntityValue(player.id, g_gameRules.SCORE_KILLS_KEY) or 0
    se.deaths = g_gameRules.game:GetSynchedEntityValue(player.id, g_gameRules.SCORE_DEATHS_KEY) or 0
    se.headshots = g_gameRules.game:GetSynchedEntityValue(player.id, g_gameRules.SCORE_HEADSHOTS_KEY) or 0
    if (g_gameRules.class == "PowerStruggle") then
        se.cp = g_gameRules:GetPlayerCP(player.id)
        se.pp = g_gameRules:GetPlayerPP(player.id)
        se.rank = g_gameRules:GetPlayerRank(player.id)
    end
end
function SavePersistantScore(player)
    local kills = se.kills or 0
    local deaths = se.deaths or 0
    local headshots = se.headshots or 0
    local pp = se.pp or 0
    local cp = se.cp or 0
    local rank = se.rank or 1
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_KILLS_KEY, kills)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_DEATHS_KEY, deaths)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_HEADSHOTS_KEY, headshots)
    if (g_gameRules.class == "PowerStruggle") then
        g_gameRules:SetPlayerCP(player.id, cp)
        g_gameRules:SetPlayerPP(player.id, pp)
        g_gameRules:SetPlayerRank(player.id, rank)
    end
end
function IsDllLoaded()
    return CPPAPI ~= nil
end
function IsDllLoaded100()
    return IS100DLLLOADED
end
function CreateJail(bPos, bheight, part)
    local ents = 1
    part = part or 24
    Script.SetTimer(
        1,
        function()
            for height = 0, 10, 10 do
                for mult = 2, 2, 2 do
                    --local mult=1;
                    for i = 0, 360, part do
                        local pos = {}
                        MergeTables(pos, bPos)
                        pos.z = pos.z + (height) + bheight
                        pos.x = pos.x + (math.sin(i) * mult)
                        pos.y = pos.y + (math.cos(i) * mult)
                        local dir = {x = 0, y = 0, z = 0}
                        dir.x = math.cos(i)
                        dir.y = -math.sin(i)
                        local params = {
                            name = "JailPart_" .. i .. "_" .. mult .. "_" .. height,
                            class = "Flag",
                            position = pos,
                            orientation = dir
                        }
                        System.SpawnEntity(params)
                        ents = ents + 1
                    end
                end
            end
            local pos = {}
            MergeTables(pos, bPos)
            pos.z = pos.z + 10 + bheight
            --pos.x=pos.x+1.5;
            --pos.y=pos.y+1.5;
            local params = {
                name = "JailTrolley",
                position = pos,
                orientation = {x = 0, y = 0, z = 0},
                class = "US_trolley"
            }
            local ent = System.SpawnEntity(params)
            ent:SetTriggerBBox({-10, -10, -10}, {10, 10, 10})
            ent.OnEnterArea = (function(self, iPlayer, areaId)
                if (iPlayer.isScriptJailed) then
                    Chat:SendToTarget(nil, iPlayer, __qt(iPlayer.lang, R.YOU_ARE_JAILED) .. "!")
                end
            end)
            ent.OnLeaveArea = (function(self, iPlayer, areaId)
                if (iPlayer.isScriptJailed and (not iPlayer:IsDead())) then
                    local npos = self:GetPos()
                    npos.z = npos.z - 9
                    TeleportPlayer(iPlayer, npos)
                end
            end)
            ents = ents + 1
        end
    )
end
function MakeSimpleVote(timeout, callback1, callback2, callback3)
    local gd = SafeWriting.GlobalData
    if (gd.__VoteInProgress) then
        return false, "There is already voting in progress, please wait"
    end
    gd.__VoteTimeout = timeout
    gd.__VoteCallbackYes = callback1
    gd.__VoteCallbackNo = callback2
    gd.__VoteCallbackYesNo = callback3
    gd.__VoteInProgress = true
    gd.__VoteStart = _time
    gd.__YesVotes = {}
    gd.__NoVotes = {}
    return true, "Vote has been successfuly initialized"
end
function AsyncCreateId(id, func)
    if id then
        SafeWriting.AsyncAwait[#SafeWriting.AsyncAwait + 1] = {id, func}
    else
        printf("AsyncCreateId fail")
    end
end
function AsyncCreate(callback, func, ...)
    local id = func(...)
    if id then
        SafeWriting.AsyncAwait[#SafeWriting.AsyncAwait + 1] = {id, callback}
    end
end
function ForceSet(cvar, val)
    if IsDllLoaded() or IsDllLoaded100() then
        CPPAPI.FSetCVar(cvar, tostring(val))
    else
        System.SetCVar(cvar, val)
    end
end
function ParseHTTP(tmp_ret)
    if not tmp_ret then
        return nil, nil, "No content"
    end
    local ret = tostring(tmp_ret or "")
    local header_end = string.find(ret, "\r\n\r\n", nil, true)
    local content, header = "", ""
    local err = false
    if ret:find("Error:", nil, true) == 1 then
        err = ret
    end
    if ret:find("Unexpected error occured:", nil, true) == 1 then
        err = ret
        System.LogAlways("$4[SafeWriting::ConnectWebsite] " .. err)
    end
    if header_end then
        header = string.sub(ret, 1, header_end)
        content = string.sub(ret, header_end + 4)
        local mtch = string.match(header, "Content.Length[:] (.-)\r\n")
        if mtch then
            content = string.sub(ret, ret:len() - (tonumber(mtch) or 0) + 1)
        else
            content = content:gsub("^([^a-zA-Z0-9<]+)", "")
        end
    else
        content = tmp_ret
    end

    if content:len() >= 3 then
        if content:byte(1) == 0xef and content:byte(2) == 0xbb and content:byte(3) == 0xbf then
            content = content:sub(4)
        end
    end

    return content, header, err
end
function ConnectHTTP(host, url, method, port, http11, timeout, alive)
    local tmp_ret = ""
    timeout = timeout or 15
    if (IsDllLoaded() or IsDllLoaded100()) and CPPAPI then
        method = method or "GET"
        method = method:upper()
        tmp_ret =
            CPPAPI.ConnectWebsite(
            host,
            url,
            port or 80,
            http11 or false,
            timeout,
            method == "GET" and true or false,
            alive or false
        )
    else
        local reqtype = "connecthttp10"
        if http11 then
            reqtype = "connecthttp11"
        end
        tmp_ret = SafeWritingCall(reqtype, method or "GET", host, port or 80, timeout, url)
    end
    local ret = tmp_ret or ""
    local header_end = string.find(ret, "\r\n\r\n", nil, true)
    local content, header = "", ""
    local err = false
    if tmp_ret:find("Error:", nil, true) == 1 then
        err = tmp_ret
    end
    content, header, err = ParseHTTP(tmp_ret)
    return tmp_ret, content, header, err
end
function AsyncConnectHTTP(host, url, method, port, http11, timeout, func)
    --printf("Connecting %s",host);
    --printf("URL: %s",(url:gsub("[%%]","#")) or "nil");
    method = method or "GET"
    method = method:upper()
    if CPPAPI ~= nil and CPPAPI.Request ~= nil then
        local protocol = "http"
        local body = nil
        local headers = nil
        if port % 1000 == 443 then
            protocol = "https"
        end
        if port ~= 80 and port ~= 443 then
            host = host .. ":" .. tostring(port)
        end

        if method == "POST" then
            headers = {
                ["Content-Type"] = "application/x-www-form-urlencoded; charset=utf-8"
            }
            local originalUrl = url
            url, body = url:match("(.-)%?(.*)")
            url = url or originalUrl
        end

        CPPAPI.Request(
            {
                method = method,
                url = protocol .. "://" .. host .. url,
                body = body,
                headers = headers
            },
            function(error, response, code)
                if error then
                    func("\\\\Error:" .. error, error)
                else
                    func(string.format("Content-Length: %d\r\n\r\n%s", #response, response))
                end
            end
        )
    else
        AsyncConnCtr = (AsyncConnCtr or 0) + 1
        AsyncCreateId(
            CPPAPI.AsyncConnectWebsite(
                host,
                url,
                port or 80,
                http11 or false,
                timeout,
                method == "GET" and true or false,
                false
            ),
            func
        )
    end
end
function SmartHTTP(method, host, url, func)
    if url:find("?") then
        url = url .. "&rqt=" .. Time()
    else
        url = url .. "?rqt=" .. Time()
    end
    return AsyncConnectHTTP(
        host,
        url,
        method,
        80,
        true,
        5000,
        function(ret, error)
            if error ~= nil then
                func(error, true)
            elseif ret:sub(1, 8) == "\\\\Error:" then
                func(ret:sub(3), true)
            else
                func(ret, false)
            end
        end
    )
end
function SmartHTTPS(method, host, url, func)
    if url:find("?") then
        url = url .. "&rqt=" .. Time()
    else
        url = url .. "?rqt=" .. Time()
    end
    return AsyncConnectHTTP(
        host,
        url,
        method,
        443,
        true,
        5000,
        function(ret, error)
            if error ~= nil then
                func(error, true)
            elseif ret:sub(1, 8) == "\\\\Error:" then
                func(ret:sub(3), true)
            else
                func(ret, false)
            end
        end
    )
end

HTTP = SmartHTTP
HTTPS = SmartHTTPS

function GetIP(host)
    local prIP = host
    if (IsDllLoaded() or IsDllLoaded100()) and CPPAPI then
        prIP = CPPAPI.GetIP(host)
        if (not IsRealIP(prIP)) then
            prIP = _GetIP(prIP)
        end
    else
        prIP = _GetIP(host)
    end
    return prIP
end

function GetMapName()
    local n = System.GetCVar("sv_map")
    if (IsDllLoaded() or IsDllLoaded100()) and CPPAPI then
        local o = CPPAPI.GetMapName()
        if o then
            n = o
        end
    else
        n = System.GetCVar("sv_map")
    end
    return n:lower()
end

function Random(n, x)
    if n and x then
        local num = CPPAPI.Random()
        local diff = x - n + 1
        num = num + diff
        num = num % (diff)
        return num + n
    end
    return CPPAPI.Random()
end

function SanitizeName(player)
    name = player:GetName()
    sannam = "Player" .. player.profile
    if (string.find(name, "%$")) then
        sannam = name
    end
    return sannam
end

function ProcChatMsg(sender, target, line)
    local mType = 0
    local tgt = nil
    if (target == "ALL") then
        mType = ChatToAll
        tgt = nil
    elseif (target == "TEAM" or target == "Team black" or target == "Team tan") then
        mType = ChatToTeam
        tgt = nil
    else
        tgt = System.GetEntityByName(target)
    end
    local src = System.GetEntityByName(sender)
    if (not src) then
        src = SafeWriting.ChatEntity
    end
    g_gameRules:OnChatMessage(mType, src, tgt, line:sub(4))
end

function ProcSvLogMsg(line)
    _G["AwaitMsg"] = _G["AwaitMsg"] or false
    local sender, target = string.match(line, "^CHAT (.*) to (.*):")
    if (sender and target) then
        _G["AwaitMsg"] = true
        _G["MsgSender"] = sender
        _G["MsgTarget"] = target
    else
        if _G["AwaitMsg"] then
            ProcChatMsg(_G["MsgSender"], _G["MsgTarget"], line)
            _G["AwaitMsg"] = false
        else
            local mapn = string.match(line, "^[*]LOADING: Level (.-) loading time")
            if mapn then
                SafeWriting.ProbableMap = mapn
                System.SetCVar("sv_map", mapn)
            end
        end
    end
end
function ReadServerLog()
    local f, err = io.open(SafeWriting.MainFolder .. "Server.log", "r")
    if f then
        _G["LastLineIdx"] = _G["LastLineIdx"] or 0
        f:seek("set", _G["LastLineIdx"])
        local line = ""
        while line do
            line = f:read("*line")
            if (not line) then
                break
            end
            ProcSvLogMsg(line)
        end
        _G["LastLineIdx"] = f:seek()
        f:close()
    else
        SetError("Failed to open the Server.log stream!", true)
    end
end
function CheckStatusLines(_player, _chnlId)
    if _chnlId and _player then
        if not _G["ChannelInfo"] then
            _G["ChannelInfo"] = {}
        end
        if _G["ChannelInfo"][_chnlId] then
            local i = _G["ChannelInfo"][_chnlId]
            _player.host = i.host
            _player.profile = i.profile
            _player.channelId = i.channelId
            _player.state = i.state
            _player.ip = i.ip
            _player.port = i.port
            if not _player.connecttime then
                _player.connecttime = _time
            end
            CheckPlayer(_player)
            return
        end
    end
    System.ExecuteCommand("status")
    local file, err = io.open(SafeWriting.MainFolder .. "Server.log", "r")
    for line in file:lines() do
        local name, channelId, hostname, port, ping, state, profile =
            string.match(line, "^name: (.*)  id: (.*)  ip: (.*):(.*)  ping: (.*)  state: (.*) profile: (.*)")
        channelId, port, ping, state, profile = tonum(channelId, port, ping, state, profile)
        if (channelId) then
            channelId = tonumber(channelId)
            local player = g_gameRules.game:GetPlayerByChannelId(channelId)
            if (player) then
                if (not player.connecttime) then
                    player.connecttime = _time
                end
                player.profile = profile
                player.host = hostname
                if (not player.ip) then
                    player.ip = _GetIP(hostname)
                end
                player.channelId = channelId
                player.port = port
                player.state = state
                CheckPlayer(player, true)
            else
                if not _G["ChannelInfo"] then
                    _G["ChannelInfo"] = {}
                end
                if not _G["ChannelInfo"][channelId] then
                    _G["ChannelInfo"][channelId] = {
                        profile = profile,
                        host = hostname,
                        ip = _GetIP(hostname),
                        channelId = channelId,
                        port = port,
                        state = state
                    }
                end
            end
        end
    end
end
function CheckPlayer(player, noevent)
    novent = noevent or false
    local se = SafeWriting.Settings
    local channelId = player.actor:GetChannel()
    if _G["ChannelInfo"] and _G["ChannelInfo"][channelId] then
        local newProfile = _G["ChannelInfo"][player.channelId].profile
        if newProfile ~= "0" and (player.profile == nil or player.profile == "0") then
            player.profile = newProfile
            printf("Re-using cached profile %s for player %s (channel %d)", player.profile, player:GetName(), channelId)
        elseif player.profile ~= nil and player.profile ~= "0" then
            _G["ChannelInfo"][player.channelId].profile = player.profile
            printf("Caching profile ID %s for player %s (channel %d)", player.profile, player:GetName(), channelId)
        end
    end
    if se.AllowMasterServer and (tostring(player.profile) == "0" or (not player.isSfwCl)) then
        player.waitingForAuth = _time
        printf(
            "Skipped player check yet (%s, profile is %s, this is %d. time)",
            player:GetName(),
            tostring(player.gsprofile or 0),
            (player.checkSkips or 1)
        )
        player.checkSkips = (player.checkSkips or 1) + 1
        return
    end
    if player.desiredName then
        RenamePlayer(player, player.desiredName)
        player.desiredName = nil
    end
    if (not player.WasChecked) then
        if (se.UsePermaBans) then
            local ispermabanned, reason = IsPermabanned(player)
            if (ispermabanned) then
                player.wasForceDisconnected = true
                CryAction.BanPlayer(player.id, reason or "You are permabanned here")
                if KICK_REMOVE_ENTITY then
                    System.RemoveEntity(player.id)
                end
                return true
            end
        end
        player.assignedProperties = HasProperty(player, "GET_ALL")
        if player.assignedProperties then
            for i, v in pairs(player.assignedProperties) do
                local fn = _G[i]
                if fn then
                    fn(player)
                end
            end
        end
        if (se.BannedProviders) then
            for i, v in pairs(se.BannedProviders) do
                if (string.match(player.host, v) or player.host:find(v)) then
                    player.wasForceDisconnected = true
                    CryAction.BanPlayer(player.id, "your ISP is banned here")
                    if KICK_REMOVE_ENTITY then
                        System.RemoveEntity(player.id)
                    end
                    return
                end
            end
        end
        if (player.ip and IsRealIP(player.ip)) then
            if (verifyip(player.ip)) then
                player.wasForceDisconnected = true
                CryAction.BanPlayer(player.id, "you are permabanned here")
                if KICK_REMOVE_ENTITY then
                    System.RemoveEntity(player.id)
                end
                return
            end
        end
        if (se.AntiCheatEnabled and se.DetectProfileSpoofing) then
            if (math.abs(tonumber(player.profile)) >= (se.ProfileSpoofingThreshold or 1250000000)) then
                AntiCheat:DealWithPlayer(player, "spoofed profile id")
            elseif (tonumber(player.profile) < 0) then
                AntiCheat:DealWithPlayer(player, "spoofed profile id")
            end
        end
        if tonumber(player.profile) == 0 or (player.isOpenSpy and (not player.isSfwCl)) then
            player.waitingForAuth = _time
        end
        local pid = tonumber(player.profile)
        printf(
            "CheckPlayer: player client: %s, profile: %s, gsprofile: %s",
            player.isSfwCl and "sfwcl" or "else",
            tostring(player.profile),
            tostring(player.gsprofile)
        )

        if player.isSfwCl and (SafeWriting.Settings.StrictProfilePolicy and pid >= 800000 and pid <= 1000000) then
            KickPlayer(player, "please, update your client")
            return
        end
        g_gameRules.game:RenamePlayer(player.id, player:GetName():gsub(" ", "-"))
        if (se.EnableCrews) then
            local plcrew = GetPlayersCrew(player)
            if (plcrew and player.CrewName == nil) then
                player.CrewName = plcrew
                local tag = GetCrewTag("left") .. player.CrewName .. GetCrewTag("right")
                local name = player:GetName()
                if (name:sub(0, string.len(tag)) ~= tag) then
                    name = tag .. name
                end
                g_gameRules.game:RenamePlayer(player.id, name)
            end
        end
        if (se.EnableStatistics) then
            local profile = tonumber(player.profile)
            if (GetPlayersInfo and (not player.statsidx) and player.isSfwCl and (profile < 800000 or profile > 1000000)) then
                player.statsidx = GetPlayersInfo(player)
            end
        end
        if (se.UseAuthentificationPassword and g_gameRules.class == "PowerStruggle") then --auth works only on PS :(
            player.IsAdminLogged = false
            player.IsModeratorLogged = false
            player.IsPremiumLogged = false
        else
            if (se.Admins[player.profile]) then
                player.IsAdminLogged = true
            end
            if (se.Moderators[player.profile]) then
                player.IsModeratorLogged = true
            end
            if (se.Premiums[player.profile]) then
                player.IsPremiumLogged = true
            end
        end
        local pts = split(player.host, ".")
        pts = CTableToLuaTable(pts)
        lang = pts[#pts]
        if (player.ip:sub(1, ("192.168"):len()) == "192.168") then
            lang = se.HomeCountry or "localhost"
            player.isHost = true
        else
            if tonumber(lang) then
                lang = "unknown"
            end
        end
        player.country = lang or "unknown"
        if not noevent then
            MakePluginEvent("CheckPlayer", player)
        end
        player.WasChecked = true
    end
end
function verifyip(ip)
    local se = SafeWriting.Settings
    local ipparts = CTableToLuaTable(split(ip, "."))
    local success = 0
    if (not se.IPRangeBans) then
        return false
    end
    for i, v in pairs(se.IPRangeBans) do
        local rbp = CTableToLuaTable(split(v, "."))
        for j, w in pairs(rbp) do
            if (string.find(w, "-", nil, true)) then
                local _min, _max = unpack(CTableToLuaTable(split(w, "-")))
                _min = tonumber(_min)
                _max = tonumber(_max)
                local _ip = tonumber(ipparts[j])
                if (_max and _min and _ip) then
                    if ((_ip >= _min) and (_ip <= _max)) then
                        success = success + 1
                    end
                end
            else
                if (w == ipparts[j]) then
                    success = success + 1
                end
            end
        end
        if (success == 4) then
            return true
        else
            success = 0
        end
    end
    return false
end
function GetCrewTag(dir)
    --0 l,1 r
    if (dir == "left") then
        dir = 0
    end
    if (dir == "right") then
        dir = 1
    end
    local crbasics = SafeWriting.Settings.CrewBasics
    if (not crbasics) then
        crbasics = "<>"
    end
    if (dir == nil) then
        dir = 0
    end
    if (dir > 1) then
        dir = 0
    end
    if (dir == 0) then
        return crbasics:sub(0, 1)
    else
        return crbasics:sub(2)
    end
end
function GetPlayerByName(name)
    if (name == nil) then
        return nil
    end
    local players = g_gameRules.game:GetPlayers()
    local selplayer = nil
    local fplayers = 0
    if (players) then
        for i, player in pairs(players) do
            if (player:GetName() == name) then
                return player
            elseif (string.find(string.lower(player:GetName()), string.lower(name), nil, true)) then
                fplayers = fplayers + 1
                selplayer = player
            end
        end
    else
        return nil
    end
    if (fplayers == 1) then
        return selplayer
    else
        return false
    end
end
GetPlayer = GetPlayerByName
function GetPlayers()
    local players = g_gameRules.game:GetPlayers()
    if not players then
        return {}
    end
    local known = {}
    local pl = {}
    for i, v in pairs(players) do
        if not known[v.id] then
            known[v.id] = true
            pl[#pl + 1] = v
        end
    end
    return pl
end
function GetPlayersByName(name)
    local players = GetPlayers()
    if (name == "*all") then
        return players
    end
    local out = {}
    if (players) then
        if name == "*us" or name == "*nk" and g_gameRules.class == "PowerStruggle" then
            local tgtTeam = (name == "*us" and 2 or 1)
            for i, v in pairs(players) do
                if g_gameRules.game:GetTeam(v.id) == tgtTeam then
                    out[#out + 1] = v
                end
            end
            return out
        end
        for i, player in pairs(players) do
            local _name = player:GetName()
            if (_name == name) then
                out[#out + 1] = player
            end
            _name = _name:lower()
            if (string.find(_name, name:lower(), nil, true)) then
                out[#out + 1] = player
            end
        end
    end
    return out
end
function GetRandomName(tries)
    --math.randomseed(Time());
    local se = SafeWriting.Settings
    if (not se.PlayerNames) then
        return nil
    end
    local tr = 0
    if (not tries) then
        tr = 0
    else
        tr = tries
    end
    local ridx = math.random(1, #se.PlayerNames)
    local rname = se.PlayerNames[ridx]
    local player = GetPlayerByName(rname)
    while player do
        ridx = math.random(1, #se.PlayerNames)
        rname = se.PlayerNames[ridx]
        player = GetPlayerByName(rname)
        tr = tr + 1
        if (tr * 2 > #se.PlayerNames) then
            return "RandomNumber_" .. ridx
        end
    end
    return rname
end
function GetPlayerByProfile(profileid)
    if (profileid == nil) then
        return
    end
    local players = g_gameRules.game:GetPlayers()
    for i, player in pairs(players) do
        if (player) then
            if (tonumber(player.profile) == tonumber(profileid)) then
                return player
            end
        end
    end
end
function HavePrivileges(player, cmdname)
    if (SSMCMDS[cmdname] ~= nil) then
        if (SSMCMDS[cmdname].AdminOnly == true) then
            if (IsAdmin(player)) then
                return true
            else
                return false
            end
        elseif (SSMCMDS[cmdname].AdminModOnly == true) then
            if (IsAdminOrMod(player)) then
                return true
            else
                return false
            end
        elseif (SSMCMDS[cmdname].ModOnly == true) then
            if (IsModerator(player)) then
                return true
            else
                return false
            end
        elseif (SSMCMDS[cmdname].PremiumOnly == true) then
            if (IsPremium(player)) then
                return true
            else
                return false
            end
        else
            return true
        end
    end
end
function CreateChatEntity(field, name, noprepare, notell)
    field = field or "ChatEntity"
    local entname = name or (SafeWriting.Settings.ChatEntityName or "[SafeWriting]")
    if System.GetEntityByName(entname) then
        if not notell then
            printf("Chat entity already exists")
        end
    else
        local params = {
            class = "Shotgun",
            position = {x = 1, y = 1, z = 3000},
            orientation = {x = 0, y = 0, z = 1},
            name = entname,
            properties = {
                bAdjustToTerrain = 1,
                Respawn = {
                    bRespawn = 1,
                    nTimer = 1,
                    bUnique = 1
                }
            }
        }
        local ChatEntity = System.SpawnEntity(params)
        if (ChatEntity) then
            CreateActor(ChatEntity)
            ChatEntity["is" .. field] = true
            if (g_gameRules and g_gameRules.class == "InstantAction") then
                g_gameRules.game:SetTeam(0, ChatEntity.id)
            else
                g_gameRules.game:SetTeam(2, ChatEntity.id)
            end
            SafeWriting[field] = ChatEntity
            if not notell then
                printf("Successfuly spawned the chat entity %s", entname)
            end
            if not noprepare then
                PrepareAll()
            end
        end
    end
end
function GetPlayerScore(player)
    local kills = g_gameRules:GetPlayerScore(player.id)
    local deaths = g_gameRules:GetPlayerDeaths(player.id)
    return kills, deaths
end
function GetRealPlayerScore(player)
    local kills = player.rKills or 0
    local deaths = player.rDeaths or 0
    return kills, deaths
end
function SetPlayerScore(player, kills, deaths)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_KILLS_KEY, kills)
    g_gameRules.game:SetSynchedEntityValue(player.id, g_gameRules.SCORE_DEATHS_KEY, deaths)
end
function ChangeTeam(player, newTeam)
    local newTeamID = 1
    local oldTeamID = g_gameRules.game:GetTeam(player.id)
    if (oldTeamID == 1) then
        newTeamID = 2
    end
    if (newTeam) then
        newTeamID = newTeam
    end
    if (g_gameRules.class ~= "PowerStruggle" and g_gameRules.class ~= "TeamInstantAction") then
        return
    end
    if (newTeamID ~= oldTeamID) then
        if (player.actor:GetHealth() > 0 and player.actor:GetSpectatorMode() == 0) then
            local kills, deaths = GetPlayerScore(player)
            local rKills, rDeaths = nil, nil
            if (player.rKills) then
                rKills = player.rKills
            end
            if (player.rDeaths) then
                rDeaths = player.rDeaths
            end
            g_gameRules:KillPlayer(player)
            SetPlayerScore(player, kills, deaths)
            if (rKills) then
                player.rKills = rKills
            end
            if (rDeaths) then
                player.rDeaths = rDeaths
            end
        end
        g_gameRules.game:SetTeam(newTeamID, player.id)
        if (newTeamID ~= 0) then
            g_gameRules.Server.RequestSpawnGroup(
                g_gameRules,
                player.id,
                g_gameRules.game:GetTeamDefaultSpawnGroup(newTeamID) or NULL_ENTITY,
                true
            )
            g_gameRules:QueueRevive(player.id)
        end
    end
end
function get_timezone()
    local now = os.time()
    return os.difftime(now, os.time(os.date("!*t", now)))
end
function get_tzoffset(timezone)
    local h, m = math.modf(timezone / 3600)
    return string.format("%+.4d", 100 * h + 60 * m)
end
function GetTimeZone(ts)
    local utcdate = os.date("!*t", ts)
    local localdate = os.date("*t", ts)
    localdate.isdst = false -- this is the trick
    return math.ceil(os.difftime(os.time(localdate), os.time(utcdate)) / 3600)
end
function ParseTime(s)
    local parts = {}
    if tonumber(s) ~= nil then
        return tonumber(s)
    end
    s:gsub(
        "([0-9]+)([smhdoyin]+)",
        function(num, unit)
            parts[unit] = tonumber(num)
        end
    )
    local y, mo, d, h, m, s =
        parts.y or 0,
        parts.mo or 0,
        parts.d or 0,
        parts.h or 0,
        parts.m or parts.min or 0,
        parts.s or 0
    return s + m * 60 + h * 3600 + d * 86400 + mo * 86400 * 30 + y * 86400 * 365
end
function AddCommand(name, params, desc, rights, fn)
    SSMCMDS[name] = {info = desc, name = name, params = params}
    SSMCMDS[name].func = function(self, sender, msg)
        local _msg = (fsplit0(msg, " "))
        _msg[0] = nil
        local succ, msg = fn(sender, unpack(_msg))
        if (not succ) and msg then
            Chat:SendToTarget(sender, msg)
        end
    end
end
function AddChatCommand(name, func, params, rights, desc, usage_gen)
    rights = rights or {}
    params = params or {}
    if (type(func) == "table" and type(params) == "function") then
        local tmp = func
        func = params
        params = tmp
    end
    if (not name) then
        printf(
            "Error: $0AddChatCommand($1name$0,func(self,sender,msg,...),params,[rights,desc,usage_gen]), name is missing!"
        )
        return
    end
    if (not func) then
        printf(
            "Error: $0AddChatCommand(name,$1func(self,sender,msg,...)$0,params,[rights,desc,usage_gen]), function is missing!"
        )
        return
    end
    if (desc) then
        SSMCMDS[name] = {info = desc}
    else
        SSMCMDS[name] = {}
    end
    SSMCMDS[name]["name"] = name
    SSMCMDS[name]["params"] = params
    SSMCMDS[name]["_func"] = func
    MergeTables(SSMCMDS[name], rights)
    if (SSMCMDS[name]["info"] and usage_gen) then
        SSMCMDS[name]["info"] = SSMCMDS[name]["info"] .. ", $6!" .. name .. " " .. table.concat(params, " ")
    end
    SSMCMDS[name].func = function(self, sender, msg)
        local args = {}
        local parametre = self["params"]
        local _msg = (fsplit0(msg, " "))
        _msg[0] = nil
        for i, v in pairs(parametre) do
            if i > #_msg then
                break
            end
            local arg = nil
            local val = _msg[i]
            if (v == PLAYER) then
                arg = GetPlayerByName(val)
            elseif (v == PLAYERS) then
                arg = GetPlayersByName(val)
            elseif (v == NUMBER or v == DOUBLE) then
                if (not val) then
                    arg = nil
                else
                    arg = tonumber(val)
                end
            elseif (v == INT) then
                if (not val) then
                    arg = nil
                else
                    arg = tonumber(val)
                    if (arg) then
                        arg = math.floor(arg)
                    end
                end
            elseif (v == WORD) then
                arg = val
            elseif (v == TEXT) then
                arg = table.concat(_msg, " ", i)
                args[i] = arg
                break
            elseif (v == TIME) then
                if not val then
                    arg = nil
                else
                    arg = ParseTime(val)
                end
            end
            args[i] = arg
        end
        self.IsUsable = function(self, player, timeout, tell, health)
            return IsCommandUsableForPlayer(self.name, player, timeout, tell or true, health)
        end
        self.OpenConsole = function(self, player)
            Msg:SendToTarget(player, __qt(player.lang, R.OPEN_CONSOLE))
        end

        self.NeedPP = function(self, player, pp)
            Chat:SendToTarget(player, "[[YOU_NEED_POINTS]]", pp)
        end
        self.NotEnoughPP = function(self, player)
            Chat:SendToTarget(player, "[[NOT_ENOUGH_POINTS]]")
        end

        --Enter
        self.EnterValidPlayer = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_PLAYER]]")
        end
        self.EnterValidPP = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_VALUE_PP]]")
        end
        self.EnterValidClass = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_CLASS]]")
        end
        self.EnterValidDistance = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_DISTANCE]]")
        end
        self.EnterText = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_TEXT]]")
        end
        self.EnterValidCommand = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_COMMAND]]")
        end
        self.EnterValidValue = function(self, player)
            Chat:SendToTarget(player, "[[ENTER_VALID_VALUE]]")
        end

        self.PlayerNotFound = function(self, player, name)
            Chat:SendToTarget(player, "[[PLAYER_NOT_FOUND]]", name or "")
        end

        --Must
        self.MustBeInVehicle = function(self, player)
            Chat:SendToTarget(player, "[[YOU_MUST_BE_IN_VEHICLE]]")
        end
        self.MustBeGreaterThan0 = function(self, player)
            Chat:SendToTarget(player, "[[VALUE_MUST_BE_HIGHER_THAN_0]]")
        end
        self.GetName = function(self)
            return self.name
        end
        _pcall(self._func, self, sender, msg, unpack(args))
    end
end
function Console:SendToTarget(player, msg, ...)
    if not player then
        return
    end
    if (not player.host) and player.actor then
        local channelId = player.actor:GetChannel() or -1
        if _G["ChannelInfo"] and _G["ChannelInfo"][channelId] then
            local f = _G["ChannelInfo"][channelId]
            player.host = f.host
            player.profile = f.profile
            player.ip = f.ip
            player.channelId = f.channelId
        end
    end
    if (SafeWriting.Settings.DisableTranslations) then
        player.lang = "en"
    end
    if (not player.lang) then
        if (Translator_DetectLang) then
            Translator_DetectLang(player)
        end
        if not player.lang then
            player.lang = "en"
        end
    end
    if (player.lang) then
        msg = SafeWriting.Translator:Translate(player.lang, msg)
    end
    local style = SafeWriting.Settings.Style or {}
    msg =
        msg:gsub(
        "${t:(%w-)|([0-9])}",
        function(a, b)
            return "$" .. (style[a] or b)
        end
    )
    if (...) then
        local p = {...}
        for i, v in pairs(p) do
            if type(v) == "string" then
                p[i] = v:gsub("($[0-9])", ""):gsub("([$])", "")
            end
        end
        msg = string.format(msg, unpack(p))
    end
    if ClearString then
        msg = ClearString(msg)
    end
    Out:Push(MESSAGE, {TextMessageConsole, msg, TextMessageToClient, player.id})
end
function Console:SendToAll(msg, ...)
    local style = SafeWriting.Settings.Style or {}
    msg =
        msg:gsub(
        "${t:(%w-)|([0-9])}",
        function(a, b)
            return "$" .. (style[a] or b)
        end
    )
    if (...) then
        msg = string.format(msg, ...)
    end
    if ClearString then
        msg = ClearString(msg)
    end
    Out:Push(MESSAGE, {TextMessageConsole, msg, TextMessageToAll})
end
function Chat:SendToTarget(from, to, msg, ...)
    local argv = nil
    if from ~= nil and type(to) == "string" then
        local tmp = to
        local tmpmsg = msg
        to = from
        from = nil
        msg = tmp
        if tmpmsg then
            argv = {tmpmsg, ...}
        end
    else
        if ... then
            argv = {...}
        end
    end
    if (not from or from == nil) then
        from = SafeWriting.ChatEntity
    end
    if (not from) then
        CreateChatEntity()
        from = SafeWriting.ChatEntity
    end
    if (not to or to == nil) then
        to = SafeWriting.ChatEntity
    end
    if (not to.host) and to.actor then
        local channelId = to.actor:GetChannel() or -1
        if _G["ChannelInfo"] and _G["ChannelInfo"][channelId] then
            local f = _G["ChannelInfo"][channelId]
            to.host = f.host
            to.profile = f.profile
            to.ip = f.ip
            to.channelId = f.channelId
        end
    end
    if (not to.lang) then
        if (Translator_DetectLang) then
            Translator_DetectLang(to)
        end
        if not to.lang then
            to.lang = "en"
        end
    end
    if (SafeWriting.Settings.DisableTranslations) then
        to.lang = "en"
    end
    if (to.lang) then
        msg = SafeWriting.Translator:Translate(to.lang, msg)
    end
    if argv then
        msg = string.format(msg, unpack(argv))
    end
    from.ChatExceptions = (from.ChatExceptions or 0) + 1
    Out:Push(CHAT, {ChatToTarget, from.id, to.id, msg, false})
    --g_gameRules.game:SendChatMessage(ChatToTarget,from.id,to.id,msg,false);
end
function Chat:SendToAll(from, msg, ...)
    local argv = nil
    if from ~= nil and type(from) == "string" then
        local tmpmsg = msg
        msg = from
        from = nil
        if tmpmsg then
            argv = {tmpmsg, ...}
        end
    else
        if ... then
            argv = {...}
        end
    end
    if (not from or from == nil) then
        from = SafeWriting.ChatEntity
    end
    if (not from) then
        CreateChatEntity()
        from = SafeWriting.ChatEntity
    end
    if argv then
        msg = string.format(msg, unpack(argv))
    end
    from.ChatExceptions = (from.ChatExceptions or 0) + 1
    Out:Push(CHAT, {ChatToAll, from.id, from.id, msg, false})
    --g_gameRules.game:SendChatMessage(ChatToAll,from.id,from.id,msg,false);
end
function Msg:SendToTarget(player, msg, kind, ...)
    if not player then
        return
    end
    if (kind) then
        kind = kind:lower()
        if (kind == "center") then
            kind = TextMessageCenter
        elseif (kind == "big") then
            kind = TextMessageBig
        elseif (kind == "info") then
            kind = TextMessageInfo
        elseif (kind == "server") then
            kind = TextMessageServer
        elseif (kind == "console") then
            kind = TextMessageConsole
        elseif (kind == "error") then
            kind = TextMessageError
        else
            kind = TextMessageCenter
        end
    end
    if (not kind) then
        kind = TextMessageCenter
    end
    if (not player.host) and player.actor then
        local channelId = player.actor:GetChannel() or -1
        if _G["ChannelInfo"] and _G["ChannelInfo"][channelId] then
            local f = _G["ChannelInfo"][channelId]
            player.host = f.host
            player.profile = f.profile
            player.ip = f.ip
            player.channelId = f.channelId
        end
    end
    if (not player.lang) then
        if (Translator_DetectLang) then
            Translator_DetectLang(player)
        end
        if not player.lang then
            player.lang = "en"
        end
    end
    if (SafeWriting.Settings.DisableTranslations) then
        player.lang = "en"
    end
    if (player.lang) then
        msg = SafeWriting.Translator:Translate(player.lang, msg)
    end
    if (...) then
        msg = string.format(msg, ...)
    end
    if ClearString then
        msg = ClearString(msg)
    end
    Out:Push(MESSAGE, {kind, msg, TextMessageToClient, player.id})
    --g_gameRules.game:SendTextMessage(kind,msg,TextMessageToClient,player.id);
end
function Msg:SendToAll(msg, kind, ...)
    if (kind) then
        kind = kind:lower()
        if (kind == "center") then
            kind = TextMessageCenter
        elseif (kind == "big") then
            kind = TextMessageBig
        elseif (kind == "info") then
            kind = TextMessageInfo
        elseif (kind == "server") then
            kind = TextMessageServer
        elseif (kind == "console") then
            kind = TextMessageConsole
        elseif (kind == "error") then
            kind = TextMessageError
        else
            kind = TextMessageCenter
        end
    end
    if (not kind) then
        kind = TextMessageCenter
    end
    if (...) then
        msg = string.format(msg, ...)
    end
    if ClearString then
        msg = ClearString(msg)
    end
    Out:Push(MESSAGE, {kind, msg, TextMessageToAll})
end
function MutePlayer(player)
    player.IsMuted = true
end
function UnmutePlayer(player)
    player.IsMuted = false
end
function BanPlayer(player, reason, duration)
    local originalTimeout = System.GetCVar("ban_timeout")
    if (not duration) then
        duration = System.GetCVar("ban_timeout")
    end
    if (not reason) then
        reason = "You were banned from this server"
    end
    player.wasForceDisconnected = true
    System.ExecuteCommand("ban_timeout " .. duration)
    CryAction.BanPlayer(player.id, reason)
    if KICK_REMOVE_ENTITY then
        System.RemoveEntity(player.id)
    end
    System.ExecuteCommand("ban_timeout " .. originalTimeout)
end
function KickPlayer(player, reason, duration)
    local originalTimeout = System.GetCVar("ban_timeout")
    if (not duration) then
        duration = 0.01
    end
    if (not reason) then
        reason = "You were kicked from this server"
    end
    player.wasForceDisconnected = true
    System.ExecuteCommand("ban_timeout " .. duration)
    CryAction.BanPlayer(player.id, reason)
    if KICK_REMOVE_ENTITY then
        System.RemoveEntity(player.id)
    end
    System.ExecuteCommand("ban_timeout " .. originalTimeout)
end
function TempBanPlayer(player, reason, t, bannedBy)
    return PermaBanPlayer(player, reason, bannedBy, false, Time(t))
end
function PermaBanPlayer(player, reason, bannedBy, isCheater, expire)
    if (not reason and isCheater) then
        reason = "cheating"
    end
    if (not reason) then
        reason = "permaban"
    end
    if (not bannedBy) then
        bannedBy = "AntiCheat"
    end
    if (not isCheater) then
        isCheater = false
    end
    local name = xmlfixstring(SanitizeName(player))
    local profile = player.profile
    local ip = player.ip
    local host = player.host
    local bantime = os.date("%d.%m.%Y %H:%M:%S", time)
    SafeWriting.Bans[#SafeWriting.Bans + 1] = {
        name,
        ip,
        host,
        profile,
        reason,
        bantime,
        bannedBy,
        expire or "0",
        player.hwid
    }
    local file, err = io.open(SafeWriting.GlobalStorageFolder .. "Bans.lua", "w")
    if (file) then
        file:write(arr2str(SafeWriting.Bans, "SafeWriting.Bans"))
        file:close()
    end
    player.wasForceDisconnected = true
    CryAction.BanPlayer(player.id, reason)
    if KICK_REMOVE_ENTITY then
        System.RemoveEntity(player.id)
    end
end
function CheckHWIDBan(player)
    local isperma, reason = IsPermabanned(player)
    if isperma then
        CryAction.BanPlayer(player.id, reason or "You are permabanned here")
        player.wasForceDisconnected = true
        System.RemoveEntity(player.id)
    end
end
function IsPermabanned(player)
    return HasProperty(player, "BanPlayer")
end
function GetPlayerProperties(player)
    player.assignedProperties = HasProperty(player, "GET_ALL")
end
function SetPlayerProperty(player, giver, prop, undo, reason, expire)
    if type(giver) == "string" then
        expire = reason
        reason = undo
        undo = prop
        giver = player
    end
    if type(reason) == "number" then
        local c = expire
        expire = reason
        reason = c
    end
    local name, ip, host, profile, reason, bannedBy =
        xmlfixstring(SanitizeName(player)),
        player.ip,
        player.host,
        player.profile,
        reason or "property assigned",
        giver:GetName()
    local bantime = os.date("%d.%m.%Y %H:%M:%S", time)
    expire = expire or 0
    SafeWriting.Bans[#SafeWriting.Bans + 1] = {
        name,
        ip,
        host,
        profile,
        reason,
        bantime,
        bannedBy,
        tostring(expire),
        player.hwid,
        prop,
        undo
    }
    if tostring(expire) ~= "0" then
        SafeWriting.Bans[#SafeWriting.Bans][8] = Time(expire)
    end
    local file, err = io.open(SafeWriting.GlobalStorageFolder .. "Bans.lua", "w")
    if (file) then
        file:write(arr2str(SafeWriting.Bans, "SafeWriting.Bans"))
        file:close()
    end
    player.assignedProperties = player.assignedProperties or {}
    player.assignedProperties[prop] = {["undo"] = undo, ["expire"] = SafeWriting.Bans[#SafeWriting.Bans][8]}
    local fn = _G[prop]
    fn(player)
end
function RemovePlayerProperty(player, prop, def)
    if player.assignedProperties[prop] then
        local fn = _G[player.assignedProperties[prop].undo or def]
        if fn then
            fn(player)
        end
        player.assignedProperties[prop] = nil
    elseif _G[def] then
        local fn = _G[def]
        fn(player)
    end
    HasProperty(player, prop, true)
end
function HasProperty(player, prop, rem)
    local bans = loadfile(SafeWriting.GlobalStorageFolder .. "Bans.lua")
    local props = {}
    local get = false
    if prop == "GET_ALL" then
        get = true
    end
    if bans then
        assert(bans)()
    end
    if (player.profile) then
        local expCount = 0
        local newBans = {}
        for i, v in pairs(SafeWriting.Bans) do
            local expire = tostring(v[8])
            --expire=tonumber(expire);
            if (tostring(expire) ~= "0") then
                if (Time() > expire) then
                    --table.remove(SafeWriting.Bans,i);
                    expCount = expCount + 1
                else
                    newBans[#newBans + 1] = v
                end
            else
                newBans[#newBans + 1] = v
            end
        end
        SafeWriting.Bans = newBans
        if expCount > 0 then
            printf("%d bans expired, they are deleted since now", expCount)
            local file, err = io.open(SafeWriting.GlobalStorageFolder .. "Bans.lua", "w")
            if (file) then
                file:write(arr2str(SafeWriting.Bans, "SafeWriting.Bans"))
                file:close()
            end
        end
        local torem = {}
        for i, v in pairs(SafeWriting.Bans) do
            local name, ip, host, profile, reason, bantime, bannedBy, expire, hwid, action, rem_action = unpack(v)
            action = action or "BanPlayer"
            local plName = xmlfixstring(player:GetName())
            --expire=tonumber(expire);
            local plProfile = player.profile
            local plIP = player.ip
            local plHost = player.host
            local useIDBan = SafeWriting.Settings.UseIDBan
            if SafeWriting.Settings.AllowMasterServer then
                local p1 = tonumber(plProfile)
                local p2 = tonumber(profile)
                if (p1 >= 800000 and p1 <= 1000000) then
                    useIDBan = false --Temp profile, cant ban this!
                end
            end
            local useIPBan = SafeWriting.Settings.UseIPBan
            local useNameBan = SafeWriting.Settings.UseNameBan
            local expired = false
            if (tostring(expire) ~= "0") then
                if (Time() > tostring(expire)) then
                    expired = true
                end
            end
            if action == prop or get or rem then
                if (useNameBan and name == plName and (not expired)) then
                    if rem then
                        torem[i] = true
                    elseif get then
                        props[action] = {["undo"] = rem_action, ["expire"] = expire}
                    else
                        return true, reason
                    end
                elseif (useIPBan and (ip == plIP or host == plHost) and (not expired)) then
                    if rem then
                        torem[i] = true
                    elseif get then
                        props[action] = {["undo"] = rem_action, ["expire"] = expire}
                    else
                        return true, reason
                    end
                elseif (useIDBan and profile == plProfile and (not expired)) then
                    if rem then
                        torem[i] = true
                    elseif get then
                        props[action] = {["undo"] = rem_action, ["expire"] = expire}
                    else
                        return true, reason
                    end
                elseif player.hwid and hwid and player.hwid == hwid and (not expired) then
                    if rem then
                        torem[i] = true
                    elseif get then
                        props[action] = {["undo"] = rem_action, ["expire"] = expire}
                    else
                        return true, reason
                    end
                end
            end
        end
        if rem then
            local n = {}
            local ctr = 0
            for i, v in pairs(SafeWriting.Bans) do
                if not torem[i] then
                    n[#n + 1] = v
                else
                    ctr = ctr + 1
                end
            end
            SafeWriting.Bans = n
            local file, err = io.open(SafeWriting.GlobalStorageFolder .. "Bans.lua", "w")
            if (file) then
                file:write(arr2str(SafeWriting.Bans, "SafeWriting.Bans"))
                file:close()
            end
            return ctr
        end
    end
    if get then
        return props
    end
    return false
end
--Spawning
function Spawn:CalculatePosition(player, distance)
    local pos = player:GetBonePos("Bip01 head")
    local dir = player:GetBoneDir("Bip01 head")
    if not pos and not dir then
        pos = player:GetWorldPos()
        dir = player:GetWorldAngles()
    end
    ScaleVectorInPlace(dir, tonumber(distance))
    FastSumVectors(pos, pos, dir)
    dir = player:GetDirectionVector(1)
    return pos, dir
end
function Spawn:Entity(player, class, distance, callback, customparams)
    Script.SetTimer(
        2,
        function()
            if (not SafeWriting.SpawnCounter) then
                SafeWriting.SpawnCounter = 0
            end
            if type(class) == "number" and type(distance) == "string" then
                local tmp = distance
                distance = class
                class = tmp
            end
            local pos, dir = Spawn:CalculatePosition(player, distance)
            local realClass = class
            local fCallbacks = nil
            if KnownClasses and KnownClasses[class] then
                realClass = KnownClasses[class]
            --fCallbacks=_G[class];
            end
            local params = {
                class = realClass,
                position = pos,
                orientation = dir,
                name = class .. "(" .. SafeWriting.SpawnCounter .. ")"
            }
            if (customparams) then
                MergeTables(params, customparams)
            end
            SafeWriting.SpawnCounter = SafeWriting.SpawnCounter + 1
            local SpawnedEntity = System.SpawnEntity(params)
            if (SpawnedEntity) then
                SpawnedEntity:AwakePhysics(1)
                if fCallbacks then
                    for i, v in pairs(fCallbacks) do
                        if type(v) == "function" then
                            SpawnedEntity[i] = v
                        end
                    end
                    MergeTables(SpawnedEntity, fCallbacks)
                end
                SpawnedEntity["is" .. class] = true
                if (callback) then
                    callback(SpawnedEntity)
                end
            else
                return nil
            end
        end
    )
end
function Spawn:Vehicle(player, class, distance, callback, modification, customparams)
    Script.SetTimer(
        2,
        function()
            if (not SafeWriting.SpawnCounter) then
                SafeWriting.SpawnCounter = 1
            end
            if type(class) == "number" and type(distance) == "string" then
                local tmp = distance
                distance = class
                class = tmp
            end
            local pos, dir = Spawn:CalculatePosition(player, distance)
            local teamId = g_gameRules.game:GetTeam(player.id)
            local paintname = nil
            local fCallbacks = nil
            local realClass = class
            if KnownClasses and KnownClasses[class] then
                realClass = KnownClasses[class]
            --fCallbacks=_G[class];
            end
            local params = {
                class = realClass,
                position = {x = pos.x, y = pos.y, z = pos.z + 4.0},
                orientation = dir,
                name = class .. "(" .. SafeWriting.SpawnCounter .. ")",
                properties = {
                    Modification = modification or "",
                    Paint = paintname,
                    Respawn = {
                        bRespawn = 0,
                        nTimer = 30,
                        bUnique = 1,
                        bAbandon = 0,
                        nAbandonTimer = 0
                    }
                }
            }
            if (customparams) then
                MergeTables(params, customparams)
            end
            if (teamId ~= 0 and g_gameRules.VehiclePaint) then
                params.properties.Paint = g_gameRules.VehiclePaint[g_gameRules.game:GetTeamName(teamId)] or ""
            end
            SafeWriting.SpawnCounter = SafeWriting.SpawnCounter + 1
            local SpawnedEntity = System.SpawnEntity(params)
            if (SpawnedEntity) then
                g_gameRules.game:SetTeam(teamId, SpawnedEntity.id)
                SpawnedEntity:AwakePhysics(1)
                SpawnedEntity["is" .. class] = true
                if fCallbacks then
                    for i, v in pairs(fCallbacks) do
                        if type(v) == "function" then
                            SpawnedEntity[i] = v
                        end
                    end
                    MergeTables(SpawnedEntity, fCallbacks)
                end
                if (callback) then
                    callback(SpawnedEntity)
                end
            else
                return nil
            end
        end
    )
end
function Spawn:VehicleForPlayer(player, class, distance, autolock)
    self:Vehicle(
        player,
        class,
        distance,
        function(veh)
            if (autolock) then
                veh.lockowner = player.profile
                veh.vehicle:SetOwnerId(player.id)
            end
        end,
        "MP"
    )
end
function GetTargetPos(player)
    local hittbl = {}
    local posvec = player.actor:GetHeadPos()
    local dirvec = player.actor:GetHeadDir()
    dirvec.x = dirvec.x * 8192
    dirvec.y = dirvec.y * 8192
    dirvec.z = dirvec.z * 8192
    local hits = Physics.RayWorldIntersection(posvec, dirvec, 10, ent_all, player.id, nil, hittbl)
    if (hits > 0) then
        local hit_ = hittbl[1]
        return hit_.pos
    end
    return nil
end
ChatStream = {_type = "none"}
ConsoleStream = {_type = "none"}
MessageStream = {_type = "center"}
function GetPlayersInTeam(t)
    if type(t) == "number" then
        return GetPlayers("*" .. (teamNames[t] or "all"))
    end
    return GetPlayers("*" .. (t or "all"))
end
function ChatStream:SendToTarget(target, fmt, ...)
    Chat:SendToTarget(nil, target, fmt, ...)
end
function ConsoleStream:SendToTarget(target, fmt, ...)
    Console:SendToTarget(target, fmt, ...)
end
function MessageStream:SendToTarget(target, fmt, ...)
    Msg:SendToTarget(target, fmt, self._type, ...)
end
function ChatStream:SendToAll(fmt, ...)
    Chat:SendToAll(nil, fmt, ...)
end
function ConsoleStream:SendToAll(fmt, ...)
    Console:SendToAll(fmt, ...)
end
function MessageStream:SendToAll(fmt, ...)
    Msg:SendToAll(fmt, self._type, ...)
end
function ChatStream:SendToTeam(target, fmt, ...)
    local t = GetPlayersInTeam(target)
    for i, v in pairs(t) do
        Chat:SendToTarget(nil, v, fmt, ...)
    end
end
function ConsoleStream:SendToTeam(target, fmt, ...)
    local t = GetPlayersInTeam(target)
    for i, v in pairs(t) do
        Console:SendToTarget(v, fmt, ...)
    end
end
function MessageStream:SendToTeam(target, fmt, ...)
    local t = GetPlayersInTeam(target)
    for i, v in pairs(t) do
        Msg:SendToTarget(t, fmt, self._type, ...)
    end
end
function MessageStream:SetType(t)
    self._type = t
end
function ChatStream:SetType(t)
    self._type = t
end --avoid errors
function ConsoleStream:SetType(t)
    self._type = t
end
function SendMessage(tp, to, msg)
    local args = {msg}
    local stream = MessageStream
    if tp == CHAT then
        stream = ChatStream
    elseif tp == CONSOLE then
        stream = ConsoleStream
    else
        stream:SetType(TypeRev[tp] or "server")
    end
    if type(to) == "number" then
        if to == ALL then
            stream:SendToAll(msg)
        else
            stream:SendToTeam(to, msg)
        end
    else
        stream:SendToTarget(to, msg)
    end
end
System.AddCCommand("SfWDir", "InitializeFolders(%%)", "Initializes folders for SSM SafeWriting")
System.AddCCommand("SfW_ReloadScripts", "ReloadAllScripts(%%)", "Reloads all scripts of SSM SafeWriting")
System.AddCCommand("SfWGameVer", "SetGameVersion(%%)", "Sets game version for mod")
System.AddCCommand("SfWSetTempVer", "SfwSetTempVersion(%line)", "Sets old version")
System.AddCCommand("Sfw_ReloadCore", "CPPAPI.ReloadCoreScripts()", "Reloads core scripts")
SafeWriting.GeneratedCCommands["reloadscripts"] = true
printf("Setting up the directories")
System.ExecuteCommand("exec sfw.cfg")
if not SafeWriting.__MainFolder then
    InitializeFolders(System.GetCVar("sys_root"))
end
_G.ExternalScriptsLoaded = true
