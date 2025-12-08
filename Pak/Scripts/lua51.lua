unpack = table.unpack
loadstring = load
string.gfind = string.gmatch

math.pow = function(a, b) return a ^ b end

table.getn = function(self)
	return #self
end