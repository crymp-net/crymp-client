--Created on 25th of December 2012 as part of SSM SafeWriting project
--Contains demonstration of using CreateClass function and :OnShoot event
CreateClass("FlareGun","SOCOM");	--define a new entity class called FlareGun, which is derived from SOCOM
function FlareGun:OnShoot(hit)		--now when its created, we can work with this class just like with plugin, because it will be plugin
	local wpn,player,dir,pos=hit.weapon,hit.shooter,hit.dir,hit.pos;	--since we created OnShoot method, param hit is passed here and we need to get important args from it
	if player and wpn and wpn.isFlareGun then --first check whether shooter exists and whether weapon is realy FlareGun
		player.flareGunShot=(player.flareGunShot or 0)+1;	--because we created this class "FlareGun", Spawn:Entity or GiveItem or Spawn:Vehicle should automaticaly spawn SOCOM and set is{classname} flag to true (in this case isFlareGun)
		pos=player:GetWorldPos();	--now get position, where we start flare
		pos.z=pos.z+1.6;			--make it start at player's head/weapon bone
		FastSumVectors(pos,pos,dir)	--fast sum vectors, so it is nicer
		dir.z=dir.z+torad(8.96);	--edit directions, because it could go more to left side etc.
		dir.x=dir.x-torad(2.96);
		dir.y=dir.y-torad(2.96);
		local fr=hit.fireRate;		--get firerate (so we recognize firemode, 450 = simple, 800 = burst mode)
		if fr==450 then				--if simple fire mode, shoot a flare
			g_gameRules:CreateExplosion(player.id,weaponId,0,pos,dir,1,1,1,1,"explosions.flare.night_time",1.6, 1, 1, 1);
		else
			if player.flareGunShot%2==0 then	--else shoot firework when it every 2nd shot = 1 click
				g_gameRules:CreateExplosion(player.id,weaponId,0,pos,dir,1,1,1,1,"misc.extremly_important_fx.celebrate",1.6, 1, 1, 1);
			end
		end
	end
end
SafeWriting.FuncContainer:LoadPlugin(FlareGun);	--and now just load our plugin
--to get flare gun, just simply do !spawn 1 FlareGun or !give item ... FlareGun or even it works with LevelDesigner :FlareGun
AddChatCommand("flaregun",function(self,player)
	GiveItem(player,"FlareGun");
	Chat:SendToTarget(player,"Here one FlareGun for you!");
end,nil,{AdminOnly=true;},"Gives you a flare gun");
