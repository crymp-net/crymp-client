SafeWriting.Settings={

	Maps={
		["multiplayer/ps/recovery"]="[2025.03]https://crymp.org/static/Recovery202503.zip";
	};
	
	--[[
		Set to Yes/No/Maybe
		Maybe = will become Yes automaticaly on 1.6.2014
	--]]
	AllowMasterServer=Yes;	 --allows reporting to masterserver + allows users connected without GS using SafeWritingClient
	MasterHost="crymp.org";	--master server host
	
	StrictProfilePolicy=true;	--set to true to kick players with old profile IDs <800 000, 999 999>
	
	EnablePremiumBuilders=false;	--enable leveldesigner for premiums, their builds wont save when !ldsave
	LevelDesignerPremiumLimit=128;	--max entities that can be built by premium player
	
	ServerDescription="SSM SafeWriting server";	--description for your server
	APIKey=nil;	--leave this empty unless you are a trusted server
	
	OptimizeSpeed=false;	--makes server do less output (recommended for non-stop servers)
	HomeCountry="unknown";	--set it to anything, for example: sk,cz,pl,ua,at,ru,de,uk,...
	EnableSafeWritingExe=true;	--enables using SSMSafeWriting.exe for mod
	UseDLLInfoLoader=true; --use dll to load player's info - it's much faster than normal Lua way.
	HashSeed=0x1F0;	--this is seed for hashing algorithm :) please set it to any random number that is on your mind and is higher than 127
	OutQueueLimit=100; --sets the limit of console queue, when nil, it is 6e+21
	ChatMessageSpamTime=3;	--sets the limit between all chat commands in seconds
	LogHitsToConsole=false;	--if true, when player gets hit, he gets info written in console - damage, shooter, weapon
	UseDidYouMeanFeature=true;	--if player writes wrong name of command, it gives him possible correction
	DidYouMeanLimit=30;		--threshold of correction similarity, lowest = worst correction, max = 99 (99% similarity of wrongly written command and corrected command)
	CanAllSeeChat=true;	--if true, normal players can see chat of dead and spectators
	CommandsExtension='!';	--this can also be an array, like {'!'} or {'!','/'}, this thing sets, that what must command begin with, so if you set it to '/' instead of '!', all commands will be like /name, /teleport, not !name, !teleport
							--if you use array with multiple patterns, it will work with all, so for example {'!','/'} means that /name will work, but !name will work  too :)
	InfantriesBlockLAWC4=false;	--if enabled, LAW (bazooka) and C4s will make 0 damage on infantries
	ReduceVehicleDamage=false;		--if enabled, vehicles at stunts will take 8 times less damage, but also, they wont explode after getting shot to fuel tank
	ShowTargetHealth=false;	--enable displaying health bars when shooting enemy
	
	UseRealTime=false; --set to true, when you want in-game time of real world, it means: when it's 20:00 in real world, its also 20:00 in-game
	Gravitation=(-9.8); --default -9.8
	InitJailOnStart=true; --creates jail on random position on round start
	
	UseCustomTime=false; --set to true when you want to use custom time,but make sure you set UseRealTime to false
	TimeOfDayStart=12; --set to any number you wish
	TimeOfDaySpeed=0;  --set to any number you wish	
	
	AutoGenerateCCommands=true;	--if enabled, script will generate console command for every chat command, console command will be sfw_{chat command name} {params...}
	--^^^^^ for above, notice that if chat command does not require any parameters, console command still does, so use it like sfw_flare 0 or anything
	
	Admins={ -- here are profile IDs of admins
		["1"] = "Zi;"; --you can even write name here if you want
	};
	Moderators={ -- here are profile IDs of moderators
		--Moderators
	};
	Premiums={ -- here are profile IDs of premium players
		--Premiums
	};
	Others={	--put here all other people you want to protect names of
		--["123"]="Johnny123"; -- add people like this ["profile_id"]="name";
	};
	ProtectNames=true;	--enable name protection
	
	UseAuthentificationPassword=false;		--works only on PS, enables admin/moderator/... authentification through console "buy ..."
	AdminAuthPassword="AdminPass";			--admin login password goes here
	ModeratorAuthPassword="ModeratorPass";	--moderator login password goes here
	PremiumAuthPassword="PremiumPass";		--premium login password goes here
	UseCommandsSession=false;	--enables commands session = commands can be used only in time of session for player
	SessionExpiry=30; --in seconds
	CommandsSessionFlags=SessionFlags.AdminsModerators; --SessionFlags.AdminsModerators or SessionFlags.All
	ImmediateExpire=false;	--session expires immediately after using some command when set to true
	UseSessionSalt=false;	--if enabled, password will be equal to: JL1Hash:Hash(password..player.profile)
	
	EnableChatLog=true; -- enables chat log in console
	LogChatToAdmins=true; --enables logging chat to admins
	AllowShortPMs=true;	--enables short form of private messages: @player text...
	UseClearNames=true; --when enabled, removes all ¡,´,ÿ,¸,²,¨,½,^,°,¢ from letters in name
	DisableTranslations=false; --put this to true, to use english language for all players, but Translations.lua will be still required!
	FixOverpoweredAA=true;	--put this to true, to fix overpowered AACannon on 5767
	
	ChatEntityName="<SSM SafeWriting>";
	TempEntityName=">>";	--for non-dll version of mod! name of entity which is used to send spectator messages, if you want it to be empty, use space (" "), not ""!
	AdminTag="<ADMIN>"; -- makes admin invulnerable, with infinite energy
	ModeratorTag="<MOD>"; --gives extra health (+300) to moderator
	PremiumTag="<VIP>"; --gives some benefits to premium (+100 health)...
	CrewBasics="<>"; -- <CREW>Player
	EnableCrews=true; -- Enables crews/clans
	
	EnableStatistics=true; --Enables statistics (player's played time, kills and deaths)
	StatisticsAutoSave=true; --enable autosaving statistics
	AutoSaveInterval=1; --in minutes!
	
	AntiCheatEnabled=true; -- AntiCheat is still in develop
	RapidFireLogDetails=true; --logs details (to prevent bugs)
	ProfileSpoofingThreshold=1000000000; --most of times they are greater than 10^9
	DetectProfileSpoofing=false; --detect fake profile ids generated by hack
	DetectInfiniteAmmo=false; --detects infinite ammo
	DetectDamageHack=true; --detects modified damage (atom and hit hack)
	DetectTeleportHack=true;--also detects speed hack partially
	DetectFlyHack=false;	--uses too much CPU and is not very accurate
	DetectGhostGlitch=true; --detects ghost glitch
	DetectRapidFire=false;	--its not 100% accurate, but you can use it if you want
	
	KickForHighPing=true; --enable/disable kicking for high ping
	KickWhenMoreThanAverage=true; --if enabled, kicks only players with ping with differency of HighPingThreshold from average
	HighPingThreshold=250; --high ping threshold
	WarnForHighPing=true; --enable/disable to send warnings to players, who have high ping
	WarningsCount=3; --how many warning will player recieve till he gets kicked
	WarningInterval=10; --interval, in which player recieves warning (in seconds)
	PingSpoofRatio=1;	--the higher, the pings are "better", default: 1 - no spoof
	
	UsePermaBans=true;
	UseIDBan=true;
	UseIPBan=true;
	UseNameBan=false;
	IPRangeBans={	--put here all range bans, examples (contains all ips of crashers I know):
		--[["87.189.0-255.0-255",
		"91.61.0-255.0-255",
		"91.63.0-255.0-255",
		"141.45.0-255.0-255",
		"80.182.0-255.0-255",--]]
	};
	BannedProviders={	--put here all providers, always write \ (backslash) before interpunct!
		--[["182\-80\-r\.retail\.telecomitalia\.it",
		"p5B3D",
		"p5B3F",
		"p57BD",
		"p4FC0",--]]
	};
	
	UseWelcomeMessage=true;
	WelcomeMessage="[[WELCOME]] %s [[SpawnInfo1]]"; --%s gets replaced with players name
	OtherWelcomeMessages={
		"[[SpawnInfo2]]",
		"[[SpawnInfo3]]",
		"[[SpawnInfo4]]",
		"[[SpawnInfo5]]",
	};
	UsePersistantScores=true; --if enabled and player reconnects, his score is same as before
	
	ResetScoreOnSpectatorSwitch=false;
	ForbiddenAreaDisabled=true;
	
	UseCustomEquipment=true;
	BasicEquipment={ --use: {WeaponName,attachments...}
		{"SCAR","LAMRifle","Reflex","Silencer"},		
		{"FragGrenade"},
		{"FragGrenade"},
		{"Parachute"}
	};
	
	BlockedItems={	--you can put items here, those will be blocked
		--["macs"]=true; --blocks SCAR
		--["gauss"]=true; --blocks GaussRifle
		--you can find these ids in powerstrugglebuying.lua
	};
	
	CustomSettingsAtLoad={ --here are all cvars you want to set while mod is preparing map for game
		"hud_nightVisionConsumption 0", --please keep it in format: "cvar value", not "cvar=value" ;)
	};
	
	--you can delete this table if you don't want to use random names for Nomads
	PlayerNames={ --when player connects as Nomad it renames him to random name from those you define here
		"Hydrogenium","Helium","Lithium","Berylium",
		"Borum","Carboneum","Nitrogenium","Oxygenum",
		"Fluorum","Neon","Natrium","Magnesium",
		"Aluminium","Silicium","Phosphorus","Sulphur",
		"Chlorum","Argon","Kalium","Calcium",
		"Titanium","Chromium","Ferrum","Cuprum",
		"Zincum","Selenium","Stibium","Telurium",
		"Iodum","Xenon","Lanthanum","Actinium",
	};
		
	SpamMessages=true; --enable this to enable spammy messages
	SpammyMessagesType="server"; --server,info,center,console,big,error
	SpammyInterval=10; -- in seconds, 1 message per ? seconds
	SpammyMessages={	--here you put messages,which will spam to players in every ? seconds
		"This server is using SSM SafeWriting "..(SafeWriting.Version or " < 1.8.5"),
		"You can download free sources at https://crymp.org/",
	};
	
	MapVotingTimeout=300; --in seconds
	GameModes={
		PowerStruggle={
			Maps={
				Mesa={name="Mesa";map="mesa"};
				Beach={name="Beach";map="beach"};
				Shore={name="Shore";map="shore"};
				Plantation={name="Plantation";map="plantation"};
				Refinery={name="Refinery";map="refinery"};
			};
		};
		InstantAction={
			Maps={
				Armada={name="Armada";map="armada"};
				Outpost={name="Outpost";map="outpost"};
				Quarry={name="Quarry";map="quarry"};
				SteelMill={name="Steel Mill";map="steelmill"};
			};
		};
	};
	
	DistanceKillRewardAllowed=true;
	
	CensoreBadWords=true; --Enable this to censore bad words
	CensoreCharacter="*";	--Character used to censore word, example: fuck => f**k
	BadWords={ -- Here you put bad words
		"fuck","bitch",
		"whore","cunt",
		"nigger","nigga",
		"kurva","kokot",
		"jebe","jebat",
		"jeban","jebn",
		"kurven","kurvy",
		"jebl",	
	};
	
	Style={	--Some styling for chat log, etc.
		SessionActive=3;
		LoginSuccessful=3;
		ChatMsg1=9;	--[CHAT]
		ChatMsg2=3;	--Sender
		ChatMsg3=4;	--to
		ChatMsg4=3;	--Target
		ChatMsg5=9;	--Message	

		PosListHdr1=1;
		PosListHdr2=8;
		PosList1=9;
		PosList2=8;			
		PosListSide1=6;
		PosListSide2=4;		
		PosListInfo1=8;
		PosListInfo2=5;
		PosListInfo3=8;
		PosListInfo4=5;
		
		PlayersListHdr1=9;
		PlayersListHdr2=8;
		PlayersList1=3;
		PlayersList2=4;
		
		Top10Hdr1=5;
		Top10Hdr2=4;
		Top10On1=1;
		Top10On2=3;
		Top10Off1=9;
		Top10Off2=4;		
	};
};