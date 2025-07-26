#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CrySystem/ICryPak.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/Cry3DEngine/IMaterial.h"
#include "CryCommon/CryEntitySystem/IEntitySystem.h"
#include "CryCommon/CryAction/IVehicleSystem.h"
#include "CryCommon/CryMath/Cry_Camera.h"
#include "CrySystem/LocalizationManager.h"
#include "CrySystem/RandomGenerator.h"
#include "Library/StringTools.h"
#include "Library/Util.h"
#include "Library/WinAPI.h"

#include "ScriptBind_CPPAPI.h"
#include "CryMP/Client/Client.h"
#include "CryMP/Client/ScriptCommands.h"
#include "CryMP/Client/ScriptCallbacks.h"
#include "CryMP/Client/DrawTools.h"
#include "CryMP/Server/Server.h"
#include "CryGame/GameActions.h"
#include "CryGame/Actors/Actor.h"

ScriptBind_CPPAPI::ScriptBind_CPPAPI()
{
	Init(gEnv->pScriptSystem, gEnv->pSystem);

	SetGlobalName("CPPAPI");

#undef SCRIPT_REG_CLASSNAME
#define SCRIPT_REG_CLASSNAME &ScriptBind_CPPAPI::

	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_UPDATE);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_DISCONNECT);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_SPAWN);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_MASTER_RESOLVED);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_LOADING_START);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_GAME_RULES_CREATED);
	SCRIPT_REG_GLOBAL(SCRIPT_CALLBACK_ON_BECOME_LOCAL_ACTOR);

	SCRIPT_REG_TEMPLFUNC(AddCCommand, "name, handler");
	SCRIPT_REG_TEMPLFUNC(ApplyMaskOne, "entity, mask, apply");
	SCRIPT_REG_TEMPLFUNC(ApplyMaskAll, "mask, apply");
	SCRIPT_REG_TEMPLFUNC(FSetCVar, "cvar, value");
	SCRIPT_REG_TEMPLFUNC(GetLocaleInformation, "");
	SCRIPT_REG_TEMPLFUNC(GetMapName, "");
	SCRIPT_REG_TEMPLFUNC(MakeUUID, "salt");
	SCRIPT_REG_TEMPLFUNC(Random, "");
	SCRIPT_REG_TEMPLFUNC(Request, "params, callback");
	SCRIPT_REG_TEMPLFUNC(SetCallback, "callback, handler");
	SCRIPT_REG_TEMPLFUNC(SHA256, "text");
	SCRIPT_REG_TEMPLFUNC(URLEncode, "text");
	SCRIPT_REG_TEMPLFUNC(GetMasters, "");
	SCRIPT_REG_FUNC(GetRenderType);
	SCRIPT_REG_TEMPLFUNC(GetKeyName, "action");
	SCRIPT_REG_TEMPLFUNC(IsKeyUsed, "key");
	SCRIPT_REG_TEMPLFUNC(CreateKeyBind, "key, command");
	SCRIPT_REG_TEMPLFUNC(CreateKeyFunction, "key, function");
	SCRIPT_REG_FUNC(ClearKeyBinds);
	SCRIPT_REG_TEMPLFUNC(GetModelFilePath, "entityId, slot");
	SCRIPT_REG_TEMPLFUNC(CreateMaterialFromTexture, "materialName, texturePath");
	SCRIPT_REG_TEMPLFUNC(SetOpacity, "entityId, fAmount");
	SCRIPT_REG_TEMPLFUNC(GetLastSeenTime, "entityId");
	SCRIPT_REG_FUNC(GetLP);
	SCRIPT_REG_FUNC(GetNumVars);
	SCRIPT_REG_FUNC(GetVars);
	SCRIPT_REG_TEMPLFUNC(SetProfile, "type, profileId, token");

	// Localization
	SCRIPT_REG_TEMPLFUNC(GetLanguage, "");
	SCRIPT_REG_TEMPLFUNC(LocalizeText, "text");
	SCRIPT_REG_TEMPLFUNC(AddLocalizedLabel, "name, params");

	// DrawTools
	SCRIPT_REG_TEMPLFUNC(DrawText, "posX, posY, xscale, yscale, color1, color2, color3, color4, text");
	SCRIPT_REG_TEMPLFUNC(DrawImage, "posX, posY, width, height, texturePath");
	SCRIPT_REG_TEMPLFUNC(DrawColorBox, "posX, posY, width, height, color1, color2, color3, opacity");
	SCRIPT_REG_TEMPLFUNC(RemoveTextOrImageById, "");
	SCRIPT_REG_FUNC(RemoveTextOrImageAll);

	SCRIPT_REG_TEMPLFUNC(GetLoadingScreenMapPicturePath, "");

	//Effects
	SCRIPT_REG_TEMPLFUNC(FOVEffect, "goalFov, speed");
	
	//Entity attachments
	SCRIPT_REG_TEMPLFUNC(SetAttachmentMaterial, "entityId, characterSlot, attachmentName, materialName");
	SCRIPT_REG_TEMPLFUNC(GetCharacterAttachments, "entityId, characterSlot");
	SCRIPT_REG_TEMPLFUNC(GetCharacterJoints, "entityId, characterSlot");
	SCRIPT_REG_TEMPLFUNC(CreateCharacterDecal, "entityId, characterSlot, params");

	//Filters
	SCRIPT_REG_TEMPLFUNC(EnableActionFilter, "filterName, enable");
	SCRIPT_REG_TEMPLFUNC(IsActionFilterEnabled, "filterName");
	SCRIPT_REG_TEMPLFUNC(IsActionFilterAvailable, "filterName");
	SCRIPT_REG_TEMPLFUNC(CreateActionFilter, "filterName, actions");

	//SSM support
	SCRIPT_REG_TEMPLFUNC(GetIP, "host");
	SCRIPT_REG_TEMPLFUNC(GetLocalIP, "");
	SCRIPT_REG_TEMPLFUNC(GetTime, "future");
}

