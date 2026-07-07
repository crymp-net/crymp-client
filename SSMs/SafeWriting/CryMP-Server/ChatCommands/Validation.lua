function AttemptValidate(player, prof, uid, name, attempt)
    local url = urlfmt("/api/validate.php?prof=%s&uid=%s", prof, uid)
    local se = SafeWriting.Settings
    attempt = attempt or 0
    AsyncConnectHTTP(
        se.MasterHost or "crymp.org",
        url,
        "GET",
        443,
        true,
        15,
        function(content, error)
            if not error then
                local err = string.find(content, "%Validation:Failed%", nil, true)
                if err then
                    KickPlayer(player, "profile id validation error")
                else
                    printf("Validation success for %s/%s", name or "<unknown>", player:GetName() or "<unknown>")
                    player.profile = prof
                    local se = SafeWriting.Settings
                    if (se.UseAuthentificationPassword and g_gameRules.class == "PowerStruggle") then
                        player.IsAdminLogged = false
                        player.IsModeratorLogged = false
                        player.IsPremiumLogged = false
                    else
                        if (se.Admins[player.profile]) then
                            player.IsAdminLogged = true
                        end
                        if (se.Moderators[player.profile]) then
                            player.IsModeratorLogged = true
                        end
                        if (se.Premiums[player.profile]) then
                            player.IsPremiumLogged = true
                        end
                    end
                    player.waitingForAuth = nil
                    player.isSfwCl = true
                    _G["ValidIds"] = _G["ValidIds"] or {}
                    _G["ValidIds"][uid] = prof
                    if player.profile and player.hwid and player.isSfwCl then
                        CheckPlayer(player)
                    end
                end
            else
                if attempt < 4 then
                    printf("Validation warning: %s , attempt: %d", tostring(content), attempt + 1)
                    AttemptValidate(player, prof, uid, name, attempt + 1)
                else
                    KickPlayer(player, "profile id validation error")
                end
            end
        end
    )
end

function AttemptUUID(player)
        --printf("Attempt UUID")
        if not player.hwid and not player.doingUUID then
                player.doingUUID = true
                local handle = RPC:CallOne(player, "UUID", {salt="check"})
                RPC:Await(handle, function(_, resp)
                        --printf("Got RPC response, hwid: %s", resp.uuid)
                        player.uuid = resp;
                        player.locale = resp.locale;
                        local part1, part2 = resp.uuid:match("^([A-F0-9]+):(.+)");
                        player.hwid = part1;
                        if player.profile and player.hwid and player.isSfwCl then
                                CheckPlayer(player)
                                Translator_DetectLang(player)
                        end
                end)
        end
end

AddChatCommand(
    "validate",
    function(self, player, msg, prof, uid, name)
        player.WasChecked = false
        AttemptUUID(player)
        Script.SetTimer(
            1,
            function()
                local se = SafeWriting.Settings
                if SafeWriting.Settings.AllowMasterServer and prof and uid and name then
                    local numpr = tonumber(prof)

                    if _G["ValidIds"] and _G["ValidIds"][uid] == prof then
                        player.waitingForAuth = nil
                        player.profile = prof
                        player.isSfwCl = true
                        --RenamePlayer(player,name);
                        if (se.UseAuthentificationPassword and g_gameRules.class == "PowerStruggle") then
                            player.IsAdminLogged = false
                            player.IsModeratorLogged = false
                            player.IsPremiumLogged = false
                        else
                            if (se.Admins[player.profile]) then
                                player.IsAdminLogged = true
                            end
                            if (se.Moderators[player.profile]) then
                                player.IsModeratorLogged = true
                            end
                            if (se.Premiums[player.profile]) then
                                player.IsPremiumLogged = true
                            end
                        end
                        if player.profile and player.hwid and player.isSfwCl then
                                CheckPlayer(player)
                        end
                    else
                        AttemptValidate(player, prof, uid, name, 0)
                    end
                end
            end
        )
    end,
    {WORD, WORD, WORD}
)