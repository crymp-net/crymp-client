#include "TeamSelectionPreview.h"

#include "CryGame/Game.h"
#include "CryGame/GameCVars.h"
#include "CryCommon/CrySystem/ISystem.h"

#include <algorithm>
#include <vector>
#include "CrySystem/RandomGenerator.h"
#include "CryCommon/CryAnimation/ICryAnimation.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "CryCommon/CrySystem/IConsole.h"
#include "CryCommon/CrySystem/IFlashPlayer.h"
#include "CryCommon/CrySystem/IHardwareMouse.h"

namespace
{
//CryMP Cache standalone characters across HUD reloads
class CTeamSelectionPreview
{
public:
	static CTeamSelectionPreview& Get()
	{
		static CTeamSelectionPreview instance;
		return instance;
	}

	void Reset()
	{
		for (int side = 0; side < 2; ++side)
		{
			m_pCharacters[side] = nullptr;
			m_configured[side] = false;
			m_flagHover[side] = 0.f;
			m_lastHover[side].clear();
			m_requested[side].clear();
			m_hoverRequested[side].clear();
		}

		m_active = false;
		m_valid = false;
		m_pEnabled = nullptr;
	}

	void Suspend()
	{
		if (!m_active)
		{
			return;
		}
		for (int side = 0; side < 2; ++side)
		{
			ICharacterInstance* pCharacter = m_pCharacters[side];
			if (pCharacter)
			{
				pCharacter->GetISkeletonAnim()->StopAnimationInLayer(2, 0.f);
				pCharacter->GetISkeletonAnim()->StopAnimationInLayer(3, 0.f);
				pCharacter->SetFlags(0);
				pCharacter->GetISkeletonPose()->SetForceSkeletonUpdate(0);
			}
			m_wasHovered[side] = false;
			m_hoverWeight[side] = 0.f;
			m_headActive[side][1] = false;
			m_flagHover[side] = 0.f;
		}
		m_active = false;
	}

	void Preload()
	{
		if (!gEnv->bClient || !gEnv->bMultiplayer)
		{
			return;
		}
		Prepare();
		//CryMP Keep preloaded characters dormant until the menu opens
		m_active = true;
		Suspend();
	}

