
#include <string>
#include <vector>

#include "CryCommon/CrySystem/ISystem.h"
#include "CryCommon/CryRenderer/IRenderer.h"
#include "DrawTools.h"

DrawTools::DrawTools()
{
}

DrawTools::~DrawTools()
{
}

void DrawTools::OnUpdate()
{
	for (const auto& image : m_ImageData)
	{
		if (image.colorBox)
		{
			const float alpha = image.color[3];
			if (alpha < 1.0f)
			{
				gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST); //needed for alpha control
			}
			gEnv->pRenderer->Draw2dImage(image.posX, image.posY, image.width, image.height, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 
				image.color[0], image.color[1], image.color[2], alpha);
		}
		else // Images
		{
			// Old:
			//gEnv->pRenderer->Draw2dImage(image.posX, image.posY, image.width, image.height, image.textureId, 0.0f, 1.0f, 1.0f, 0.0f); 
			
			const float alpha = image.color[3];
			gEnv->pRenderer->SetState(GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA | GS_NODEPTHTEST);// pdn:BC3 / DXT5
			gEnv->pRenderer->Draw2dImage(
				image.posX, image.posY,
				image.width, image.height,
				image.textureId,
				//0.0f, 0.0f, 1.0f, 1.0f,  //  UVs
				0.0f, 1.0f, 1.0f, 0.0f,
				0.0f,                    // angle
				1.0f, 1.0f, 1.0f, alpha //1.0f   // r,g,b,a
			);
		}
	}

	//Render text on top of pictures
	for (const auto& text : m_TextData)
	{
		SDrawTextInfo pDrawTexInfo;
		pDrawTexInfo.color[0] = text.color[0];
		pDrawTexInfo.color[1] = text.color[1];
		pDrawTexInfo.color[2] = text.color[2];
		pDrawTexInfo.color[3] = text.color[3];
		pDrawTexInfo.xscale = text.xscale;
		pDrawTexInfo.yscale = text.yscale;

		gEnv->pRenderer->Draw2dText(text.posX, text.posY, text.text.c_str(), pDrawTexInfo);
	}
}

void DrawTools::OnDisconnect(int reason, const char* message)
{
	ClearScreen();
}

int DrawTools::Add(Text text)
{
	if (text.id)
	{
		//id specified, try to update existing 
		for (auto& t : m_TextData)
		{
			if (t.id == text.id)
			{
				t.color[0] = text.color[0];
				t.color[1] = text.color[1];
				t.color[2] = text.color[2];
				t.color[3] = text.color[3];
				t.posX = text.posX;
				t.posY = text.posY;
				t.text = text.text;
				t.xscale = text.xscale;
				t.yscale = text.yscale;
				return text.id;
			}
		}
	}

	++m_idGenerator;
	text.id = m_idGenerator;

	m_TextData.push_back(std::move(text));

	return m_idGenerator;
}


int DrawTools::Add(Image image, const char *texturePath)
{
	if (!texturePath)
		return -1;

	ITexture* pTexture = gEnv->pRenderer->EF_LoadTexture(texturePath, FT_FROMIMAGE, eTT_2D);
	if (!pTexture)
		return -1;

	return Add(image, pTexture->GetTextureID());
}

int DrawTools::Add(Image image, int textureId)
{
	if (image.id)
	{
		//id specified, try to update existing 
		for (auto& i : m_ImageData)
		{
			if (i.id == image.id)
			{
				i.height = image.height;
				i.posX = image.posX;
				i.posY = image.posY;
				i.textureId = image.textureId;
				i.width = image.width;

				return i.id;
			}
		}
	}

	image.textureId = textureId;

	++m_idGenerator;
	image.id = m_idGenerator;

	m_ImageData.emplace_back(std::move(image));

	return m_idGenerator;
}

void DrawTools::RemoveTextOrImageById(int id)
{
	for (auto& image : m_ImageData)
	{
		if (id == image.id)
		{
			gEnv->pRenderer->RemoveTexture(image.textureId);
		}
	}

	const auto p1 = [&id](const Image& d) { return id == d.id; };
	m_ImageData.erase(std::remove_if(m_ImageData.begin(), m_ImageData.end(), p1), m_ImageData.end());

	const auto p2 = [&id](const Text& d) { return id == d.id; };
	m_TextData.erase(std::remove_if(m_TextData.begin(), m_TextData.end(), p2), m_TextData.end());

	if (m_ImageData.empty() && m_TextData.empty())
	{
		m_idGenerator = 0;
	}
}

void DrawTools::ClearScreen()
{
	for (auto& image : m_ImageData)
	{
		gEnv->pRenderer->RemoveTexture(image.textureId);
	}

	m_idGenerator = 0;
	m_ImageData.clear();
	m_TextData.clear();
}
