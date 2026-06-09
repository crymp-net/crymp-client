--Created on 12th of august as part of SSM SafeWriting project by Zi;
--This script is able to generate .htm page, which shows how many players joined, from what country the most and in what hour.
--Later it will maybe be able to do advanced stuff, but anyway :)
--The result is located in ServerFolder/Storage/statistics_Month_Year.htm, for example statistics_8_2013 means statistics from august of 2013
_S_Enabled=true;
if not _S then
	_S={
		Visits={};
		TmpFiles={};
		Months={31,28,31,30,31,30,31,31,30,31,30,31,};
		MonthNames={"january","february","march","april","may","june","july","august","september","october","november","december"};
		Date={};
		Graphs={};
		OutFolder="";
		Inited=false;
		LastSave=-121;
		HomeCountry=SafeWriting.Settings.HomeCountry or "sk";
		Countries={};
		IgnoreUnkownCountries=true;	--disables things like: commercial, aerolines... in visits by country graphs (ignores it) when true
	};
end
function date(ts) return os.date(ts or "*t"); end
function offset(text,off) return string.rep("\t",off)..text; end
function _S:GetPath(path) return self.OutFolder..path; end
function _S:Init(outf)
	outf=outf or "";
	self.Inited=true;
	self.Date=os.date("*t");
	self.OutFolder=outf;
	local date=self.Date;
	if(date.year%4==0)then self.Months[2]=29; end
	for n=0,23 do self.Visits[n]={}; end
	if not self.StartMonth then
		self.StartMonth=date.month;
	end
	local try=loadfile(self:GetPath(string.format("___statistics_%d_%d.lua",date.month,date.year)));
	if try then
		assert(try)()
		for i,v in pairs(_G["_S_V_"..string.format("%d_%d",date.month,date.year)]) do
			_S.Visits[i]=v;
		end
	end
	self.Countries={
		["aero"]="air-transport industry",
		["asia"]="Asia-Pacific region",
		["biz"]="business",
		["cat"]="Catalan",
		["com"]="commercial",
		["coop"]="cooperatives",
		["info"]="information",
		["int"]="international organizations",
		["jobs"]="companies",
		["mobi"]="mobile devices",
		["museum"]="museums",
		["name"]="individuals, by name",
		["net"]="network",
		["org"]="organization",
		["post"]="postal services",
		["pro"]="professions",
		["tel"]="Internet communication services",
		["travel"]="travel and tourism industry related sites",
		["xxx"]="Porn",
		 
		["ac"]="Ascension Island";
		["ad"]="Andorra";
		["ae"]="United Arab Emirates";
		["af"]="Afghanistan";
		["ag"]="Antigua and Barbuda";
		["ai"]="Anguilla";
		["al"]="Albania";
		["am"]="Armenia";
		["an"]="Netherlands Antilles)";
		["ao"]="Angola";
		["aq"]="Antarctica";
		["ar"]="Argentina";
		["as"]="American Samoa";
		["at"]="Austria";
		["au"]="Australia";
		["aw"]="Aruba";
		["ax"]="Aland Islands";
		["az"]="Azerbaijan";
		["ba"]="Bosnia and Herzegovina";
		["bb"]="Barbados";
		["bd"]="Bangladesh";
		["be"]="Belgium";
		["bf"]="Burkina Faso";
		["bg"]="Bulgaria";
		["bh"]="Bahrain";
		["bi"]="Burundi";
		["bj"]="Benin";
		["bl"]="Saint Barthelemy";
		["bm"]="Bermuda";
		["bn"]="Brunei Darussalam";
		["bo"]="Bolivia";
		["bq"]="Bonaire, Sint Eustatius and Saba";
		["br"]="Brazil";
		["bs"]="Bahamas";
		["bt"]="Bhutan";
		["bv"]="Bouvet Island";
		["bw"]="Botswana";
		["by"]="Belarus";
		["bz"]="Belize";
		["ca"]="Canada";
		["cc"]="Cocos Islands";
		["cd"]="DR Congo";
		["cf"]="Central African Republic";
		["cg"]="Republic of the Congo";
		["ch"]="Switzerland";
		["ci"]="Cote d'Ivoire";
		["ck"]="Cook Islands";
		["cl"]="Chile";
		["cm"]="Cameroon";
		["cn"]="China";
		["co"]="Colombia";
		["cr"]="Costa Rica";
		["cu"]="Cuba";
		["cv"]="Cape Verde";
		["cw"]="Curaçao";
		["cx"]="Christmas Island";
		["cy"]="Cyprus";
		["cz"]="Czech Republic";
		["de"]="Germany";
		["dj"]="Djibouti";
		["dk"]="Denmark";
		["dm"]="Dominica";
		["do"]="Dominican Republic";
		["dz"]="Algeria";
		["ec"]="Ecuador";
		["ee"]="Estonia";
		["eg"]="Egypt";
		["eh"]="Western Sahara";
		["er"]="Eritrea";
		["es"]="Spain";
		["et"]="Ethiopia";
		["eu"]="European Union";
		["fi"]="Finland";
		["fj"]="Fiji";
		["fk"]="Falkland Islands";
		["fm"]="Micronesia";
		["fo"]="Faroe Islands";
		["fr"]="France";
		["ga"]="Gabon";
		["gb"]="United Kingdom";
		["gd"]="Grenada";
		["ge"]="Georgia";
		["gf"]="French Guiana";
		["gg"]="Guernsey";
		["gh"]="Ghana";
		["gi"]="Gibraltar";
		["gl"]="Greenland";
		["gm"]="Gambia";
		["gn"]="Guinea";
		["gp"]="Guadeloupe";
		["gq"]="Equatorial Guinea";
		["gr"]="Greece";
		["gs"]="South Georgia and the South Sandwich Islands";
		["gt"]="Guatemala";
		["gu"]="Guam";
		["gw"]="Guinea-Bissau";
		["gy"]="Guyana";
		["hk"]="Hong Kong";
		["hm"]="Heard Island and McDonald Islands";
		["hn"]="Honduras";
		["hr"]="Croatia";
		["ht"]="Haiti";
		["hu"]="Hungary";
		["id"]="Indonesia";
		["ie"]="Ireland";
		["il"]="Israel";
		["im"]="Isle of Man";
		["in"]="India";
		["io"]="British Indian Ocean Territory";
		["iq"]="Iraq";
		["ir"]="Iran";
		["is"]="Iceland";
		["it"]="Italy";
		["je"]="Jersey";
		["jm"]="Jamaica";
		["jo"]="Jordan";
		["jp"]="Japan";
		["ke"]="Kenya";
		["kg"]="Kyrgyzstan";
		["kh"]="Cambodia";
		["ki"]="Kiribati";
		["km"]="Comoros";
		["kn"]="Saint Kitts and Nevis";
		["kp"]="North Korea";
		["kr"]="South Korea";
		["kw"]="Kuwait";
		["ky"]="Cayman Islands";
		["kz"]="Kazakhstan";
		["la"]="Laos";
		["lb"]="Lebanon";
		["lc"]="Saint Lucia";
		["li"]="Liechtenstein";
		["lk"]="Sri Lanka";
		["lr"]="Liberia";
		["ls"]="Lesotho";
		["lt"]="Lithuania";
		["lu"]="Luxembourg";
		["lv"]="Latvia";
		["ly"]="Libya";
		["ma"]="Morocco";
		["mc"]="Monaco";
		["md"]="Moldova";
		["me"]="Montenegro";
		["mf"]="Saint Martin";
		["mg"]="Madagascar";
		["mh"]="Marshall Islands";
		["mk"]="Macedonia";
		["ml"]="Mali";
		["mm"]="Myanmar";
		["mn"]="Mongolia";
		["mo"]="Macao";
		["mp"]="Northern Mariana Islands";
		["mq"]="Martinique";
		["mr"]="Mauritania";
		["ms"]="Montserrat";
		["mt"]="Malta";
		["mu"]="Mauritius";
		["mv"]="Maldives";
		["mw"]="Malawi";
		["mx"]="Mexico";
		["my"]="Malaysia";
		["mz"]="Mozambique";
		["na"]="Namibia";
		["nc"]="New Caledonia";
		["ne"]="Niger";
		["nf"]="Norfolk Island";
		["ng"]="Nigeria";
		["ni"]="Nicaragua";
		["nl"]="Netherlands";
		["no"]="Norway";
		["np"]="Nepal";
		["nr"]="Nauru";
		["nu"]="Niue";
		["nz"]="New Zealand";
		["om"]="Oman";
		["pa"]="Panama";
		["pe"]="Peru";
		["pf"]="French Polynesia";
		["pg"]="Papua New Guinea";
		["ph"]="Philippines";
		["pk"]="Pakistan";
		["pl"]="Poland";
		["pm"]="Saint Pierre and Miquelon";
		["pn"]="Pitcairn";
		["pr"]="Puerto Rico";
		["ps"]="Palestinian Territory";
		["pt"]="Portugal";
		["pw"]="Palau";
		["py"]="Paraguay";
		["qa"]="Qatar";
		["re"]="Reunion";
		["ro"]="Romania";
		["rs"]="Serbia";
		["ru"]="Russian Federation";
		["rw"]="Rwanda";
		["sa"]="Saudi Arabia";
		["sb"]="Solomon Islands";
		["sc"]="Seychelles";
		["sd"]="Sudan";
		["se"]="Sweden";
		["sg"]="Singapore";
		["sh"]="Saint Helena";
		["si"]="Slovenia";
		["sj"]="Svalbard and Jan Mayen";
		["sk"]="Slovakia";
		["sl"]="Sierra Leone";
		["sm"]="San Marino";
		["sn"]="Senegal";
		["so"]="Somalia";
		["sr"]="Suriname";
		["ss"]="South Sudan";
		["st"]="Sao Tome and Principe";
		["su"]="Soviet Union";
		["sv"]="El Salvador";
		["sx"]="Sint Maarten";
		["sy"]="Syrian Arab Republic";
		["sz"]="Swaziland";
		["tc"]="Turks and Caicos Islands";
		["td"]="Chad";
		["tf"]="French Southern Territories";
		["tg"]="Togo";
		["th"]="Thailand";
		["tj"]="Tajikistan";
		["tk"]="Tokelau";
		["tl"]="Timor-Leste";
		["tm"]="Turkmenistan";
		["tn"]="Tunisia";
		["to"]="Tonga";
		["tp"]="Portuguese Timor";
		["tr"]="Turkey";
		["tt"]="Trinidad and Tobago";
		["tv"]="Tuvalu";
		["tw"]="Taiwan";
		["tz"]="Tanzania";
		["ua"]="Ukraine";
		["ug"]="Uganda";
		["uk"]="United Kingdom";
		["um"]="United States Minor Outlying Islands";
		["us"]="United States";
		["uy"]="Uruguay";
		["uz"]="Uzbekistan";
		["va"]="Vatican";
		["vc"]="Saint Vincent and the Grenadines";
		["ve"]="Venezuela";
		["vg"]="Virgin Islands, British";
		["vi"]="Virgin Islands, U.S.";
		["vn"]="Vietnam";
		["vu"]="Vanuatu";
		["wf"]="Wallis and Futuna";
		["ws"]="Samoa";
		["ye"]="Yemen";
		["yt"]="Mayotte";
		["za"]="South Africa";
		["zm"]="Zambia";
		["zw"]="Zimbabwe";
	};
