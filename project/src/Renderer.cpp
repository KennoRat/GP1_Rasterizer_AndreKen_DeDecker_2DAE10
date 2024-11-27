//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"
#include "vector"

using namespace dae;

Renderer::Renderer(SDL_Window* pWindow) :
	m_pWindow(pWindow)
{
	//Initialize
	SDL_GetWindowSize(pWindow, &m_Width, &m_Height);

	//Create Buffers
	m_pFrontBuffer = SDL_GetWindowSurface(pWindow);
	m_pBackBuffer = SDL_CreateRGBSurface(0, m_Width, m_Height, 32, 0, 0, 0, 0);
	m_pBackBufferPixels = (uint32_t*)m_pBackBuffer->pixels;

	//m_pDepthBufferPixels = new float[m_Width * m_Height];

	//Initialize Camera
	m_Camera.Initialize(60.f, { .0f,.0f,-10.f });
}

Renderer::~Renderer()
{
	//delete[] m_pDepthBufferPixels;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Cleat Backbuffer
	Uint32 clearColor = SDL_MapRGB(m_pBackBuffer->format, 100, 100, 100);
	SDL_FillRect(m_pBackBuffer, NULL, clearColor);

	//Depth Buffer
	std::vector <float> depthBuffer(m_Width * m_Height, std::numeric_limits<float>::max());

	//WorldSpace
	std::vector<Vertex> vertices_world
	{
		//Traingle 0
		{Vector3{0.0f, 2.0f, 0.0f}, colors::Red },
		{Vector3{1.5f, -1.0f, 0.0f}, colors::Red},
		{Vector3{-1.5f, -1.0f, 0.0f}, colors::Red},

		//Triangle 1
		{Vector3{0.0f, 4.0f, 2.0f}, colors::Red },
		{Vector3{3.0f, -2.0f, 2.0f}, colors::Green},
		{Vector3{-3.0f, -2.0f, 2.0f}, colors::Blue}
	};

	//ViewSpace
	std::vector<Vertex> vertices_vp(vertices_world.size());
	VertexTransformationFunction(vertices_world, vertices_vp);

	//ScreenSpace Vertices
	std::vector<Vertex> vertices_sp(vertices_vp.size());
	for (int i{}; i < vertices_vp.size(); i++)
	{
		vertices_sp[i].position.x = ((vertices_vp[i].position.x + 1) / 2.f) * float(m_Width);
		vertices_sp[i].position.y = ((1 - vertices_vp[i].position.y) / 2.f) * float(m_Height);
		vertices_sp[i].position.z = vertices_vp[i].position.z;
		vertices_sp[i].color = vertices_vp[i].color;
	}

	//Bounding Boxes
	std::vector<Vector2> topLeft(vertices_sp.size() / 3, Vector2{float(m_Width), float(m_Height)});
	std::vector<Vector2> bottomRight(vertices_sp.size() / 3);

	ColorRGB finalColor{};

	//RENDER LOGIC	
	for (int triangleIdx{}; triangleIdx < (vertices_sp.size() / 3); ++triangleIdx)
	{
		const int tIdx{ triangleIdx * 3 };
		//Bounding Boxes
		BoundingBox(vertices_sp, topLeft, bottomRight, triangleIdx);

		//Render Pixel
		for (int px{ int(topLeft[triangleIdx].x) }; px < int(bottomRight[triangleIdx].x); ++px)
		{
			for (int py{ int(topLeft[triangleIdx].y) }; py < int(bottomRight[triangleIdx].y); ++py)
			{
				bool changedColor{ false };

				//Pixel
				Vector2 P{ px + 0.5f, py + 0.5f };

				//Area Of Parallelogram and Weights
				std::vector<float> a_parallelogram(vertices_sp.size() / 3);
				std::vector<float> weights(vertices_sp.size());
				
				//Triangle Hit Test and Barycentric Coordinates
				if (TriangleHitTest(vertices_sp, weights, a_parallelogram, P, triangleIdx))
				{
					float z_value{};
					for (int weightIdx{}; weightIdx < 3; ++weightIdx)
					{
						weights[weightIdx + tIdx] = weights[weightIdx + tIdx] / a_parallelogram[triangleIdx];
						z_value += weights[weightIdx + tIdx] * vertices_sp[weightIdx + tIdx].position.z;
					}

					if (depthBuffer[px + (py * m_Width) - 1] > z_value)
					{
						depthBuffer[px + (py * m_Width) - 1] = z_value;
						changedColor = true;

						finalColor = { vertices_sp[tIdx].color * weights[tIdx]
							+ vertices_sp[1 + tIdx].color * weights[1 + tIdx]
							+ vertices_sp[2 + tIdx].color * weights[2 + tIdx] };
					}
				}
				
				//Update Color in Buffer
				if(changedColor)
				{
					finalColor.MaxToOne();

					m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
						static_cast<uint8_t>(finalColor.r * 255),
						static_cast<uint8_t>(finalColor.g * 255),
						static_cast<uint8_t>(finalColor.b * 255));
				}
			}
		}
	}

	//@END
	//Update SDL Surface
	SDL_UnlockSurface(m_pBackBuffer);
	SDL_BlitSurface(m_pBackBuffer, 0, m_pFrontBuffer, 0);
	SDL_UpdateWindowSurface(m_pWindow);
}

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex>& vertices_out) const
{
	//Todo > W1 Projection Stage
	const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);

	for (int i = 0; i < vertices_in.size(); i++)
	{
		//Step1
		vertices_out[i].position = m_Camera.viewMatrix.TransformPoint(vertices_in[i].position);
		//Step2
		vertices_out[i].position.x = vertices_out[i].position.x / vertices_out[i].position.z;
		vertices_out[i].position.y = vertices_out[i].position.y / vertices_out[i].position.z;
		vertices_out[i].position.z = vertices_out[i].position.z;
		//Step3
		vertices_out[i].position.x = vertices_out[i].position.x / (aspectRatio * m_Camera.fov);
		vertices_out[i].position.y = vertices_out[i].position.y / m_Camera.fov;

		vertices_out[i].color = vertices_in[i].color;
	}
}

