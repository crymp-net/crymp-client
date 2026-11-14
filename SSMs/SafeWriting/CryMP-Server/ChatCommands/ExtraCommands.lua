SSMCMDS.falloutcreate = {AdminOnly=true;info="[[falloutcreate_info]]"};
SSMCMDS.falloutdelete = {AdminOnly=true;info="[[falloutdelete_info]]"};
SSMCMDS.falloutstart = {AdminOnly=true;info="[[falloutstart_info]]"};
SSMCMDS.fallouttp={info="[[fallouttp_info]]"};
function SpawnFalloutPlatform(player,distance,offsetX)
	if(not SafeWriting.SpawnCounter)then
		SafeWriting.SpawnCounter=0;
	end
	local pos=Spawn:CalculatePosition(player,distance);
	pos.x=pos.x+offsetX;
	pos.z=player:GetWorldPos().z;
	local params={
		class="Flag";
		position=pos;
		orientation={x=0,y=0,z=1};
		name="FalloutPlatform".."("..SafeWriting.SpawnCounter..")"; 
	};
	SafeWriting.SpawnCounter=SafeWriting.SpawnCounter+1;
	local SpawnedEntity = System.SpawnEntity(params);	
	if(SpawnedEntity)then
		return SpawnedEntity;
	else
		return nil;
	end
end
function SSMCMDS.falloutcreate:func(player, msg)
	Script.SetTimer(2,function()
		local w=10;
		local h=10;
		local name="Fallout_Platform%d";
		if(not SafeWriting.GlobalStorage.FalloutPlatforms)then
			SafeWriting.GlobalStorage.FalloutPlatforms={};
			SafeWriting.GlobalStorage.FalloutIdx=0;
		end
		SafeWriting.GlobalStorage.FalloutPlatforms={};
		SafeWriting.GlobalStorage.FalloutIdx=0;
		SafeWriting.GlobalStorage.FalloutPos=Spawn:CalculatePosition(player, (w/2)*1.5);
		SafeWriting.GlobalStorage.FalloutPos.x=SafeWriting.GlobalStorage.FalloutPos.x+h;
		SafeWriting.GlobalStorage.FalloutTpAllowed=true;
		SafeWriting.GlobalStorage.FalloutInProgress=false;
		for x=1,w do
			for y=1,h do
				SafeWriting.GlobalStorage.FalloutPlatforms[SafeWriting.GlobalStorage.FalloutIdx]=SpawnFalloutPlatform(player,x*1.5,y*2.07);
				SafeWriting.GlobalStorage.FalloutIdx=SafeWriting.GlobalStorage.FalloutIdx+1;
			end
		end
	end);
end
function SSMCMDS.falloutdelete:func(player,msg)
	for i,v in pairs(SafeWriting.GlobalStorage.FalloutPlatforms) do
		System.RemoveEntity(v.id);
	end
	SafeWriting.GlobalStorage.FalloutIdx=0;
	SafeWriting.GlobalStorage.FalloutPlatforms={};
	SafeWriting.GlobalStorage.FalloutTpAllowed=false;
	SafeWriting.GlobalStorage.FalloutInProgress=false;
end
function SSMCMDS.falloutstart:func(player,msg)
	SafeWriting.GlobalStorage.FalloutTpAllowed=false;
	SafeWriting.GlobalStorage.FalloutInProgress=true;
	if(SafeWriting.GlobalStorage.FalloutPlatforms)then
		for i,v in pairs(SafeWriting.GlobalStorage.FalloutPlatforms) do
			Script.SetTimer(math.random(1,SafeWriting.GlobalStorage.FalloutIdx)*1000,function()
				System.RemoveEntity(v.id);
			end);
		end
		Script.SetTimer(SafeWriting.GlobalStorage.FalloutIdx*1000,
			function()
				FalloutInProgress=false;
			end
		);
	end
end
function SSMCMDS.fallouttp:func(player,msg)
	if(SafeWriting.GlobalStorage.FalloutTpAllowed)then
		TeleportPlayer(player,SafeWriting.GlobalStorage.FalloutPos);
	else
		Chat:SendToTarget(nil,player,"There's no Fallout event in progress now");
	end
end