end
function _S:Save()
	local f,err=io.open(self:GetPath(string.format("___statistics_%d_%d.lua",date().month,date().year)),"w");
	if not f then print("Error when opening the file!"); return; end
	f:write(arr2str(_S.Visits,"_S_V_"..date().month.."_"..date().year));
	f:close();
end
function _S:Remove()
	--...
end
function _S:segadd(title,text,center)
	center=center or false;
	text=[[
			<h1 class="highlight">]]..(center and "<center>" or "")..title..(center and "</center>" or "")..[[</h1>
			<div class="description">
]]..(center and "<center>" or "")..text..(center and "</center>" or "")..[[
			</div>]];
	return text;
end
function _S:subsegadd(title,text,center)
	center=center or false;
	text=[[
			<h2 class="highlight_sub">]]..(center and "<center>" or "")..title..(center and "</center>" or "")..[[</h2>
]]..(center and "<center>" or "")..text..(center and "</center>" or "");
	return text;
end
function _S:guvph(hr)	--get unique visits per hour
	local ctr=0;
	local tmp={};
	for i,v in ipairs(self.Visits[hr]) do
		if #v==3 then
			if not tmp[v[2]] and not tmp[v[3]] then ctr=ctr+1; end
			tmp[v[2]]=true;
			tmp[v[3]]=true;
		end
	end
	return ctr;