ScriptBind_CPPAPI::~ScriptBind_CPPAPI()
{
}

int ScriptBind_CPPAPI::AddCCommand(IFunctionHandler *pH, const char *name, HSCRIPTFUNCTION handler)
{
	if(!gClient) {
		return pH->EndFunction(false);
	}
	
	bool success = gClient->GetScriptCommands()->AddCommand(name, handler);

	if (!success)
		m_pSS->ReleaseFunc(handler);

	return pH->EndFunction(success);
}

int ScriptBind_CPPAPI::ApplyMaskOne(IFunctionHandler *pH, ScriptHandle entity, int mask, bool apply)
{
	IEntity *pEntity = gEnv->pEntitySystem->GetEntity(entity.n);
	if(!pEntity)
		return pH->EndFunction(false);

	IEntityRenderProxy *pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (!pRenderProxy)
		return pH->EndFunction(false);

	int newMask = pRenderProxy->GetMaterialLayersMask();

	if (apply)
		newMask |= mask;
	else
		newMask &= ~mask;

	pRenderProxy->SetMaterialLayersMask(newMask);

	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::ApplyMaskAll(IFunctionHandler* pH, int applyMask, bool apply) {
	IEntitySystem* pES = gEnv->pSystem->GetIEntitySystem();
	if (pES) {
		IEntityIt* it = pES->GetEntityIterator();
		int affectedEntities = 0;
		while (!it->IsEnd()) {
			IEntity* pEntity = it->This();
			int targetMask = applyMask;
			if (targetMask == MTL_LAYER_FROZEN && apply) {
				IVehicleSystem* pVS = g_pGame->GetIGameFramework()->GetIVehicleSystem();
				if (pVS->GetVehicle(pEntity->GetId()))
					targetMask = MTL_LAYER_DYNAMICFROZEN;
			}
			IEntityRenderProxy* pRP = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
			if (pRP) {
				int mask = pRP->GetMaterialLayersMask();
				if (apply)
					mask |= targetMask;
				else mask &= ~targetMask;
				pRP->SetMaterialLayersMask(mask);
			}
			affectedEntities++;
			it->Next();
		}
		return pH->EndFunction(affectedEntities);
	}
	return pH->EndFunction(0);
}

int ScriptBind_CPPAPI::FSetCVar(IFunctionHandler *pH, const char *cvar, const char *value)
{
	ICVar* pCVar = gEnv->pConsole->GetCVar(cvar);
	if (!pCVar)
	{
		return pH->EndFunction(false);
	}

	const std::string previousVal = pCVar->GetString();
	if (gEnv->bServer) {
		pCVar->ForceSet(value);
	} else if (previousVal != std::string_view(value)) {
		pCVar->ForceSet(value);

		// CVar still the same, is it synced?
		if (previousVal == std::string_view(pCVar->GetString()))
		{
			const int previousFlags = pCVar->GetFlags();

			// disable sync
			pCVar->SetFlags(VF_NOT_NET_SYNCED);

			// 2nd attempt
			pCVar->ForceSet(value);

			// now restore the flags
			pCVar->SetFlags(previousFlags);
			// CVar value won't change untill server changes it to something else

			if (std::string_view(value) != std::string_view(pCVar->GetString()))
			{
				CryLogWarning("$4[CryMP] Failed to change CVar %s - value still %s", cvar, pCVar->GetString());
				return pH->EndFunction(false);
			}
		}
	}

	// all good!
	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::GetLocaleInformation(IFunctionHandler *pH)
{
	return pH->EndFunction(WinAPI::GetLocale().c_str(), (int)WinAPI::GetTimeZoneBias());
}

int ScriptBind_CPPAPI::GetMapName(IFunctionHandler *pH)
{
	return pH->EndFunction(g_pGame->GetIGameFramework()->GetLevelName());
}

int ScriptBind_CPPAPI::MakeUUID(IFunctionHandler *pH, const char *salt)
{
	return pH->EndFunction(Util::GetHWID(salt).c_str());
}

int ScriptBind_CPPAPI::Random(IFunctionHandler *pH)
{
	return pH->EndFunction(RandomGenerator::GenerateFloat(0, 1));
}

int ScriptBind_CPPAPI::Request(IFunctionHandler *pH, SmartScriptTable params, HSCRIPTFUNCTION callback)
{
	HTTPClientRequest request;

	{
		CScriptSetGetChain chain(params);

		const char *url = "";
		const char *method = "GET";
		const char *body = "";
		SmartScriptTable headers;

		chain.GetValue("url", url);
		chain.GetValue("method", method);
		chain.GetValue("body", body);
		chain.GetValue("headers", headers);

		request.url = url;
		request.method = method;
		request.data = body;

		if (headers)
		{
			auto it = headers->BeginIteration();
			while (headers->MoveNext(it))
			{
				if (it.sKey && it.value.GetVarType() == ScriptVarType::svtString)
				{
					request.headers[it.sKey] = it.value.str;
				}
			}
			headers->EndIteration(it);
		}
	}

	if (request.url.empty())
	{
		m_pSS->ReleaseFunc(callback);
		return pH->EndFunction(false, "url not provided");
	}

	request.callback = [callback, pSS = m_pSS](HTTPClientResult & result)
	{
		if (pSS->BeginCall(callback))
		{
			if (result.error.empty())
			{
				pSS->PushFuncParam(false);
			}
			else
			{
				pSS->PushFuncParam(result.error.c_str());
			}

			pSS->PushFuncParam(result.response.c_str());
			pSS->PushFuncParam(result.code);
			pSS->EndCall();
		}

		pSS->ReleaseFunc(callback);
	};

	if(gClient) {
		gClient->HttpRequest(std::move(request));
	} else if(gServer) {
		gServer->HttpRequest(std::move(request));
	}

	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::SetCallback(IFunctionHandler *pH, int callback, HSCRIPTFUNCTION handler)
{
	bool success = gClient->GetScriptCallbacks()->SetHandler(static_cast<EScriptCallback>(callback), handler);

	if (!success)
		m_pSS->ReleaseFunc(handler);

	return pH->EndFunction(success);
}

int ScriptBind_CPPAPI::SHA256(IFunctionHandler *pH, const char *text)
{
	return pH->EndFunction(Util::SHA256(text).c_str());
}

int ScriptBind_CPPAPI::URLEncode(IFunctionHandler *pH, const char *text)
{
	return pH->EndFunction(HTTP::URLEncode(text).c_str());
}

int ScriptBind_CPPAPI::GetMasters(IFunctionHandler *pH)
{
	SmartScriptTable masters(m_pSS);

	if(gClient) {
		for (const std::string & master : gClient->GetMasters())
		{
			masters->PushBack(master.c_str());
		}
	}

	return pH->EndFunction(masters);
}

int ScriptBind_CPPAPI::GetRenderType(IFunctionHandler* pH)
{
	return pH->EndFunction(static_cast<int>(gEnv->pRenderer->GetRenderType()));
}

int ScriptBind_CPPAPI::GetKeyName(IFunctionHandler* pH, const char* action)
{
	IActionMapManager* pAM = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
	while (IActionMap* pMap = iter->Next())
	{
		const char* actionMapName = pMap->GetName();
		IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
		while (const SActionMapBindInfo* pInfo = pIter->Next())
		{
			if (!strcmp(pInfo->action, action))
				return pH->EndFunction(pInfo->keys[0]);
		}
	}

	return pH->EndFunction("");
}

int ScriptBind_CPPAPI::IsKeyUsed(IFunctionHandler* pH, const char* key)
{
	IActionMapManager* pAM = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionMapIteratorPtr iter = pAM->CreateActionMapIterator();
	while (IActionMap* pMap = iter->Next())
	{
		const char* actionMapName = pMap->GetName();
		IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
		while (const SActionMapBindInfo* pInfo = pIter->Next())
		{
			if (!strcmp(pInfo->keys[0], key))
				return pH->EndFunction(true);
		}
	}

	return pH->EndFunction(false);
}

int ScriptBind_CPPAPI::CreateKeyBind(IFunctionHandler* pH, const char* key, const char* action)
{
	if(gClient) {
		gClient->AddKeyBind(key, action);
		return pH->EndFunction(true);
	} else {
		return pH->EndFunction(false);
	}
}

int ScriptBind_CPPAPI::CreateKeyFunction(IFunctionHandler* pH, const char* key, HSCRIPTFUNCTION function)
{
	if(gClient) {
		gClient->AddKeyBind(key, function);
		return pH->EndFunction(true);
	} else {
		return pH->EndFunction(false);
	}
}

int ScriptBind_CPPAPI::ClearKeyBinds(IFunctionHandler* pH)
{
	if(gClient) {
		gClient->ClearKeyBinds();
	}

	return pH->EndFunction();
}

int ScriptBind_CPPAPI::GetModelFilePath(IFunctionHandler* pH, ScriptHandle entityId, int slot)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	if (ICharacterInstance* pCharacter = pEntity->GetCharacter(slot))
	{
		return pH->EndFunction(pCharacter->GetFilePath());
	}
	if (IStatObj* pObject = pEntity->GetStatObj(slot))
	{
		return pH->EndFunction(pObject->GetFilePath());
	}
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::CreateMaterialFromTexture(IFunctionHandler* pH, const char* materialName, const char* texturePath)
{
	IMaterialManager* pMatMan(gEnv->p3DEngine->GetMaterialManager());
	IMaterial* pMat(pMatMan->FindMaterial(materialName));
	if (pMat)
	{
		CryLog("[CryMP] Material '%s' already created!", pMat->GetName());
		return pH->EndFunction();
	}

	IMaterial* pMatSrc(pMatMan->LoadMaterial("Materials/Decals/Default", false, true));
	if (pMatSrc)
	{
		IMaterial* pMatDst(pMatMan->CreateMaterial(materialName, pMatSrc->GetFlags() | MTL_FLAG_NON_REMOVABLE));
		if (pMatDst)
		{
			SShaderItem& si(pMatSrc->GetShaderItem());

			SInputShaderResources isr(si.m_pShaderResources);
			isr.m_Textures[EFTT_DIFFUSE].m_Name = texturePath;

			CryLog("[CryMP] Created Material '%s' successfully (%s)", pMatDst->GetName(), si.m_pShader->GetName());

			SShaderItem siDst(gEnv->pRenderer->EF_LoadShaderItem(si.m_pShader->GetName(), true, 0, &isr, si.m_pShader->GetGenerationMask()));
			pMatDst->AssignShaderItem(siDst);

			return pH->EndFunction();
		}
	}
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::SetOpacity(IFunctionHandler* pH, ScriptHandle entity, float fAmount)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entity.n);
	if (!pEntity)
		return pH->EndFunction(false);

	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (!pRenderProxy)
		return pH->EndFunction(false);

	pRenderProxy->SetOpacity(fAmount);

	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::GetLastSeenTime(IFunctionHandler* pH, ScriptHandle entity)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entity.n);
	if (!pEntity)
		return pH->EndFunction(-1);

	IEntityRenderProxy* pRenderProxy = static_cast<IEntityRenderProxy*>(pEntity->GetProxy(ENTITY_PROXY_RENDER));
	if (!pRenderProxy)
		return pH->EndFunction(-1);

	return pH->EndFunction(pRenderProxy->GetLastSeenTime());
}

int ScriptBind_CPPAPI::GetLP(IFunctionHandler* pH)
{
	SmartScriptTable pks(m_pSS);
	ICryPak::PakInfo* pPakInfo = gEnv->pCryPak->GetPakInfo();
	size_t openPakSize = 0;
	for (uint32 pak = 0; pak < pPakInfo->numOpenPaks; pak++)
	{
		const auto &c = pPakInfo->arrPaks[pak];
		std::string path = StringTools::ToLower(c.szFilePath);
		pks->SetValue(path.c_str(), static_cast<int>(c.nUsedMem));
	}
	gEnv->pCryPak->FreePakInfo(pPakInfo);
	return pH->EndFunction(pks);
}

int ScriptBind_CPPAPI::GetNumVars(IFunctionHandler* pH)
{
	return pH->EndFunction(gEnv->pConsole->GetNumVars());
}

int ScriptBind_CPPAPI::GetVars(IFunctionHandler* pH)
{
	std::vector<const char*> cmds;

	cmds.resize(gEnv->pConsole->GetNumVars());
	gEnv->pConsole->GetSortedVars(&cmds[0], cmds.size());

	SmartScriptTable vars(m_pSS);
	for (const char* vName : cmds)
	{
		vars->PushBack(vName);
	}
	return pH->EndFunction(vars);
}

int ScriptBind_CPPAPI::SetProfile(IFunctionHandler* pH, const char* type, const char* profileId, const char* token) {
	std::string strType{ type };
	SProfileInfo info{
		.id = profileId,
		.token = token
	};
	if (info.id.length() == 0 || info.token.length() == 0) {
		auto it = m_profiles.find(strType);
		if (it != m_profiles.end()) {
			m_profiles.erase(it);
		}
	} else {
		m_profiles[type] = std::move(info);
	}
	return pH->EndFunction();
}

std::optional<SProfileInfo> ScriptBind_CPPAPI::GetProfile(const std::string& type) {
	auto it = m_profiles.find(type);
	if (it == m_profiles.end()) {
		return {};
	} else {
		return it->second;
	}
}

////////////////////////////////////////////////////////////////////////////////
// Localization
////////////////////////////////////////////////////////////////////////////////

int ScriptBind_CPPAPI::GetLanguage(IFunctionHandler* pH)
{
	const char* language = LocalizationManager::GetInstance().GetCurrentLanguage().name.c_str();

	return pH->EndFunction(language);
}

int ScriptBind_CPPAPI::LocalizeText(IFunctionHandler* pH, const char* text)
{
	const std::string result = LocalizationManager::GetInstance().Localize(text);

	return pH->EndFunction(result.c_str());
}

int ScriptBind_CPPAPI::AddLocalizedLabel(IFunctionHandler* pH, const char* name, SmartScriptTable params)
{
	LocalizationManager& localization = LocalizationManager::GetInstance();

	LocalizationManager::Label label;
	bool keepExisting = false;

	const auto readData = [&label, &keepExisting](CScriptSetGetChain& chain)
	{
		const char* value;

		if (chain.GetValue("english_text", value))
			label.englishText = value;

		if (chain.GetValue("english_subtitle", value))
			label.englishSubtitle = value;

		if (chain.GetValue("localized_text", value))
			StringTools::AssignTo(label.localizedText, value);

		if (chain.GetValue("localized_subtitle", value))
			StringTools::AssignTo(label.localizedSubtitle, value);

		if (chain.GetValue("character_name", value))
			StringTools::AssignTo(label.characterName, value);

		if (chain.GetValue("sound_event", value))
			label.soundEvent = value;

		chain.GetValue("sound_volume",           label.soundVolume);
		chain.GetValue("sound_ducking",          label.soundDucking);
		chain.GetValue("sound_radio_ratio",      label.soundRadioRatio);
		chain.GetValue("sound_radio_background", label.soundRadioBackground);
		chain.GetValue("sound_radio_squelch",    label.soundRadioSquelch);
		chain.GetValue("use_subtitle",           label.useSubtitle);

		chain.GetValue("keep_existing", keepExisting);
	};

	label.name = name;

	const LocalizationManager::Label* existingLabel = localization.FindLabel(label.name);
	if (existingLabel)
	{
		label = *existingLabel;
	}

	if (params)
	{
		CScriptSetGetChain rootChain(params);

		readData(rootChain);

		SmartScriptTable languageList;
		if (rootChain.GetValue("languages", languageList))
		{
			const char* languageName = localization.GetCurrentLanguage().name.c_str();

			SmartScriptTable language;
			if (languageList->GetValue(languageName, language))
			{
				CScriptSetGetChain languageChain(language);

				readData(languageChain);
			}
		}
	}

	const bool success = localization.Add(std::move(label), keepExisting);

	return pH->EndFunction(success);
}

////////////////////////////////////////////////////////////////////////////////
// DrawTools
////////////////////////////////////////////////////////////////////////////////
int ScriptBind_CPPAPI::DrawText(IFunctionHandler* pH, float posX, float posY, float xscale, float yscale, float color1, float color2, float color3, float color4, const char* text)
{
	if(!gClient) {
		return pH->EndFunction();
	}
	
	int replace = 0;
	if (pH->GetParamType(10) == svtNumber)
	{
		pH->GetParam(10, replace);
	}

	DrawTools::Text m;
	m.posX = posX;
	m.posY = posY;
	m.text = text;
	m.xscale = xscale;
	m.yscale = yscale;
	m.color[0] = color1;
	m.color[1] = color2;
	m.color[2] = color3;
	m.color[3] = color4;

	if (replace)
	{
		m.id = replace;
	}

	const int id = gClient->GetDrawTools()->Add(m);

	return pH->EndFunction(id);
}

int ScriptBind_CPPAPI::DrawImage(IFunctionHandler* pH, float posX, float posY, float width, float height, const char* texturePath)
{
	if(!gClient) {
		return pH->EndFunction();
	}
	
	ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(texturePath, FT_FROMIMAGE, eTT_2D);
	if (!pTexture)
	{
		CryLogWarningAlways("[DrawImage] Failed to open texture '%s'", texturePath);
		return pH->EndFunction(-1);
	}
	if (!pTexture->IsTextureLoaded())
	{
		CryLogWarningAlways("[DrawImage] Failed to load texture '%s'", pTexture->GetName());
		gEnv->pRenderer->RemoveTexture(pTexture->GetTextureID());

		return pH->EndFunction(-2);
	}

	DrawTools::Image m;
	m.posX = posX;
	m.posY = posY;
	m.width = width;
	m.height = height;

	const int id = gClient->GetDrawTools()->Add(m, pTexture->GetTextureID());

	return pH->EndFunction(id);
}

int ScriptBind_CPPAPI::DrawColorBox(IFunctionHandler* pH, float posX, float posY, float width, float height, float color1, float color2, float color3, float opacity)
{
	if(!gClient) {
		return pH->EndFunction();
	}
	
	DrawTools::Image m;
	m.posX = posX;
	m.posY = posY;
	m.width = width;
	m.height = height;
	m.colorBox = true;
	m.color[0] = color1;
	m.color[1] = color2;
	m.color[2] = color3;
	m.color[3] = opacity;

	const int id = gClient->GetDrawTools()->Add(m, 0);

	return pH->EndFunction(id);
}

int ScriptBind_CPPAPI::RemoveTextOrImageById(IFunctionHandler* pH, int id)
{
	if(!gClient) {
		return pH->EndFunction();
	}
	
	gClient->GetDrawTools()->RemoveTextOrImageById(id);
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::RemoveTextOrImageAll(IFunctionHandler* pH)
{
	if(!gClient) {
		return pH->EndFunction();
	}

	gClient->GetDrawTools()->ClearScreen();
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::GetLoadingScreenMapPicturePath(IFunctionHandler* pH, const char* level)
{
	ILevelInfo* pLI = g_pGame->GetIGameFramework()->GetILevelSystem()->GetLevelInfo(level);
	if (!pLI)
	{
		return pH->EndFunction();
	}
	string rootName = pLI->GetPath();

	//now load the actual map
	string mapName = rootName;
	int slashPos = mapName.rfind('\\');
	if (slashPos == -1)
		slashPos = mapName.rfind('/');
	mapName = mapName.substr(slashPos + 1, mapName.length() - slashPos);

	string sXml = rootName;
	sXml.append("/");
	sXml.append(mapName);
	sXml.append(".xml");
	XmlNodeRef mapInfo = GetISystem()->LoadXmlFile(sXml.c_str());
	std::vector<string> screenArray;

	const char* header = NULL;
	const char* description = NULL;

	if (mapInfo == 0)
	{
		return pH->EndFunction();
	}
	else
	{
		//retrieve the coordinates of the map
		for (int n = 0; n < mapInfo->getChildCount(); ++n)
		{
			XmlNodeRef mapNode = mapInfo->getChild(n);
			const char* name = mapNode->getTag();
			if (!_stricmp(name, "LoadingScreens"))
			{
				int attribs = mapNode->getNumAttributes();
				const char* key;
				const char* value;
				for (int i = 0; i < attribs; ++i)
				{
					mapNode->getAttributeByIndex(i, &key, &value);
					screenArray.push_back(value);
				}
			}
		}
	}

	int size = screenArray.size();
	if (size <= 0)
	{
		screenArray.push_back("loading.dds");
		size = 1;
	}

	unsigned int iUse = cry_rand() % size;
	string sImg = rootName;
	sImg.append("/");
	sImg.append(screenArray[iUse]);

	return pH->EndFunction(sImg.c_str());
}

int ScriptBind_CPPAPI::FOVEffect(IFunctionHandler* pH, float goalFOV, float speed)
{
	CActor* pClientActor = static_cast<CActor*>(g_pGame->GetIGameFramework()->GetClientActor());
	if (pClientActor)
	{
		CScreenEffects* pScreenEffects = pClientActor->GetScreenEffects();
		if (pScreenEffects)
		{
			pScreenEffects->ClearBlendGroup(pClientActor->m_hitReactionID);

			if (goalFOV <= 0.0f)
			{
				return pH->EndFunction();
			}

			CFOVEffect* zOut = new CFOVEffect(pClientActor->GetEntityId(), goalFOV);
			CLinearBlend* blend = new CLinearBlend(1);

			pScreenEffects->StartBlend(zOut, blend, speed, pClientActor->m_hitReactionID);
		}
	}
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::SetAttachmentMaterial(IFunctionHandler* pH, ScriptHandle entityId, int characterSlot, const char* attachmentName, const char* materialName)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICharacterInstance* pCharacter = pEntity->GetCharacter(characterSlot);

	if (!pCharacter)
		return pH->EndFunction();

	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

	if (pIAttachmentManager)
	{
		if (IAttachment* pAttachment = pIAttachmentManager->GetInterfaceByName(attachmentName))
		{
			if (IAttachmentObject* pAttachmentObject = pAttachment->GetIAttachmentObject())
			{
				IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(materialName, false);
				if (!pMaterial)
					CryLogWarningAlways("No such material '%s'", materialName);
				else
				{
					pAttachmentObject->SetMaterial(pMaterial);
				}
			}
		}
		else
			CryLogWarningAlways("No such attachment '%s'", attachmentName);
	}

	return pH->EndFunction();
}

int ScriptBind_CPPAPI::GetCharacterAttachments(IFunctionHandler* pH, ScriptHandle entityId, int characterSlot)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICharacterInstance* pCharacter = pEntity->GetCharacter(characterSlot);

	if (!pCharacter)
		return pH->EndFunction();

	IAttachmentManager* pIAttachmentManager = pCharacter->GetIAttachmentManager();

	SmartScriptTable attachments(m_pSS);

	if (pIAttachmentManager)
	{
		const int n = pIAttachmentManager->GetAttachmentCount();
		for (int i = 0; i < n; i++)
		{
			IAttachment* pAttachment = pIAttachmentManager->GetInterfaceByIndex(i);
			if (pAttachment)
			{
				attachments->PushBack(pAttachment->GetName());
			}
		}
	}

	return pH->EndFunction(attachments);
}

int ScriptBind_CPPAPI::GetCharacterJoints(IFunctionHandler* pH, ScriptHandle entityId, int characterSlot)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICharacterInstance* pCharacter = pEntity->GetCharacter(characterSlot);

	if (!pCharacter)
		return pH->EndFunction();

	ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
	if (!pISkeletonPose)
	{
		CryLogWarningAlways("No skeleton on this character '%s'", pCharacter->GetFilePath());
		return pH->EndFunction();
	}

	SmartScriptTable joints(m_pSS);
	if (pISkeletonPose)
	{
		uint32 numJoints = pISkeletonPose->GetJointCount();
		for (int i = 0; i < numJoints; i++)
		{
			const char* jointName = pISkeletonPose->GetJointNameByID(i);
			if (jointName && jointName[0])
			{
				joints->SetAt(i, jointName);
			}
		}
	}

	return pH->EndFunction(joints);
}

int ScriptBind_CPPAPI::CreateCharacterDecal(IFunctionHandler* pH, ScriptHandle entityId, int characterSlot, SmartScriptTable params)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId.n);
	if (!pEntity)
		return pH->EndFunction();

	ICharacterInstance* pCharacter = pEntity->GetCharacter(characterSlot);
	if (!pCharacter)
	{
		CryLogWarningAlways("Entity has no character on slot %d", characterSlot);
		return pH->EndFunction();
	}

	IRenderNode* pRenderNode = nullptr;
	if (pEntity)
	{
		IEntityRenderProxy* pRenderProxy = (IEntityRenderProxy*)pEntity->GetProxy(ENTITY_PROXY_RENDER);
		if (pRenderProxy)
			pRenderNode = pRenderProxy->GetRenderNode();
	}

	if (!pRenderNode)
	{
		CryLogWarningAlways("No render node on entity '%s'", pEntity->GetName());
		return pH->EndFunction();
	}

	CScriptSetGetChain chain(params);

	const char* material = nullptr;
	chain.GetValue("material", material);

	if (!material || !material[0])
	{
		CryLogWarningAlways("No material specified");
		return pH->EndFunction();
	}

	IMaterial* pMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial(material, false);
	if (!pMaterial)
	{
		CryLogWarningAlways("No such material '%s' exists", material);
		return pH->EndFunction();
	}

	Vec3 pos = Vec3(ZERO);
	Vec3 normal = Vec3(ZERO);
	Vec3 dir = Vec3(ZERO);
	bool adjust = false;
	bool skipoverlaptest = false;
	float size = 1.0f;
	float angle = 0.0f;
	float time = 1.0f;

	chain.GetValue("pos", pos);
	chain.GetValue("normal", normal);
	chain.GetValue("dir", dir);
	chain.GetValue("adjust", adjust);
	chain.GetValue("skipoverlaptest", skipoverlaptest);
	chain.GetValue("size", size);
	chain.GetValue("time", time);

	CryEngineDecalInfo decal;

	bool foundPos = false;
	const char* boneName = nullptr;
	if (chain.GetValue("bone", boneName))
	{
		ISkeletonPose* pISkeletonPose = pCharacter->GetISkeletonPose();
		if (pISkeletonPose)
		{
			const auto id = pISkeletonPose->GetJointIDByName(boneName);
			if (id != -1)
			{
				decal.vPos = (pEntity->GetWorldPos() + pCharacter->GetISkeletonPose()->GetAbsJointByID(id).t);
				foundPos = true;
			}
			else
				CryLogWarningAlways("No such bone '%s' exists", boneName);
		}
		else
			CryLogWarningAlways("Entity '%s' has no skeletonpose", pEntity->GetName());
	}

	if (chain.GetValue("angle", angle))
		decal.fAngle = angle;
	else
		decal.fAngle = RAD2DEG(acos_tpl(normal.Dot(dir)));

	decal.ownerInfo.pRenderNode = pRenderNode;

	if (!foundPos)
	{
		decal.vPos = pos;
	}
	decal.vNormal = normal;
	decal.vHitDirection = dir;
	decal.bAdjustPos = adjust;
	decal.bSkipOverlappingTest = skipoverlaptest;
	strcpy(decal.szMaterialName, material);
	decal.fSize = size;
	decal.fLifeTime = time;

	pCharacter->EnableDecals(1);
	pCharacter->CreateDecal(decal);

	return pH->EndFunction(true);
}