	void Draw(IFlashPlayer* pFlash, float frameTime)
	{
		int fx;
		int fy;
		int fw;
		int fh;
		float aspect;
		pFlash->GetViewport(fx, fy, fw, fh, aspect);
		const float scale = float(fh) / 768.f;
		if (scale <= 0.f)
		{
			Suspend();
			return;
		}
		float mouseX = -1.f;
		float mouseY = -1.f;
		if (gEnv->pHardwareMouse)
		{
			gEnv->pHardwareMouse->GetHardwareMouseClientPosition(&mouseX, &mouseY);
		}
		UpdateFlagHover(pFlash, frameTime, (mouseX - fx) / scale, (mouseY - fy) / scale);
		bool ready = Prepare();
		ready = ready && m_valid;
		pFlash->SetVariable("Root.Animation1._visible", SFlashVarValue(!ready));
		pFlash->SetVariable("Root.Animation2._visible", SFlashVarValue(!ready));
		if (!ready)
		{
			Suspend();
			return;
		}
		if (!m_active)
		{
			for (int side = 0; side < 2; ++side)
			{
				if (m_requested[side].find(',') != string::npos)
				{
					ConfigureAnimations(side, m_requested[side], m_hoverRequested[side]);
				}
				m_pCharacters[side]->SetFlags(CS_FLAG_DRAW_MODEL | CS_FLAG_UPDATE_ONRENDER);
			}
			m_active = true;
		}

		IRenderer* pRenderer = gEnv->pRenderer;
		int vx;
		int vy;
		int vw;
		int vh;
		pRenderer->GetViewport(&vx, &vy, &vw, &vh);
		const CCamera savedCamera = pRenderer->GetCamera();
		const float delta = max(0.f, frameTime);

		const float distance = max(1.f, m_pDistance->GetFVal());
		const float step = delta / BlendTime;

		for (int i = 0; i < 2; ++i)
		{
			const int x = fx + int((i ? 535.f : 284.f) * scale);
			const int y = fy + int(225.f * scale);
			const int w = max(1, int(202.f * scale));
			const int h = max(1, int(330.f * scale));
			const bool hovered = m_hoverValid[i] && mouseX >= x && mouseX < x + w && mouseY >= y && mouseY < y + h;
			if (hovered != m_wasHovered[i])
			{
				ISkeletonAnim* pAnimation = m_pCharacters[i]->GetISkeletonAnim();
				if (hovered)
				{
					m_hoverAnimation[i] = SelectAnimation(m_pCharacters[i], m_hoverRequested[i], "combat_weaponPunchIdleUB_nw_01", m_lastHover[i]);
					m_lastHover[i] = m_hoverAnimation[i];
					m_hoverTime[i] = 0;
					m_loopCycle[i][1] = -1;
					m_headActive[i][1] = false;
					pAnimation->StopAnimationInLayer(3, 0.f);
					StartLayer(m_pCharacters[i], m_hoverAnimation[i].c_str(), 2, BlendTime);
				}
				else
				{
					pAnimation->StopAnimationInLayer(2, BlendTime);
					pAnimation->StopAnimationInLayer(3, BlendTime);
					m_headActive[i][1] = false;
				}
			}

			m_wasHovered[i] = hovered;
			m_hoverWeight[i] = hovered ? min(1.f, m_hoverWeight[i] + step) : max(0.f, m_hoverWeight[i] - step);
			m_hoverTime[i] += delta;
			m_idleTime[i] += delta;

			CCamera camera;
			Matrix34 cameraMatrix(IDENTITY);
			cameraMatrix.SetTranslation(Vec3(0, -3.2f, .87f));
			camera.SetMatrix(cameraMatrix);
			camera.SetFrustum(w, h, DEG2RAD(33.f), .05f, 20.f);
			pRenderer->SetViewport(x, y, w, h);
			pRenderer->SetScissor(x, y, w, h);
			pRenderer->SetCamera(camera);
			ICharacterInstance* pCharacter = m_pCharacters[i];
			SetLoop(i, pCharacter, 0, m_animation[i].c_str(), m_idleTime[i], true);
			if (m_hoverValid[i] && m_hoverWeight[i] > 0.f)
			{
				SetLoop(i, pCharacter, 2, m_hoverAnimation[i].c_str(), m_hoverTime[i], hovered);
			}
			const QuatTS location = Pose(pCharacter, camera);
			const Vec3 center(i == 0 ? -.04f : -.01f, 0.f, .85f);
			cameraMatrix.SetTranslation(center + Vec3(0, -distance, 0));
			camera.SetMatrix(cameraMatrix);
			camera.SetFrustum(w, h, DEG2RAD(33.f), .05f, max(20.f, distance + 4.f));
			pRenderer->SetCamera(camera);

			pRenderer->EF_StartEf();
			pRenderer->ClearBuffer(FRT_CLEAR_DEPTH | FRT_CLEAR_IMMEDIATE, nullptr);
			//CryMP Keep portrait lighting independent of map sunlight
			CDLight keyLight;
			keyLight.m_Id = 0;
			keyLight.m_Flags = DLF_POINT | DLF_LIGHTSOURCE | DLF_LOCAL;
			keyLight.m_Origin = Vec3(-1.f, -2.5f, 2.7f);
			keyLight.m_fRadius = 8.f;
			keyLight.SetLightColor(ColorF(2.6f, 2.55f, 2.5f, 1.f));
			keyLight.m_SpecMult = 1.1f;
			pRenderer->EF_ADDDlight(&keyLight);
			CDLight rimLight;
			rimLight.m_Id = 0;
			rimLight.m_Flags = DLF_POINT | DLF_LIGHTSOURCE | DLF_LOCAL;
			rimLight.m_Origin = Vec3(1.5f, .4f, 2.4f);
			rimLight.m_fRadius = 6.f;
			rimLight.SetLightColor(ColorF(1.6f, 1.9f, 2.2f, 1.f));
			rimLight.m_SpecMult = .8f;
			pRenderer->EF_ADDDlight(&rimLight);
			Matrix34 world(location);
			SRendParams params;
			params.pMatrix = &world;
			params.AmbientColor = ColorF(.54f, .55f, .57f, 1.f);
			params.nDLightMask = keyLight.m_Id >= 0 && keyLight.m_Id < 32 ? (1u << keyLight.m_Id) : 0;
			if (rimLight.m_Id >= 0 && rimLight.m_Id < 32)
			{
				params.nDLightMask |= 1u << rimLight.m_Id;
			}
			params.dwFObjFlags |= FOB_NO_FOG;
			pCharacter->Render(params, QuatTS(IDENTITY));
			pRenderer->EF_EndEf3D(SHDF_SORT | SHDF_NOASYNC);
		}
		pRenderer->SetCamera(savedCamera);
		pRenderer->SetScissor();
		pRenderer->SetViewport(vx, vy, vw, vh);
	}

private:
	static constexpr float BlendTime = .3f;
	void UpdateFlagHover(IFlashPlayer* pFlash, float frameTime, float mouseX, float mouseY)
	{
		const float blend = 1.f - expf(-12.f * max(0.f, frameTime));
		for (int side = 0; side < 2; ++side)
		{
			const float left = side == 0 ? 272.4f : 532.35f;
			const bool hovered = mouseX >= left && mouseX < left + 220.15f && mouseY >= 188.05f && mouseY < 557.5f;
			m_flagHover[side] += ((hovered ? 1.f : 0.f) - m_flagHover[side]) * blend;
			pFlash->SetVariable(side == 0 ? "Root.FlagUS._alpha" : "Root.FlagNK._alpha",
								SFlashVarValue(55.f * m_flagHover[side]));
		}
	}

