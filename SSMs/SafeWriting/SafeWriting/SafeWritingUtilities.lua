--Created 22.2.2013 as part of SSM SafeWriting project, this file contains Lua utilities which can be useful sometimes
--System.LogAlways("SafeWritingUtilities.lua loaded");
--CPPAPI.LoadSSMScript("Files/SafeWritingUTF8.lua");

Stack={_v={};_i=1;limit=0xFFFFFFFFFFFFFFFF;};
function Stack:Create()
	local stack={_v={};_i=1;limit=0xFFFFFFFFFFFFFFFF;};
	setmetatable(stack,self);
	self.__index=self;
	return stack;
end
function Stack:Push(...)
	for i,v in pairs({...}) do
		if self._i>self.limit then break; end
		self._v[self._i]=v;
		self._i=self._i+1;
	end
end
function Stack:Pop()
	if(self._i-1<=0)then
		return nil;
	end
	local v=self._v[self._i-1];
	self._v[self._i-1]=nil;
	self._i=self._i-1;
	return v;
end
function Stack:IsEmpty()
	return self._v[1]==nil;
end
function Stack:Limit(l) self.limit=l; end
function Stack:Top() return self._v[self._i-1]; end
function Stack:Front() return self._v[1]; end

Queue={_v={};_i=1;limit=0xFFFFFFFFFFFFFFFF;};
function Queue:Create()
	local queue={_v={};_i=1;limit=0xFFFFFFFFFFFFFFFF;};
	setmetatable(queue,self);
	self.__index=self;
	return queue;
end
function Queue:Push(...)
	for i,v in pairs({...}) do
		if self._i>self.limit then break; end
		self._v[self._i]=v;
		self._i=self._i+1;
	end
end
function Queue:Pop()
	if(self._i-1<=0)then
		return nil;
	end
	local v=self._v[1];
	for i=2,self._i do
		self._v[i-1]=self._v[i];
	end
	self._v[self._i-1]=nil;
	self._i=self._i-1;
	return v;
end
function Queue:IsEmpty()
	return self._v[1]==nil;
end
function Queue:Limit(l) self.limit=l; end
function Queue:Top() return self._v[self._i-1]; end
function Queue:Front() return self._v[1]; end

FuncQueue={_v={};_i=1;};
function FuncQueue:Create()
	local funcQueue={_v={};_i=1;};
	setmetatable(funcQueue,self);
	self.__index=self;
	return funcQueue;
end
function FuncQueue:Push(f,...)
	for i,v in pairs({...}) do
		self._v[self._i]={func=f;params={...}};
		self._i=self._i+1;
	end
end
function FuncQueue:Pop()
	if(self._i-1<=0)then
		return nil;
	end
	local v=self._v[1];
	for i=2,self._i do
		self._v[i-1]=self._v[i];
	end
	self._v[self._i-1]=nil;
	self._i=self._i-1;
	return v.func,v.params;
end
function FuncQueue:IsEmpty()
	return self._v[1]==nil;
end

FunctionsContainer={Funcs={};AllowedCategs={};ACC=0;}
function FunctionsContainer:Create()
	local container={Funcs={};AllowedCategs={};ACC=0;};
	setmetatable(container,self);
	self.__index=self;
	return container;
end
function FunctionsContainer:AddCategs(tbl)
	for i,v in pairs(tbl) do self.AllowedCategs[v]=true; self.ACC=self.ACC+1; end
