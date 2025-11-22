SfwUpdates={};
SfwUpdates["2.0.0"]={Index=1;};
SfwUpdates["2.0.0"].Update=function(self)
	
end
SfwUpdates["2.0.1"]={Index=2;};
SfwUpdates["2.0.1"].Update=function(self)
	
end
SfwUpdates["2.0.2"]={Index=3;};
SfwUpdates["2.0.2"].Update=function(self)
	
end
SfwUpdates["2.1.0"]={Index=4;};
SfwUpdates["2.1.0"].Update=function(self)

end
SfwUpdates["2.1.1"]={Index=5;};
SfwUpdates["2.1.1"].Update=function(self)
	local file,err=io.open(SafeWriting.GlobalStorageFolder.."Permabans.txt","r");
	local bans={};
	if file then
		for line in file:lines() do
			local name,ip,host,profile,reason,bantime,bannedBy=string.match(line,"name:\"(.*)\" ip:\"(.*)\" host:\"(.*)\" profile:\"(.*)\" reason:\"(.*)\" time:\"(.*)\" bannedby:\"(.*)\"");
			bans[#bans+1]={name,ip,host,profile,reason,bantime,bannedBy};
		end
		file:close();
		file,err=io.open(SafeWriting.GlobalStorageFolder.."Permabans.txt","w");
		if(file)then
			for i,v in ipairs(bans) do
				local name,ip,host,profile,reason,bantime,bannedBy=unpack(v);
				local line=SpecialFormat("name:%s\t ip:%s\t host:%s\t profile:%s\t reason:%s\t time:%s\t bannedby:%s\t expire:0",name,ip,host,profile,reason,bantime,bannedBy);
				file:write(line.."\n");
			end
			file:close();
		end
	end
	SfwLog("Successfuly updated Permabans.txt, now temp bans are available")
end
SfwUpdates["2.2.0"]={Index=6;};
SfwUpdates["2.2.0"].Update=function(self)
	local file,err=io.open(SafeWriting.GlobalStorageFolder.."Permabans.txt","r");
	if(file)then
		for line in file:lines() do
			local name,ip,host,profile,reason,bantime,bannedBy,expire=string.match(line,"name:(.*)\t ip:(.*)\t host:(.*)\t profile:(.*)\t reason:(.*)\t time:(.*)\t bannedby:(.*)\t expire:(.*)");
			SafeWriting.Bans[#SafeWriting.Bans+1]={name,ip,host,profile,reason,bantime,bannedBy,expire};
		end
		local f,err=io.open(SafeWriting.GlobalStorageFolder.."Bans.lua","w");
		if(f)then
			f:write(arr2str(SafeWriting.Bans,"SafeWriting.Bans"));
			f:close();
		end
		file:close();
		os.remove(SafeWriting.GlobalStorageFolder.."Permabans.txt");
	end
end
function GetIndex(m)
	for i,v in pairs(SfwUpdates) do
		if(i==m)then
			return v.Index;
		end
	end
	return count(SfwUpdates)-1;
end
function CreateSortedTable()
	local tbl={};
	for i,v in pairs(SfwUpdates) do
		tbl[v.Index]=v;
		tbl[v.Index].Version=i;
	end
	return tbl;
end
function __Update(start)
	local updates=CreateSortedTable();
	local lastVer="";
	for i,v in pairs(updates) do
		if(v.Index>start)then
			v:Update();
			SfwLog("Successfuly actualized to "..v.Version);
			lastVer=v.Version;
		end
	end
	return lastVer;
end
function BeginUpdates(m)
	m=SafeWriting.TempVersion;
	if(m==SafeWriting.Version)then
		SfwLog("SafeWriting is up to date");
	else
		SfwLog("Beginning update from "..SafeWriting.TempVersion.." to "..SafeWriting.Version);
		__Update(GetIndex(m));
	end
	SafeWriting.TempVersion=SafeWriting.Version;
	local f,err=io.open(SafeWriting.MainFolder.."sfw.cfg","w");
	if(f)then
		f:write("SfwDir "..SafeWriting.__MainFolder.."\r\n");
		f:write("SfwGameVer "..SafeWriting.GameVersion.."\r\n");
		f:write("SfWSetTempVer "..SafeWriting.Version.."\r\n");
		f:close();
	end
end
function GetTempVer()
	return SafeWriting.TempVersion or "1.9.10";
end