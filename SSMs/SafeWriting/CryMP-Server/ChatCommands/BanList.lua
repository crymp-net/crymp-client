AddChatCommand("banlist", function(self, player)
	Console:SendToTarget(player, "$5%3s $8%-26s $9%9s $8%-15s $9%s", "ID", "Name", "Profile", "Banned by", "Reason");
	for i,v in pairs(SafeWriting.Bans) do
		local name,ip,host,profile,reason,bantime,bannedBy,expire,hwid=unpack(v);
		Console:SendToTarget(player, "$5%3d $8%-26s $9%9s $8%-15s $9%s", i, name, tostring(profile), bannedBy, reason);
	end
	self:OpenConsole(player);
end, {}, {AdminModOnly=true; });
AddChatCommand("unban", function(self, player, id)
	if id<=0 or id>#SafeWriting.Bans then
		return Chat:SendToTarget(player, "enter valid ID");
	end
	table.remove(SafeWriting.Bans, id);
	local file,err=io.open(SafeWriting.GlobalStorageFolder.."Bans.lua","w");
	if(file)then
		file:write(arr2str(SafeWriting.Bans,"SafeWriting.Bans"));
		file:close();
	end
end, {INT}, {AdminModOnly=true;}, "unbans player, usage: !unban <id> (id from banlist)");