	bool Prepare()
	{
		BindVariables();
		bool ready = m_pEnabled && m_pEnabled->GetIVal() != 0;
		const char* models[2] = {"objects/characters/human/us/nanosuit/nanosuit_us_multiplayer.cdf",
								  "objects/characters/human/asian/nanosuit/nanosuit_asian_multiplayer.cdf"};
		for (int side = 0; ready && side < 2; ++side)
		{
			if (!m_pCharacters[side])
			{
				m_pCharacters[side] = gEnv->pCharacterManager->CreateInstance(models[side]);
			}
		}
		ready = ready && m_pCharacters[0] && m_pCharacters[1];
		if (ready)
		{
			for (int side = 0; side < 2; ++side)
			{
				const char* requested = m_pClip[side]->GetString();
				const char* hoverRequested = m_pHoverClip[side]->GetString();
				if (!m_configured[side] || m_requested[side] != requested || m_hoverRequested[side] != hoverRequested)
				{
					ConfigureAnimations(side, requested, hoverRequested);
				}
			}
		}
		return ready;
	}

	void BindVariables()
	{
		if (m_pEnabled)
		{
			return;
		}
		IConsole* pConsole = gEnv->pConsole;
		m_pEnabled = pConsole->GetCVar("hud_teamPreview3D");
		m_pClip[0] = g_pGame->GetCVars()->pHudTeamUSAnim;
		m_pClip[1] = g_pGame->GetCVars()->pHudTeamNKAnim;
		m_pHoverClip[0] = g_pGame->GetCVars()->pHudTeamUSHover;
		m_pHoverClip[1] = g_pGame->GetCVars()->pHudTeamNKHover;
		m_pDistance = pConsole->GetCVar("hud_teamPreviewDistance");
	}

