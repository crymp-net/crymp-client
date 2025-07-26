CreateClass("PortalGun","SOCOM");

function PortalGun:OnShoot(hit)
	local wpn,player,dir,pos=hit.weapon,hit.shooter,hit.dir,hit.pos;
	if player and wpn and wpn.isPortalGun then
		local finalPos = GetTargetPos(player);
		if finalPos then
			TeleportPlayer(player,finalPos);
		end
	end
end
SafeWriting.FuncContainer:LoadPlugin(PortalGun);

AddChatCommand("portalgun",function(self,player)
	GiveItem(player,"PortalGun");
	Chat:SendToTarget(player,"Here one PortalGun for you!");
end,nil,{AdminOnly=true;},"Gives you a portal gun");