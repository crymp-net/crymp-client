unpack = table.unpack
loadstring = load
string.gfind = string.gmatch

table.getn = function(self)
	return #self
end