end
function _S:guv()	--get unique visits in all month
	local ctr=0;
	local tmp={};
	for j,w in ipairs(self.Visits) do
		for i,v in pairs(w) do
			if #v==3 then
				if not tmp[v[2]] and not tmp[v[3]] then ctr=ctr+1; end
				tmp[v[2]]=true;
				tmp[v[3]]=true;
			end
		end
	end
	return ctr;
end
function _S:GenerateHTML(out)
	out=out or self:GetPath(string.format("statistics_%d_%d.htm",date().month,date().year));
	self:PrepareData();
	local template=[[
<!doctype html5>
<html>
	<head>
		<title>Statistics for month ]]..self.MonthNames[self.Date.month]..[[</title>
		<script type="text/javascript" src="https://www.google.com/jsapi"></script>
		<script>
			%s		</script>
		<style>
			*{
				font-family: Helvetica,Verdana,Arial;
			}
			h1{
				font-family: Helvetica,Verdana,Arial;
				color:#004050;
				margin: 0;
			}
			h2,h3{
				font-family: Helvetica,Verdana,Arial;
				color:#008090;
				margin: 0;
			}
			.highlight_sub{
				background:#f0f0f0;
			}
			.highlight{
				background:#d0e0f0;
			}
			.highlight_title{
				background:#dfefff;
			}
			.highlight_dark{
				background:#c0d0e0;
				text-align: center;
			}
			.description{
				font-family: Helvetica,Verdana,Arial; 
				background:#f7f7f7;
				color:#001030;
				font-size:16px;
				border-left:1px solid #d0e0f0;
				padding-left:5px;
			}
			.center{
				text-align: center;
			}
			.content{
				margin:0 auto;
				width: 1080px;
			}
			a:visited,a:active,a{
				color:#004050;
			}
			.code{
				background:#f7f7ff;
				color:#000080;
				margin: 0;
			}
			.red{
				color: #e00000;
			}
			.blue{
				color: #0000e0;
			}
			.green{
				color: #008000;
			}
			.violet{
				color: #800080;
			}
			.gray{
				color: #909090;
			}
			table{
				border-collapse: collapse;
			}
			table tr th{
				min-width:100px;
				text-align:left;
			}
		</style>
	</head>
	<body>
		<h1 class="highlight_title"><div class="center">Statistics for month ]]..self.MonthNames[self.Date.month]..[[</div></h1>
		<div class="content">
			<h3 class="highlight_dark">%s</h3>
			%s
		</div>
	</body>
	<!-- autogenerated by SSM SafeWriting -->
</html>]];
	local sc={};
	local graphs=[[
google.load("visualization", "1", {packages:["corechart"]});
			google.load("visualization", "1", {packages:["geochart"]});
			google.setOnLoadCallback(DrawCharts);
			function DrawCharts(){
]];
	for i,v in pairs(self.Graphs) do
		local title,data,_out,desc,options,func=unpack(v);
		graphs=graphs.."\t\t\t\tvar data_"..i.."=google.visualization.arrayToDataTable([\n\t\t\t\t\t["--; '"..desc[1].."','"..desc[2].."'],\n";
		for i,v in pairs(desc) do
			graphs=graphs.."'"..v:gsub("%'","\\'").."',";
		end
		graphs=graphs.."],\n";
		for j,w in pairs(data) do
			graphs=graphs.."\t\t\t\t\t[";
			graphs=graphs.."'"..tostring(j):gsub("%'","\\'").."',";
			if type(w)=="number" then
				graphs=graphs..w..",";
			elseif type(w)=="table" then
				for k,q in pairs(w) do
					if type(q)=="number" then
						graphs=graphs..q..",";
					elseif type(q)=="string" then
						graphs=graphs.."'"..q:gsub("%'","\\'").."',";
					end
				end
			elseif type(w)=="string" then
				graphs=graphs.."'"..w:gsub("%'","\\'").."',";
			end
			graphs=graphs.."],\n";
		end
		graphs=graphs.."				]);\n";
		graphs=graphs.."				var options_"..i.."={title: \""..title.."\","..(options or "").."};\n";
		graphs=graphs.."				var chart_"..i.." = new google.visualization."..func.."(document.getElementById('".._out.."'));\n";
		graphs=graphs.."				chart_"..i..".draw(data_"..i..",options_"..i..");\n";
	end
	graphs=graphs..[[
			}
]];
	local visits,countries,map,others,mainSeg=self:MakeOutContainer("hrvisits"),self:MakeOutContainer("countries"),self:MakeOutContainer("map"),"<table border='1' style='width:98%'>";
	local tv,uv=0,self:guv();
	local svname=System.GetCVar("sv_servername") or "unknown server";
	for i=0,23 do tv=tv+#self.Visits[i]; end
	others=others.."<tr><th>Description</th><th>Value</th></tr><tr><td>Total visits: </td><td>"..tv.."</td></tr><tr><td>Total unique visits: </td><td>"..uv.."</td></tr>";
	others=others.."</table>";
	visits=self:subsegadd("Visits by hour",visits,true);
	countries=self:subsegadd("Visits by country",countries,true);
	map=self:subsegadd("Visits by country - map",map,true);
	others=self:subsegadd("Other statistics",others,true);
	local final=visits..countries..map..others;
	mainSeg=self:segadd("Connection",final);
	sc[1]=mainSeg;
	local content=string.format(template,graphs,svname,unpack(sc));
	local f,err=io.open(out,"w");
	if not f then return; end
	f:write(content);
	--printf("Statistics HTML Generated, length: %d",#content,out)
	f:close();
end
function _S:OnConnect(person,fhour)
	--for i,v in pairs(date()) do print(i,v); end
	local info=self.Visits[fhour or tonumber(date().hour)];
	info[#info+1]={person.country,person.ip,person.profile};
end
function _S:MakeGraph(title,data,out,desc,options,f)
	self.Graphs[#self.Graphs+1]={title,data,out,desc or {"(unknown)","(unknown)"},options or false,f or "PieChart"};
end
function _S:OnDisconnect(person)
	
end
function _S:MakeOutContainer(id)
	return string.format([[<div id="%s" style="width:100%%; height:500px; "></div>]],id);
end
function _S:PrepareData()
	self.Graphs=nil;
	self.Graphs={};
	local countries1,ips,profiles,hrvisits={},{},{},{};
	local allow=false;
	--local countries2={};
	for i,v in pairs(self.Visits)do
		hrvisits[i]={#v,self:guvph(i),};
		for j,w in pairs(v or {}) do
			local country,ip,profile=unpack(w);
			local cn=self.Countries[country] or "unknown";
			if not self.IgnoreUnkownCountries then allow=true; else
				if not tonumber(country) and country:len()==2 and country~="eu" then
					allow=true;
				end
			end
			if allow then
				countries1[cn]=(countries1[cn] or 0)+1;
			end
			--countries2[country]=(countries2[country] or 0)+1;
			profiles[profile]=(profiles[profile] or 0)+1;
			ips[ip]=(ips[ip] or 0)+1;
			allow=false;
		end
	end
	local desc1,desc2={"Nation","Visits"},{"Hour","Visits","Unique visits"};
	self:MakeGraph("Visits by country",countries1,"countries",desc1,nil,"PieChart");
	self:MakeGraph("Visits by country - map",countries1,"map",desc1,nil,"GeoChart");
	self:MakeGraph("Visits by hour",hrvisits,"hrvisits",desc2,"hAxis: {title: 'Hour'}","AreaChart");
end

function _S_OnTimerTick()
	if not _S.Inited then
		--printf("[ _S Init: %s ]",SafeWriting.GlobalStorageFolder)
		_S:Init(SafeWriting.GlobalStorageFolder);
	else
		local mnow=os.date("*t").month;
		if not _S.StartMonth then
			_S.StartMonth=mnow;
			printf("Setting start month to %d",mnow);
		end
		local before=_S.StartMonth;
		if before~=mnow then
			printf("Statistics::NewMonth!");
			_S.Visits=nil;
			_S.Visits={};
			for n=0,23 do _S.Visits[n]={}; end
			_S.StartMonth=mnow;
		end
		if _time-_S.LastSave>120 then
			--printf("[ _S GenerateHTML: ]");
			_S:GenerateHTML();
			_S:Save();
			_S.LastSave=_time;
		end
	end
end
function _S_CheckPlayer(player)
	--printf("[ _S OnConnect: %s, country: %s ]",player:GetName(),player.country);
	_S:OnConnect({
		["country"]=player.country;
		["ip"]=player.ip;
		["profile"]=player.profile;
	});
end
if _S_Enabled then
	SafeWriting.FuncContainer:AddFunc(_S_OnTimerTick,"OnTimerTick");
	SafeWriting.FuncContainer:AddFunc(_S_CheckPlayer,"CheckPlayer");
end