int ScriptBind_CPPAPI::EnableActionFilter(IFunctionHandler* pH, const char* name, bool enable)
{
	IActionMapManager* pMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionFilter* pFilter = pMapManager ? pMapManager->GetActionFilter(name) : nullptr;
	if (pFilter)
	{
		pFilter->Enable(enable);

		return pH->EndFunction(true);

	}
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::IsActionFilterEnabled(IFunctionHandler* pH, const char* name)
{
	IActionMapManager* pMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionFilter* pFilter = pMapManager ? pMapManager->GetActionFilter(name) : nullptr;
	if (pFilter)
	{
		return pH->EndFunction(pFilter->Enabled());
	}
	return pH->EndFunction();
}

int ScriptBind_CPPAPI::IsActionFilterAvailable(IFunctionHandler* pH, const char* name)
{
	IActionMapManager* pMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();
	IActionFilter* pFilter = pMapManager ? pMapManager->GetActionFilter(name) : nullptr;
	if (pFilter)
	{
		return pH->EndFunction(true);
	}
	return pH->EndFunction(false);
}

int ScriptBind_CPPAPI::CreateActionFilter(IFunctionHandler* pH, const char* name, SmartScriptTable actions)
{
	IActionMapManager* pMapManager = gEnv->pGame->GetIGameFramework()->GetIActionMapManager();

	if (pMapManager->GetActionFilter(name))
	{
		CryLogWarningAlways("CPPAPI.CreateActionFilter: a filter named `%s` already exsists!", name);
		return pH->EndFunction();
	}
	std::vector<std::string> validActions;

	IScriptTable::Iterator it = actions->BeginIteration();
	while (actions->MoveNext(it))
	{
		if (it.value.GetVarType() == svtString)
		{
			const char* actionName = it.value.str;
			if (actionName && actionName[0] != '\0')
			{
				bool found = false;
				IActionMapIteratorPtr iter = pMapManager->CreateActionMapIterator();
				while (IActionMap* pMap = iter->Next())
				{
					IActionMapBindInfoIteratorPtr pIter = pMap->CreateBindInfoIterator();
					while (const SActionMapBindInfo* pInfo = pIter->Next())
					{
						if (strcmp(pInfo->action, actionName) == 0)
						{
							validActions.push_back(actionName);
							found = true;
							break;
						}
					}
					if (found)
						break;
				}
				if (!found)
				{
					CryLogWarningAlways("CPPAPI.CreateActionFilter: actionName %s not available", actionName);
				}
			}
		}
		else
		{
			CryLogWarningAlways("CPPAPI.CreateActionFilter: table must contain string values only");
		}
	}
	actions->EndIteration(it);

	if (!validActions.empty())
	{
		IActionFilter* pFilter = pMapManager->CreateActionFilter(name, EActionFilterType::eAFT_ActionFail);
		if (pFilter)
		{
			for (const auto& action : validActions)
			{
				pFilter->Filter(ActionId(action.c_str()));
			}
			return pH->EndFunction(true);
		}
	}

	CryLogWarningAlways("CPPAPI.CreateActionFilter: no valid actions found in the table");
	return pH->EndFunction(false);
}

int ScriptBind_CPPAPI::GetIP(IFunctionHandler* pH, const char* host) {
	return pH->EndFunction(WinAPI::GetIP(host).c_str());
}

int ScriptBind_CPPAPI::GetLocalIP(IFunctionHandler* pH) {
	return pH->EndFunction(WinAPI::GetLocalIP().c_str());
}

int ScriptBind_CPPAPI::GetTime(IFunctionHandler* pH, int future) {
	time_t t = time(0) + future;
	tm *info = localtime(&t);
	static char bf[64];
	sprintf(bf, "%04d%02d%02d%02d%02d%02d", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
	return pH->EndFunction(bf);
}