bool dae::Renderer::TriangleHitTest(const std::vector<Vertex>& vertices_in, std::vector<float>& weights, std::vector<float>& area, const Vector2 P, const int indexStart)
{
	bool pixelIsInsideTri{ false };
	const int tIdx{ indexStart * 3 };

	for (int edgeIdx{}; edgeIdx < 3; ++edgeIdx)
	{
		Vector3 e{};
		if (edgeIdx == 2)
		{
			e = vertices_in[tIdx].position
				- vertices_in[2 + tIdx].position;
		}
		else
		{
			e = vertices_in[edgeIdx + 1 + tIdx].position
				- vertices_in[edgeIdx + tIdx].position;
		}

		Vector2 pV{ P.x - vertices_in[edgeIdx + tIdx].position.x,
			P.y - vertices_in[edgeIdx + tIdx].position.y };

		float result{ Vector2::Cross(Vector2{ e.x, e.y }, pV) };
		if (result < 0)
		{
			pixelIsInsideTri = false;
			break;
		}
		else
		{
			if (edgeIdx == 1) weights[tIdx] = result;
			else if (edgeIdx == 2) weights[1 + tIdx] = result;
			else weights[2 + tIdx] = result;
			area[indexStart] += result;
			pixelIsInsideTri = true;
		}
	}
	return pixelIsInsideTri;
}

void dae::Renderer::BoundingBox(const std::vector<Vertex>& vertices_in, std::vector<Vector2>& topLeft, std::vector<Vector2>& bottomRight, const int indexStart)
{
	const int tIdx{ indexStart * 3 };

	for (int edgeIdx{}; edgeIdx < 3; ++edgeIdx)
	{
		topLeft[indexStart].x = std::clamp(int(std::min(topLeft[indexStart].x, vertices_in[tIdx + edgeIdx].position.x)), 0, m_Width - 1);
		topLeft[indexStart].y = std::clamp(int(std::min(topLeft[indexStart].y, vertices_in[tIdx + edgeIdx].position.y)), 0, m_Height - 1);
		bottomRight[indexStart].x = std::clamp(int(std::max(bottomRight[indexStart].x, vertices_in[tIdx + edgeIdx].position.x)), 0, m_Width - 1);
		bottomRight[indexStart].y = std::clamp(int(std::max(bottomRight[indexStart].y, vertices_in[tIdx + edgeIdx].position.y)), 0, m_Height - 1);
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