	static bool StartLayer(ICharacterInstance* pCharacter, const char* name, int layer, float blendIn = 0.f)
	{
		if (!name[0] || pCharacter->GetIAnimationSet()->GetAnimIDByName(name) < 0)
		{
			return false;
		}
		CryCharAnimationParams params(layer, CA_MANUAL_UPDATE | CA_FORCE_SKELETON_UPDATE | CA_REPEAT_LAST_KEY |
												 CA_ALLOW_ANIM_RESTART | (layer >= 2 ? CA_PARTIAL_SKELETON_UPDATE : 0));
		params.m_fTransTime = 0;
		params.m_fLayerBlendIn = blendIn;
		return pCharacter->GetISkeletonAnim()->StartAnimation(name, nullptr, nullptr, nullptr, params);
	}

	static void SeekLayer(ICharacterInstance* pCharacter, int layer, float phase)
	{
		ISkeletonAnim* pAnimation = pCharacter->GetISkeletonAnim();
		if (pAnimation->GetNumAnimsInFIFO(layer) == 0)
		{
			return;
		}
		pAnimation->ManualSeekAnimationInFIFO(layer, 0, phase, false);
	}

	//CryMP Crossfade advancing tail and head poses for seamless loops
	void SetLoop(int side, ICharacterInstance* pCharacter, int layer, const char* name, float time, bool allowHead)
	{
		IAnimationSet* pAnimationSet = pCharacter->GetIAnimationSet();
		const int id = pAnimationSet->GetAnimIDByName(name);
		const float duration = max(.001f, pAnimationSet->GetDuration_sec(id));
		const float overlap = min(BlendTime, duration * .25f);
		const float period = duration - overlap;
		const int cycle = int(time / period);
		const int pair = layer / 2;
		ISkeletonAnim* pAnimation = pCharacter->GetISkeletonAnim();
		if (cycle != m_loopCycle[side][pair])
		{
			if (m_headActive[side][pair])
			{
				pAnimation->StopAnimationInLayer(layer + 1, 0.f);
			}
			m_headActive[side][pair] = false;
			m_loopCycle[side][pair] = cycle;
		}
		const float sourceTime = fmodf(time, period) + overlap;
		const float headTime = max(0.f, sourceTime - period);
		if (allowHead && sourceTime >= period && !m_headActive[side][pair])
		{
			m_headActive[side][pair] = StartLayer(pCharacter, name, layer + 1, overlap);
		}
		SeekLayer(pCharacter, layer, sourceTime / duration);
		SeekLayer(pCharacter, layer + 1, headTime / duration);
	}

	static string SelectAnimation(ICharacterInstance* pCharacter, const string& requested, const char* defaultAnimation, const string& previous)
	{
		IAnimationSet* pAnimationSet = pCharacter->GetIAnimationSet();
		std::vector<string> choices;
		int position = 0;
		while (position >= 0)
		{
			string clip = requested.Tokenize(",", position);
			clip.Trim();
			if (!clip.empty() && pAnimationSet->GetAnimIDByName(clip.c_str()) >= 0 &&
				std::find(choices.begin(), choices.end(), clip) == choices.end())
			{
				choices.push_back(clip);
			}
		}
		if (choices.empty())
		{
			return defaultAnimation;
		}
		if (choices.size() > 1)
		{
			choices.erase(std::remove(choices.begin(), choices.end(), previous), choices.end());
		}
		return choices[RandomGenerator::GenerateIndex(choices.size())];
	}

