local math_random = math.random

unpack = table.unpack

table.getn = function(self)
	return #self
end

math.random = lua51.random