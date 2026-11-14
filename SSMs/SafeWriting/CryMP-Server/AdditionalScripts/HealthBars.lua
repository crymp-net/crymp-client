HealthBars = {}

function HealthBars:UpdatePlayer(player)
	g_gameRules.game:SetSynchedEntityValue(player.id, 901, math.max(0, player.actor:GetHealth()))
end

LoadPlugin(HealthBars)
