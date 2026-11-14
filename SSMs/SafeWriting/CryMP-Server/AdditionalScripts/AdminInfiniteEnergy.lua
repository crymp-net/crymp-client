function AdminInfinite_OnUpdate(player)
	if(IsAdmin(player))then
		if(GetProtectionKind(player)==1)then
			player.actor:SetNanoSuitEnergy(200);
		end
	end
end
SafeWriting.FuncContainer:AddFunc(AdminInfinite_OnUpdate,"UpdatePlayer");