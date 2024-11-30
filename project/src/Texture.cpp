#include "Texture.h"
#include "Vector2.h"
#include <SDL_image.h>
#include <iostream>

namespace dae
{
	Texture::Texture(SDL_Surface* pSurface) :
		m_pSurface{ pSurface },
		m_pSurfacePixels{ (uint32_t*)pSurface->pixels }
	{
	}

	Texture::~Texture()
	{
		if (m_pSurface)
		{
			SDL_FreeSurface(m_pSurface);
			m_pSurface = nullptr;
		}
	}

	Texture* Texture::LoadFromFile(const std::string& path)
	{
		//TODO
		//Load SDL_Surface using IMG_LOAD
		//Create & Return a new Texture Object (using SDL_Surface)
		SDL_Surface* loadedSurface = IMG_Load(path.c_str());
		if (!loadedSurface) {
			std::cerr << "Unable to load image" << std::endl;
			return nullptr;
		}

		Texture* texturePtr{ new Texture(loadedSurface) };

		return texturePtr;
	}

	ColorRGB Texture::Sample(const Vector2& uv) const
	{
		//TODO
		//Sample the correct texel for the given uv
		int width{ m_pSurface->w };
		int height{ m_pSurface->h };
		int U{ static_cast<int>(uv.x * width) };
		int V{ static_cast<int>(uv.y * height) };
		int pixelIndex{ U + (V * width) };

		Uint8 red{}, green{}, blue{};

		SDL_GetRGB(m_pSurfacePixels[pixelIndex], m_pSurface->format, &red, &green, &blue);
		ColorRGB texelColor{ red / 255.f, green / 255.f, blue / 255.f };

		return texelColor;
	}
}