end
function FunctionsContainer:AddFunc(func,name,pl)
	name=name:gsub("g_gameRules:","");
	local isallowed=self.AllowedCategs[name];
	if self.ACC==0 then isallowed=true; end
	if isallowed then
		if(not self.Funcs[name])then
			self.Funcs[name]={};
		end
		self.Funcs[name][#self.Funcs[name]+1]={func,pl};
	end
end
function FunctionsContainer:GetFuncs(name)
	return self.Funcs[name];
end
function FunctionsContainer:LoadPlugin(pl)
	for i,v in pairs(pl) do
		if type(v)=="function" then
			self:AddFunc(v,i,pl);
		end
	end
end
Translator={__Languages={};};
function Translator:Create()
	local t={__Languages={};};
	setmetatable(t,self);
	self.__index=self;
	return t;
end
function Translator:AddLanguage(lang,tbl)
	lang=lang:lower();
	self.__Languages[lang]={};
	MergeTables(self.__Languages[lang],tbl);
end
function Translator:Translate(lang,text)
	lang=lang:lower();
	if(self.__Languages[lang])then
		text=text:gsub("%[%[(.-)%]%]",function(i)
			if R and R.LANGUAGE and R[i] then i=R[i] end
			return self.__Languages[lang][i];
		end)
	end
	return text;
end
function __qt(p,m)
	if(SafeWriting.Settings.DisableTranslations)then
		p="en";
	end
	if(SafeWriting.Translator.__Languages[p])then
		return SafeWriting.Translator.__Languages[p][m] or "";
	else return "[["..m.."]]"; end
end
JL1Hash={Seed=1;};
function JL1Hash:Create(seed)
	local tbl={Seed=seed;};
	setmetatable(tbl,self);
	self.__index=self;
	self.Seed=seed;
	return tbl;
end
function JL1Hash:SetSeed(seed)
	self.Seed=seed;
end
function JL1Hash:bleft(num,p)
	return (num*2^p);
end
function JL1Hash:bright(num,p)
	return math.floor(num/2^p);
end
function JL1Hash:band(num,p)
	return num%(p+1);
end
function JL1Hash:Hash(text)
	local hash=0;
	local len=(text:len())+2;
	local c=0;
	local tmp=text.."jl";
	for i=0,len-1 do
		local c=string.byte(tmp,i+1);
		if(c%2==0)then c=c+256; end
		hash=hash+(c*(i+1)*self.Seed);
		if(i>0 and c>0)then
			if(i>c)then
				if(i%c==2)then
					hash=self:bright(hash,1);
				end
			else
				if(c%i==2)then
					hash=self:bright(hash,1);
				end
			end
		end
		hash=self:band(hash,0xFFFFFFFFFFFFFFFF);
	end
	return string.format("%x",hash);
end
ScheduleBase={
	Events={};
};
function ScheduleBase:New()
	local tbl={Events={}};
	setmetatable(tbl,self);
	self.__index=self;
	return tbl;
end
function ScheduleBase:Create()
	return self:New();
end
function ScheduleBase:AddEvent(timeout,event,params,_repeat)
	if (type(params)=="boolean" or type(params)=="number") and _repeat==nil then
		_repeat=params;
		params={};
	end
	params=params or {};
	self.Events[#self.Events+1]={os.clock(),timeout/1000,event,params,_repeat};
end
function ScheduleBase:Update()
	local i=1;
	local v;
	while i<=#self.Events do
		v=self.Events[i];
		if os.clock()-v[1]>v[2] then
			local event=v[3];
			local params=v[4];
			local rep=v[5];
			local nextR=rep;
			if rep and type(rep)=="number" then
				if rep<=1 then rep=false; end
				nextR=nextR-1;
			end
			if rep then
				self:AddEvent(v[2]*1000,event,params,nextR);
			end
			event(unpack(params));
			local be=#self.Events;
			table.remove(self.Events,i);
		else i=i+1; end
	end
end
function ScheduleBase:IsEmpty()
	return #self.Events==0;
end
function xmlfixstring(str)
	str=str:gsub("&","&amp;");
	str=str:gsub("\"","&quot;");
	return str;
end
function xmlgetstring(str)
	str=str:gsub("&quot;","\"");
	str=str:gsub("&amp;","&");
	return str;
end
function requestencode(str)
	str=str:gsub("[&]",string.char(0x80));
	str=str:gsub("[<]",string.char(0x81));
	str=str:gsub("[>]",string.char(0x82));
	return str;
end

function similartext_old(a,b)
	local delta=a:len()-b:len();
	local nullchr=string.char(0);
	if delta<0 then a=a..string.rep(nullchr,-delta); elseif delta>0 then b=b..string.rep(nullchr,delta); end
	local e,f=0,a:len();
	for i=1,f do
		local c,d=a:byte(i),b:byte(i);
		if c == d then e=e+1; end
	end
	return e*100/f;
end

function similartext(a, b)
    local len_a = #a
    local len_b = #b

    -- Create a 2D matrix
    local matrix = {}
    for i = 0, len_a do
        matrix[i] = {}
        matrix[i][0] = i
    end
    for j = 0, len_b do
        matrix[0][j] = j
    end

    -- Compute Levenshtein distance
    for i = 1, len_a do
        for j = 1, len_b do
            local cost = (a:sub(i,i) == b:sub(j,j)) and 0 or 1
            matrix[i][j] = math.min(
                matrix[i-1][j] + 1,      -- Deletion
                matrix[i][j-1] + 1,      -- Insertion
                matrix[i-1][j-1] + cost  -- Substitution
            )
        end
    end

    -- The distance is in the bottom-right cell
    local distance = matrix[len_a][len_b]
    return 100 / (1 + distance)
end

function CompileFile(file,folder)
	folder=folder or SafeWriting.AdditionalScriptsFolder;
	local out=folder..(file:gsub("[.]lua",".sfwc"));
	file=folder..file;
	local f,err=io.open(out,"wb");
	if f then
		local bc=loadfile(file);
		f:write(string.dump(bc));
		f:close();
	else print("Failed to open output stream in CompileFile"); end
end
function SpecialFormat(fmt,...)
	local ctr=1;
	local ctrend=#{...};
	while ctr<=ctrend do
		({...})[ctr]=string.gsub(({...})[ctr],"%%","%%%%");
		ctr=ctr+1;
	end
	local style=SafeWriting.Settings.Style or {};
	fmt=fmt:gsub("${t:(%w-)|([0-9])}",function(a,b)
		return "$"..(style[a] or b);
	end);
	return string.format(fmt,...);
end
function GetSpecialFormat(fmt,...)
	local ctr=1;
	local ctrend=#{...};
	while ctr<=ctrend do
		({...})[ctr]=string.gsub(({...})[ctr],"%%","%%%%");
		ctr=ctr+1;
	end
	return fmt,...;
end
function MergeTables(tbl1,tbl2)
	for i,v in pairs(tbl2) do
		if(type(v)=="table")then
			tbl1[i]=tbl1[i] or {};
			MergeTables(tbl1[i],tbl2[i]);
		else
			tbl1[i]=v;
		end
	end
end
function MergeFunctions(f1,f2,p1)
	local f3=f1;
	f1=function(...)
		f3(...);
		f2(...);
	end
	return f1;
end
function tonum(...)
	for i,v in ipairs({...}) do
		({...})[i]=tonumber(v);        
	end
	return ...;
end
function trim(str)
	str=str:gsub("%\n","");
	str=str:gsub("%\t","");
	str=str:gsub("%\r","");
	str=str:gsub("% ","");
	
	return str;
end
function trim_from(str,params)
	local ret=str;
	for i,v in pairs(params)do
		ret=ret:gsub(v,'');
	end
	return ret
end
function sfwstrfind(str,_substr,off)
	local pos=str:find(_substr,off or 1,true);
    return pos or -1;
end
function strcontains(str,_substr)
	return (sfwstrfind(str,_substr)>(-1));
end
function readjson(text)
	local text=text:gsub([["(.-)"[ ]*:[ ]*(.-)([,}])]],function(a,b,c) return string.format("[\"%s\"]=%s%s",a,b,c); end);
	text="return "..text..";";
	local t=assert(loadstring(text));
	return t();
end
function in_array(s,arr)
	for i,v in ipairs(arr)do
		if(v==s)then
			return true;
		end
	end
	return false;
end
function count(arr)
	local c=0;
	for i,v in pairs(arr) do c=c+1; end
	return c;
end
function GetDistance(a,b)
	local x,y,z=a.x-b.x,a.y-b.y,a.z-b.z;
	local c=math.sqrt(x^2+y^2);
	local d=math.sqrt(c^2+z^2);
	return d;
end
function torad(num)
	return num/(180/math.pi);
end
function todeg(num)
	return num*(180/math.pi);
end
function arr2str_fast(arr,stp,off)
	if(not off)then off=""; end
	if(not stp)then stp=""; end
	local t="";
	t=off..stp.." = {\n";
	for i,v in pairs(arr) do
		local val=tostring(v);
		if(type(v)=="string")then
			val="\""..v:gsub("[\"]","\\\"").."\"";
		elseif type(v)=="number" and v>100000 then val = string.format("%d", v); end
		local ival=tostring(i);
		if(type(i)=="string")then
			ival="\""..i:gsub("[\"]","\\\"").."\"";
		end
		if(type(v)=="table")then
			t=t..arr2str(v,"["..ival.."]",off.."\t");
		else
			t=t..(off.."\t["..ival.."] = "..val)..";\n";
		end
	end
	t=t..off.."};\n";
	return t;
end
function arr2str(arr,stp,off)
	if(not off)then off=""; end
	if(not stp)then stp=""; end
	local t="";
	t=off..stp.." = {\n";
	for i,v in _pairs(arr) do
		local val=tostring(v);
		if(type(v)=="string")then
			val="\""..v:gsub("[\"]","\\\"").."\"";
		elseif type(v)=="number" and v>100000 then val = string.format("%d", v); end
		local ival=tostring(i);
		if(type(i)=="string")then
			ival="\""..i:gsub("[\"]","\\\"").."\"";
		end
		if(type(v)=="table")then
			t=t..arr2str(v,"["..ival.."]",off.."\t");
		else
			t=t..(off.."\t["..ival.."] = "..val)..";\n";
		end
	end
	t=t..off.."};\n";
	return t;
end
function _pairs(arr)
	local c1=#arr;
	for i,v in pairs(arr) do c1=c1-1; end
	if c1==0 then return ipairs(arr); else return pairs(arr); end
end
function split(text,separator)
	local arr={};
	local idx=0;
	local skip=0;
	for i=1,string.len(text) do
		local c=text:sub(i,i+(string.len(separator)-1));
		if(c==separator)then
			skip=separator:len()-1;
			idx=idx+1;
		else
			if(skip==0)then
				arr[idx]=(arr[idx] or "")..text:sub(i,i);
			else
				skip=skip-1;
			end
		end
	end
	return arr;
end
function fsplit(str,a)
	local t={};
	for i in str:gmatch("([^"..a.."]+)") do
		t[#t+1]=i;
	end
	return t;
end
function fsplit0(str,a)
	local t={};
	local idx=0;
	for i in str:gmatch("([^"..a.."]+)") do
		t[idx]=i;
		idx=idx+1;
	end
	return t;
end
function CTableToLuaTable(tbl)
	local ntbl={};
	for i=0,#tbl do
		ntbl[i+1]=tbl[i];
	end
	return ntbl;
end
function CmdGetNameAndText(line)
	local params=split(line," ");
	params=CTableToLuaTable(params);
	local name=params[2];
	local text;
	if(#params>2)then
		text=table.concat(params," ",3);
	end
	return name,text;
end
function ClearString(str,name)
	if utf8clean then
		str=utf8clean(str);
	end
	local chars={
		[string.char(252)] = "u";
		[string.char(251)] = "u";
		[string.char(250)] = "u";
		[string.char(249)] = "u";
		[string.char(248)] = "r";
		[string.char(246)] = "o";
		[string.char(245)] = "o";
		[string.char(244)] = "o";
		[string.char(243)] = "o";
		[string.char(242)] = "n";
		[string.char(241)] = "n";
		[string.char(240)] = "d";
		[string.char(239)] = "d";
		[string.char(238)] = "i";
		[string.char(237)] = "i";
		[string.char(140)] = "S";
		[string.char(138)] = "S";
		[string.char(128)] = "e";
		[string.char(253)] = "y";
		[string.char(156)] = "s";
		[string.char(154)] = "s";
		[string.char(143)] = "Z";
		[string.char(142)] = "Z";
		[string.char(141)] = "T";
		[string.char(165)] = "A";
		[string.char(163)] = "L";
		[string.char(159)] = "z";
		[string.char(158)] = "z";
		[string.char(157)] = "t";
		[string.char(188)] = "L";
		[string.char(185)] = "a";
		[string.char(179)] = "l";
		[string.char(175)] = "Z";
		[string.char(204)] = "E";
		[string.char(202)] = "E";
		[string.char(201)] = "E";
		[string.char(200)] = "C";
		[string.char(199)] = "C";
		[string.char(198)] = "C";
		[string.char(197)] = "L";
		[string.char(196)] = "A";
		[string.char(195)] = "A";
		[string.char(194)] = "A";
		[string.char(193)] = "A";
		[string.char(192)] = "R";
		[string.char(191)] = "z";
		[string.char(190)] = "l";
		[string.char(220)] = "U";
		[string.char(219)] = "U";
		[string.char(218)] = "U";
		[string.char(217)] = "U";
		[string.char(216)] = "R";
		[string.char(214)] = "O";
		[string.char(213)] = "O";
		[string.char(212)] = "O";
		[string.char(211)] = "O";
		[string.char(210)] = "N";
		[string.char(209)] = "N";
		[string.char(208)] = "D";
		[string.char(207)] = "D";
		[string.char(206)] = "I";
		[string.char(205)] = "I";
		[string.char(236)] = "e";
		[string.char(234)] = "e";
		[string.char(233)] = "e";
		[string.char(232)] = "c";
		[string.char(231)] = "c";
		[string.char(230)] = "c";
		[string.char(229)] = "l";
		[string.char(228)] = "a";
		[string.char(227)] = "a";
		[string.char(226)] = "a";
		[string.char(225)] = "a";
		[string.char(224)] = "r";
		[string.char(221)] = "Y";

	};
	for i,v in pairs(chars) do
		str=str:gsub(i,v);
	end
	if name then
		str=VerifyName(str);
	end
	return str;
end
function VerifyName(meno)
	local povolene="abcdefghijklmnopqrstuvwxyz1235467890+-*/#[](){}<>.:;!?$^= ";
	local povolene_t={};
	local konecne_t={};
	local konecne="";
	for i=1,povolene:len() do
		povolene_t[povolene:sub(i,i)]=true;
	end
	local tm=meno:lower();
	for i=1,meno:len() do
		if(not povolene_t[tm:sub(i,i)])then
			konecne_t[i]='_';
		else
			konecne_t[i]=meno:sub(i,i);
		end
	end
	for i,v in pairs(konecne_t)do
		konecne=konecne..v;
	end
	return konecne;
end
function ParseDecIP(ip)
	local formacie={
		{
			regex="(%d+)-(%d+)-(%d+)-(%d+)[.]cm[.]vtr[.]net",
			poradie={4,3,2,1},
		},
		{
			regex="(%d+)[.](%d+)-(%d+)-(%d+)",
			poradie={2,3,4,1},
		},
		{
			regex="(%d+)[.](%d+)[.](%d+)[.](%d+)",
			poradie={1,2,3,4},
		},
		{
			regex="(%d+)-(%d+)-(%d+)-(%d+)",
			poradie={1,2,3,4},
		},
		{
			regex="(%d+)[.-](%d+)[-.][a-zA-Z0-9]+[.-](%d+)[.-](%d+)",
			poradie={4,3,2,1},
		},
		{
			regex="(%d+)[x](%d+)[x](%d+)[x](%d+)",
			poradie={1,2,3,4},
		},
		{
			regex="([0-9][0-9][0-9])([0-9][0-9][0-9])([0-9][0-9][0-9])([0-9][0-9][0-9])",
			poradie={1,2,3,4},
		},
	};
	local fip="";
	for i,v in pairs(formacie)do
		local regex=v.regex;
		local poradie=v.poradie;
		local pts={string.match(ip,regex)};
		local fpts={};
		if(#pts==4)then
			for j,w in pairs(poradie) do
				fip=fip.."."..tonumber(pts[w]);
			end
		break;
		end
	end
	if(not IsDllLoaded() and not IsDllLoaded100())then
		if(fip:len()==0)then fip=SafeWritingCall("getip",ip); if(IsRealIP(fip))then return fip; else return nil; end; end
	end
	if(fip:len()>0)then fip=fip:sub(2); else return nil; end
	return fip;
end
function IsRealIP(ip)
	if string.match(ip,"^(%d)[.](%d)[.](%d)[.](%d)$") then return false; end
	return string.match(ip,"^(%d+)[.](%d+)[.](%d+)[.](%d+)$");
end
function _GetIP(host)
	local isHex=false;
	local isKnown=false;
	local finalIP=nil;
	local ipParts={};
	local isExtraDec=false;
	local hexRange={
		["0"]=true;
		["1"]=true;
		["2"]=true;
		["3"]=true;
		["4"]=true;
		["5"]=true;
		["6"]=true;
		["7"]=true;
		["8"]=true;
		["9"]=true;
		["A"]=true;
		["B"]=true;
		["C"]=true;
		["D"]=true;
		["E"]=true;
		["F"]=true;
	};
	if(string.match(host,"([0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9][0-9])"))then
		isExtraDec=true;
	end
	if(not isExtraDec)then
		for i=1,host:len() do
			local tmpIP=host:sub(i,i+7);
			local valid=true;
			for j=1,8 do
				if(not hexRange[tmpIP:sub(j,j):upper()])then
					valid=false;
					break;
				end
			end
			if(tmpIP:len()<8)then
				break;
			end
			if(valid)then
				ipParts={};
				for i=1,8,2 do
					ipParts[#ipParts+1]=tonumber(tmpIP:sub(i,i+1),16);
					isHex=true;
				end
				break;
			end
		end
	end
	if(not isHex)then
		local finip=ParseDecIP(host) or host;
		ipParts[1],ipParts[2],ipParts[3],ipParts[4]=string.match(finip,"(%d+).(%d+).(%d+).(%d+)");
	end
	if(#ipParts>0)then
		finalIP="";
		for i,v in pairs(ipParts)do
			finalIP=finalIP..v..".";
		end
		finalIP=finalIP:sub(1,finalIP:len()-1);
		return finalIP;
	end
	return host;
end
function average(...)
	local pars={...};
	if #pars==1 and type(pars[1])=="table" then
		if #pars[1]>0 then
			local avg=0;
			for i,v in ipairs(pars[1]) do avg=avg+v; end
			avg=avg/#pars[1];
			return avg;
		else return 0; end
	else
		if #pars>0 then
			local avg=0;
			for i,v in ipairs(pars) do
				avg=avg+v;
			end
			avg=avg/#pars;
			return avg;
		else return 0; end
	end
end
function CalcDistance2D_XY(a,b)
	local c,d=a.x-b.x,a.y-b.y;
	return math.sqrt(c^2+d^2);
end
function CalcDistance2D_XZ(a,b)
	local c,d=a.x-b.x,a.z-b.z;
	return math.sqrt(c^2+d^2);
end
function CalcDistance2D_YZ(a,b)
	local c,d=a.y-b.y,a.z-b.z;
	return math.sqrt(c^2+d^2);
end
function CalcDistance3D(a,b)
	local c,d,e=a.y-b.y,a.y-b.y,a.z-b.z;
	return math.sqrt(c^2+d^2+e^2);
end
function CreateClass(name,derivatedFrom)
	KnownClasses=KnownClasses or {};
	KnownClasses[name]=derivatedFrom or "";
	_G[name]={ __className__=name; __motherName__=derivatedFrom or ""  };
	return _G[name];
end
function itype(a)
	local tp=type(a);
	if tp=="table" then
		if a.__className__ then
			return a.__className__;
		end
	end
	return tp;
end
function str_replace_i(t,w,i)
	local pos={};
	local orig,ow,cnt=t,w,0;
	t=t:lower();
	w=w:lower();
	local p=string.find(t,w,nil,true);
	while p do
		pos[#pos+1]=p;
		p=string.find(t,w,p+1,true);
	end
	local start=1;
	for j,v in ipairs(pos) do
		orig=orig:sub(start,v-1)..i..orig:sub(v+w:len());
		start=v;
	end
	return orig;
end
function pmcreate(name,body,...)
	local fullPath=name.."?";
	for i,v in pairs({...}) do
		local tp=itype(v);
		fullPath=fullPath.."@"..tp;
	end
	_G[fullPath]=body;
end
function pmcall(name,...)
	local fullPath=name.."?";
	for i,v in pairs({...}) do
		local tp=itype(v);
		fullPath=fullPath.."@"..tp;
	end
	local f=_G[fullPath];
	if not f then
		print("Call to unexisting function "..fullPath.."!");
	else
		f(...);
	end
end
function in_range(num1,num2,c)
	return math.abs(num1-num2)<c;
end
function looksAt(pos1,pos2,angle1,thrDeg)
	thrDeg=thrDeg or 1;
	thrDeg=torad(thrDeg);
	local angles=calcViewAngles(pos1,pos2);
	return (in_range(angles.x,angle1.x,thrDeg) and in_range(angles.z,angle1.z,thrDeg));
end
function calcViewAngles(pos1,pos2)
	local x,y,z=pos2.x-pos1.x,pos2.y-pos1.y,pos2.z-pos1.z;
	local d=math.sqrt(x^2+y^2);
	local angles={
		x=math.atan2(z,d);
		y=0;
		z=-math.atan2(x,y);
	};
	return angles;
end
function Derive(a)
	local b={};
	for i,v in pairs(a) do
		b[i]=v;
	end
	return b;
end
derive=Derive;
AdvancedPlugin={
	OutFolder="";
	OutFile="";
	Name=nil;
	TableName="";
	Data={};
};
function AdvancedPlugin:GetPath(path) return self.OutFolder..path; end
function AdvancedPlugin:Init(outfile,outf)
	self.Name=self:GetName();
	self.OutFolder=outf or SafeWriting.GlobalStorageFolder;
	self.OutFile=outfile or (self.Name.."_DATA.lua");
	self.TableName=self.Name..".Data";
	self:LoadData();
end
function AdvancedPlugin:LoadData()
	local f=loadfile(self:GetPath(self.OutFile));
	if f then
		assert(f)();
	end
end
function AdvancedPlugin:GetName()
	if self.Name then return self.Name; end
	for i,v in pairs(_G) do if v==self then return i; end; end
end
function AdvancedPlugin:SaveData()
	local out=self:GetPath(self.OutFile);
	local f,err=io.open(out,"w");
	if f then
		f:write(arr2str(self.Data,self.TableName));
		f:close();
	end
end
function AdvancedPlugin:New(...)
	local plg={
		OutFolder="";
		OutFile="";
		Name=nil;
		TableName="";
		Data={};
	};
	setmetatable(plg,self);
	plg.__IDENTIFIER__=math.random();
	self.__index=self;
	return plg;
end
function AdvancedPlugin:Load() SafeWriting.FuncContainer:LoadPlugin(self); end

if not R then
CompatEn = {
		LANGUAGE="Jazyk",
	LANGUAGE_OBJECT="Jazyk Akuzativ",
	LANGUAGE_WAS_CHANGED_TO="Jazyk bol zmeneny na",
	WELCOME="Vitaj",
	SPAWN_INFO_1="SpawnInfo1",
	SPAWN_INFO_2="SpawnInfo2",
	SPAWN_INFO_3="SpawnInfo3",
	SPAWN_INFO_4="SpawnInfo4",
	SPAWN_INFO_5="SpawnInfo5",
	ONLY_ON_PS="IbaPS",
	YOU_NEED_POINTS="Potrebujes___Bodov",
	ENTER_VALID_PLAYER="Zadaj platneho hraca",
	ENTER_TEXT="Zadaj text",
	PLAYER_NOT_FOUND="Hrac ___ nebol najdeny",
	MESSAGE_SENT_TO_PLAYER="Sprava bola poslana hracovi",
	COMMAND_BLOCKED="Tento prikaz je zablokovany",
	NOT_ENOUGH_SPACE="Nedostatok miesta v inventari",
	NO_DESCRIPTION="Ziadny popis",
	UNKNOWN="nezname",
	SCORE_WAS_RESET="Skore bolo anulovane",
	VEHICLE="Vozidlo",
	LOCKED="zamknute",
	UNLOCKED="odomknute",
	YOU_MUST_BE_DRIVER="Musis byt vodic",
	YOU_MUST_BE_IN_VEHICLE="Musis byt vo vozidle",
	TIME_NOW="Aktualny cas",
	NOT_ENOUGH_POINTS="Nedostatok bodov",
	ENTER_VALID_VALUE_PP="Zadaj platnu ciastku",
	VALUE_MUST_BE_HIGHER_THAN_0="Zadana ciastka musi byt vacsia ako 0",
	NO_VOTING_IN_PROGRESS="Neprebieha hlasovanie",
	VOTING_IN_PROGRESS="Prebieha hlasovanie",
	SUCCESSFULY_VOTED_FOR="Uspesne si hlasoval za",
	YES="ano",
	NO="nie",
	ALREADY_VOTED="Uz si hlasoval",
	ENTER_VALID_DISTANCE="Zadaj platnu vzdialenost",
	ENTER_VALID_CLASS="Zadaj platne meno triedy",
	ENTER_VALID_VALUE="Zadaj platnu hodnotu",	
	TIME_WAS_CHANGED_TO	="Cas bol zmeneny",
	GRAVITATION_WAS_CHANGED_TO="Gravitacia bola zmenena",
	ENTER_VALID_COMMAND="Zadaj platny prikaz",
	SUCCESSFULY_DONE="Uspesne vykonane",
	ENTER_VALID_ACTION="Zadaj platnu akciu",
	ALREADY_JAILED="Tento hrac je uz vazneny",
	YOU_WERE_JAILED="Bol si vzaty do basy",
	YOU_WERE_RELEASED="Bol si prepusteny z basy",
	NOT_JAILED="Tento hrac nie je vazneny",
	REASON="dovod",
	THIS_COMMAND_IS="Tento prikaz je",
	ADMIN_ONLY="len pre adminov",
	ADMIN_MOD_ONLY="len pre adminov a moderatorov",
	MOD_ONLY="len pre moderatorov",
	PREMIUM_ONLY="len pre premium hracov",
	COMMAND_DOESNT_EXIST="Prikaz ___ neexistuje",
	PING_TOO_HIGH="Tvoj ping je privysoky",
	WARNING="Varovanie",
	THIS_VEHICLE_IS_LOCKED="Toto vozidlo je zamknute",
	PLEASE_DONT_SWEAR="Nenadavaj",
	YOU_ARE_MUTED="Si umlcany",
	YOU_ARE_JAILED="Si vazneny",
	USABLE_ONLY_N_SECONDS="Iba___sekund",
	KILLS="Zabitia",
	DEATHS="Smrti",
	BEST_DSG1_KILL="Najdlhsie zabitie s DSG1",
	PLAYED_TIME="Nahrany cas",
	YOU_DONT_HAVE_STATS_YET="Nemas statistiky",
	HOURS="hodin",
	MINUTES="minut",
	AND="a",
	NAME_NOW="Meno teraz",
	OPEN_CONSOLE="Otvor konzolu",
	PLAYED_ADDED_BETWEEN="Hrac ___ bol pridany medzi",
	PLAYER_REMOVED_FROM="Hrac ___ bol odobrany z",
	ADMINS="adminov",
	MODS="moderatorov",
	PREMIUMS="premium hracov",
	ACCEPT_COMMAND="Potvrd prikaz",
	ITEM_LOCKED="Tato polozka je zablokovana",	
	TRANSLATED_BY="Prelozil",
	DID_YOU_MEAN="Mali ste na mysli"
};
R = CompatEn;
end
