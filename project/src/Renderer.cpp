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

	m_pDepthBufferPixels = new float[m_Width * m_Height];


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
	Uint32 clearColor = SDL_MapRGB(m_pBackBuffer->format, 0, 0, 0);
	SDL_FillRect(m_pBackBuffer, NULL, clearColor);

	//Depth Buffer
	std::fill(m_pDepthBufferPixels, m_pDepthBufferPixels + m_Width * m_Height, std::numeric_limits<float>::max());

	//Texture;
	Texture* m_ptrTexture = Texture::LoadFromFile("resources/uv_grid_2.png");

	//WorldSpace
	std::vector<Mesh> meshes_world
	{
		Mesh
		{
		{
			//Vertexes
			{Vector3{-3, 3, -2}, colors::White	, Vector2{0.0f, 0.0f}},
			{Vector3{0, 3, -2	}, colors::White, Vector2{0.5f, 0.0f}},
			{Vector3{3, 3, -2	}, colors::White, Vector2{1.0f, 0.0f}},
			{Vector3{-3, 0, -2	}, colors::White, Vector2{0.0f, 0.5F}},
			{Vector3{0, 0, -2	}, colors::White, Vector2{0.5f, 0.5f}},
			{Vector3{3, 0, -2	}, colors::White, Vector2{1.0f, 0.5f}},
			{Vector3{-3, -3, -2}, colors::White	, Vector2{0.0f, 1.0f}},
			{Vector3{0, -3, -2	}, colors::White, Vector2{0.5f, 1.0f}},
			{Vector3{3, -3, -2	}, colors::White, Vector2{1.0f, 1.0f}}
		},
		{
			//Indices
			3,0,4,1,5,2,
			2,6, //degenerate triangles
			6,3,7,4,8,5
		},
		PrimitiveTopology::TriangleStrip
		}
	};

	//std::vector<Mesh> meshes_world
	//{
	//	Mesh
	//	{
	//	{
	//		//Vertexes
	//		{Vector3{-3, 3, -2}, colors::White},
	//		{Vector3{0, 3, -2	}, colors::White},
	//		{Vector3{3, 3, -2	}, colors::White},
	//		{Vector3{-3, 0, -2	}, colors::White},
	//		{Vector3{0, 0, -2	}, colors::White},
	//		{Vector3{3, 0, -2	}, colors::White},
	//		{Vector3{-3, -3, -2}, colors::White},
	//		{Vector3{0, -3, -2	}, colors::White},
	//		{Vector3{3, -3, -2	}, colors::White}
	//	},
	//	{
	//		//Indices
	//		3,0,1, 1,4,3, 4,1,2,
	//		2,5,4, 6,3,4, 4,7,6,
	//		7,4,5, 5,8,7
	//	},
	//		PrimitiveTopology::TriangleList
	//	}
	//};

	std::vector<Vertex> vertices_world(100);

	//Index Buffer List or Strip
	IndexBuffer(vertices_world, meshes_world);

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
		vertices_sp[i].uv = vertices_vp[i].uv;
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
					//UV texture
					Vector2 UV{};
					//Depth
					float z_interpolated{};
					for (int weightIdx{}; weightIdx < 3; ++weightIdx)
					{
						weights[weightIdx + tIdx] = weights[weightIdx + tIdx] / a_parallelogram[triangleIdx];
						z_interpolated += (1.f/vertices_sp[weightIdx + tIdx].position.z) * weights[weightIdx + tIdx];

						UV += weights[weightIdx + tIdx] * (vertices_sp[weightIdx + tIdx].uv / vertices_sp[weightIdx + tIdx].position.z);
					}

					z_interpolated = 1.f / z_interpolated;
					//Multiplying correct depth
					UV = UV * z_interpolated;

					if (m_pDepthBufferPixels[px + (py * m_Width) - 1] > z_interpolated)
					{
						m_pDepthBufferPixels[px + (py * m_Width) - 1] = z_interpolated;
						changedColor = true;

						finalColor = m_ptrTexture->Sample(UV);
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

		//UV
		vertices_out[i].uv = vertices_in[i].uv;
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

void dae::Renderer::IndexBuffer(std::vector<Vertex>& vertices_out, const std::vector<Mesh>& meshes_in)
{
	for(int meshIndex{}; meshIndex < meshes_in.size(); ++meshIndex)
	{
		if (meshes_in[meshIndex].primitiveTopology == PrimitiveTopology::TriangleList)
		{
			for(int VertexIndex{}; VertexIndex < meshes_in[meshIndex].indices.size(); ++VertexIndex)
			{
				int Indices{ int(meshes_in[meshIndex].indices[VertexIndex])};
				vertices_out[VertexIndex].position = meshes_in[meshIndex].vertices[Indices].position;
				vertices_out[VertexIndex].color = meshes_in[meshIndex].vertices[Indices].color;
				vertices_out[VertexIndex].uv = meshes_in[meshIndex].vertices[Indices].uv;
			}
		}
		else if(meshes_in[meshIndex].primitiveTopology == PrimitiveTopology::TriangleStrip)
		{
			for (int VertexIndex{}; VertexIndex < 12; ++VertexIndex)
			{
				int TriangleIndex{ VertexIndex * 3};
				if (VertexIndex % 2 == 1)
				{
					//Switch vertices
					for (int index{}; index < 3; ++index)
					{
						int SwitchIndex{};

						if (index == 1) SwitchIndex = int(meshes_in[meshIndex].indices[VertexIndex + 2]);
						else if (index == 2) SwitchIndex = int(meshes_in[meshIndex].indices[VertexIndex + 1]);
						else SwitchIndex = int(meshes_in[meshIndex].indices[VertexIndex]);

						vertices_out[TriangleIndex + index].position = meshes_in[meshIndex].vertices[SwitchIndex].position;
						vertices_out[TriangleIndex + index].color = meshes_in[meshIndex].vertices[SwitchIndex].color;
						vertices_out[TriangleIndex + index].uv = meshes_in[meshIndex].vertices[SwitchIndex].uv;
					}
				}
				else
				{
					for(int index{}; index < 3; ++index)
					{
						int Indices{ int(meshes_in[meshIndex].indices[VertexIndex + index]) };
						vertices_out[TriangleIndex + index].position = meshes_in[meshIndex].vertices[Indices].position;
						vertices_out[TriangleIndex + index].uv = meshes_in[meshIndex].vertices[Indices].uv;
					}
				}
			}
		}
	}
	
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