	void ConfigureAnimations(int side, const string& requested, const string& hoverRequested)
	{
		m_requested[side] = requested;
		m_hoverRequested[side] = hoverRequested;
		m_configured[side] = true;
		ICharacterInstance* pCharacter = m_pCharacters[side];
		IAnimationSet* pAnimationSet = pCharacter->GetIAnimationSet();
		m_animation[side] = SelectAnimation(pCharacter, requested, "combat_idle_nw_01", m_animation[side]);
		m_hoverAnimation[side] = SelectAnimation(pCharacter, hoverRequested, "combat_weaponPunchIdleUB_nw_01", m_lastHover[side]);
		m_idleTime[side] = 0;
		m_hoverWeight[side] = 0;
		m_hoverTime[side] = 0;
		m_wasHovered[side] = false;
		pCharacter->SetFlags(CS_FLAG_DRAW_MODEL | CS_FLAG_UPDATE_ONRENDER);
		ISkeletonAnim* pAnimation = pCharacter->GetISkeletonAnim();
		pAnimation->SetAnimationDrivenMotion(0);
		pAnimation->StopAnimationsAllLayers();
		m_baseValid[side] = StartLayer(pCharacter, m_animation[side].c_str(), 0);
		m_hoverValid[side] = pAnimationSet->GetAnimIDByName(m_hoverAnimation[side].c_str()) >= 0;
		m_valid = m_baseValid[0] && m_baseValid[1];
		for (int pair = 0; pair < 2; ++pair)
		{
			m_loopCycle[side][pair] = -1;
			m_headActive[side][pair] = false;
		}
	}

	static QuatTS Pose(ICharacterInstance* pCharacter, const CCamera& camera)
	{
		QuatTS location(IDENTITY);
		location.q = Quat::CreateRotationZ(gf_PI);
		ISkeletonPose* pSkeleton = pCharacter->GetISkeletonPose();
		pSkeleton->SetForceSkeletonUpdate(8);
		pCharacter->SkeletonPreProcess(QuatT(location.q, location.t), location, camera, 0x55);
		pCharacter->SkeletonPostProcess(QuatT(location.q, location.t), location, nullptr, 1.f, 0x55);
		//CryMP Remove root travel while preserving pelvis sway and foot planting
		const int rootJoint = pSkeleton->GetJointIDByName("Bip01");
		if (rootJoint >= 0)
		{
			const Vec3 motion = pSkeleton->GetAbsJointByID(rootJoint).t - pSkeleton->GetDefaultAbsJointByID(rootJoint).t;
			location.t = -(location.q * motion);
		}
		return location;
	}

	int m_loopCycle[2][2] = {};
	bool m_headActive[2][2] = {};
	float m_hoverWeight[2] = {};
	float m_hoverTime[2] = {};
	float m_flagHover[2] = {};
	bool m_hoverValid[2] = {};
	bool m_wasHovered[2] = {};
	ICVar* m_pEnabled = nullptr;
	ICVar* m_pClip[2] = {};
	ICVar* m_pHoverClip[2] = {};
	ICVar* m_pDistance = nullptr;
	_smart_ptr<ICharacterInstance> m_pCharacters[2];
	string m_animation[2];
	string m_hoverAnimation[2];
	string m_requested[2];
	string m_hoverRequested[2];
	bool m_configured[2] = {};
	bool m_baseValid[2] = {};
	float m_idleTime[2] = {};
	string m_lastHover[2];
	bool m_valid = false;
	bool m_active = false;
};
}

//-----------------------------------------------------------------------------------------------------

void TeamSelectionPreview::Preload()
{
	CTeamSelectionPreview::Get().Preload();
}

//-----------------------------------------------------------------------------------------------------

void TeamSelectionPreview::Reset()
{
	CTeamSelectionPreview::Get().Reset();
}

//-----------------------------------------------------------------------------------------------------

void TeamSelectionPreview::Draw(IFlashPlayer* pFlash, float frameTime)
{
	CTeamSelectionPreview::Get().Draw(pFlash, frameTime);
}

//-----------------------------------------------------------------------------------------------------

void TeamSelectionPreview::Suspend()
{
	CTeamSelectionPreview::Get().Suspend();
}
