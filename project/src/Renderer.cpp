//External includes
#include "SDL.h"
#include "SDL_surface.h"

//Project includes
#include "Renderer.h"
#include "Maths.h"
#include "Texture.h"
#include "Utils.h"
#include "vector"
#include "iostream"

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
	m_Camera.Initialize(60.f, { 0.0f, 5.0f,-30.f }, m_Width, m_Height);
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_ptrTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	const auto yawAngle = ((cos(pTimer->GetTotal()) + 1.f) * M_PI) + M_PI;

	m_WorldMatrix = Matrix::CreateRotationY(yawAngle);
}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Clear Backbuffer
	Uint32 clearColor = SDL_MapRGB(m_pBackBuffer->format, 0, 0, 0);
	SDL_FillRect(m_pBackBuffer, NULL, clearColor);

	//Depth Buffer
	std::fill(m_pDepthBufferPixels, m_pDepthBufferPixels + m_Width * m_Height, std::numeric_limits<float>::max());

	// WorldSpace
	//std::vector<Mesh> meshes_world
	//{
	//	Mesh
	//	{
	//	{
	//		//Vertexes
	//		{Vector3{-3, 3, -2},   colors::White, Vector2{0.0f, 0.0f}},
	//		{Vector3{0, 3, -2	}, colors::White, Vector2{0.5f, 0.0f}},
	//		{Vector3{3, 3, -2	}, colors::White, Vector2{1.0f, 0.0f}},
	//		{Vector3{-3, 0, -2	}, colors::White, Vector2{0.0f, 0.5F}},
	//		{Vector3{0, 0, -2	}, colors::White, Vector2{0.5f, 0.5f}},
	//		{Vector3{3, 0, -2	}, colors::White, Vector2{1.0f, 0.5f}},
	//		{Vector3{-3, -3, -2},  colors::White, Vector2{0.0f, 1.0f}},
	//		{Vector3{0, -3, -2	}, colors::White, Vector2{0.5f, 1.0f}},
	//		{Vector3{3, -3, -2	}, colors::White, Vector2{1.0f, 1.0f}}
	//	},
	//	{
	//		//Indices
	//		3,0,4,1,5,2,
	//		2,6, //degenerate triangles
	//		6,3,7,4,8,5
	//	},
	//	PrimitiveTopology::TriangleStrip
	//	}
	//};

	std::vector<Mesh> meshes_world(1);
	if (!Utils::ParseOBJ("resources/tuktuk.obj", meshes_world[0].vertices, meshes_world[0].indices), false) {
		std::cerr << "Failed to load OBJ file!" << std::endl;
	}

	//Index Buffer List or Strip
	std::vector<Vertex> vertices_world{};
	vertices_world.reserve(meshes_world[0].indices.size());
	IndexBuffer(vertices_world, meshes_world);

	//ViewSpace
	std::vector<Vertex_Out> vertices_vp(vertices_world.size());
	VertexTransformationFunction(vertices_world, vertices_vp);

	//ScreenSpace Vertices
	std::vector<Vertex_Out> vertices_sp(vertices_vp.size());
	for (int i{}; i < vertices_vp.size(); i++)
	{
		vertices_sp[i].position.x = ((vertices_vp[i].position.x + 1) / 2.f) * float(m_Width);
		vertices_sp[i].position.y = ((1 - vertices_vp[i].position.y) / 2.f) * float(m_Height);
		vertices_sp[i].position.z = vertices_vp[i].position.z;
		vertices_sp[i].position.w = vertices_vp[i].position.w;
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
		//Frustrum culling
		if (FrustrumCulling(vertices_sp, tIdx)) continue;

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
					// UV and depth interpolation
					Vector2 UV_Numerator{};
					float UV_Denominator = 0.f;

					float z_Numerator = 0.f; 
					float z_Denominator = 0.f;

					for (int weightIdx = 0; weightIdx < 3; ++weightIdx) {
						float invW = 1.f / vertices_sp[weightIdx + tIdx].position.w;

						// UV Interpolation
						UV_Numerator += weights[weightIdx + tIdx] * (vertices_sp[weightIdx + tIdx].uv * invW);
						UV_Denominator += weights[weightIdx + tIdx] * invW;

						// Depth (z) Interpolation
						z_Numerator += weights[weightIdx + tIdx] * (vertices_sp[weightIdx + tIdx].position.z * invW);
						z_Denominator += weights[weightIdx + tIdx] * invW;
					}

					// Final UV
					Vector2 UV = UV_Numerator / UV_Denominator;

					UV.x = std::clamp(UV.x, 0.0f, 1.0f);
					UV.y = std::clamp(UV.y, 0.0f, 1.0f);

					// Final perspective-correct depth (z)
					float z_interpolated = z_Numerator / z_Denominator;

					z_interpolated = std::clamp(z_interpolated, 0.0f, 1.0f);

					// Depth buffer comparison
					if ((z_interpolated >= 0 && z_interpolated <= 1) &&
						(m_pDepthBufferPixels[px + (py * m_Width) - 1] > z_interpolated))
					{
						m_pDepthBufferPixels[px + (py * m_Width) - 1] = z_interpolated;
						changedColor = true;

						if (m_DepthColor) {
							finalColor.r = Utils::Remap(z_interpolated, 0.995f, 1.f);
							finalColor.g = finalColor.r;
							finalColor.b = finalColor.r;
						}
						else {
							finalColor = m_ptrTexture->Sample(UV);
						}
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

void Renderer::VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex_Out>& vertices_out) const
{
	//Todo > W1 Projection Stage
	const float aspectRatio = static_cast<float>(m_Width) / static_cast<float>(m_Height);

	const Matrix WVPMatrix{ m_WorldMatrix * m_Camera.viewMatrix * m_Camera.projectionMatrix};

	for (int i = 0; i < vertices_in.size(); i++)
	{
		// Transform the vertex
		Vector4 transformed = WVPMatrix.TransformPoint(
			vertices_in[i].position.x,
			vertices_in[i].position.y,
			vertices_in[i].position.z,
			0);

		// Perspective divide by w (not z)
		transformed.x /= transformed.w;
		transformed.y /= transformed.w;
		transformed.z /= transformed.w;

		// Store w for later depth calculations
		vertices_out[i].position = { transformed.x, transformed.y, transformed.z, transformed.w };
		vertices_out[i].color = vertices_in[i].color;
		vertices_out[i].uv = vertices_in[i].uv;
	}
}

bool dae::Renderer::TriangleHitTest(const std::vector<Vertex_Out>& vertices_in, std::vector<float>& weights, std::vector<float>& area, const Vector2 P, const int indexStart)
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

void dae::Renderer::BoundingBox(const std::vector<Vertex_Out>& vertices_in, std::vector<Vector2>& topLeft, std::vector<Vector2>& bottomRight, const int indexStart)
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
	vertices_out.clear();

	for (size_t meshIndex = 0; meshIndex < meshes_in.size(); ++meshIndex)
	{
		const auto& mesh = meshes_in[meshIndex];

		// Check if there's enough data
		if (mesh.indices.size() < 3) continue;

		if (mesh.primitiveTopology == PrimitiveTopology::TriangleList)
		{
			for (size_t vertexIndex = 0; vertexIndex < mesh.indices.size(); ++vertexIndex)
			{
				int index = static_cast<int>(mesh.indices[vertexIndex]);
				if (index >= 0 && index < mesh.vertices.size())
				{
					vertices_out.push_back(mesh.vertices[index]);
				}
			}
		}
		else if (mesh.primitiveTopology == PrimitiveTopology::TriangleStrip)
		{
			for (size_t vertexIndex = 0; vertexIndex < mesh.indices.size() - 2; ++vertexIndex)
			{
				if (vertexIndex % 2 == 1)
				{
					vertices_out.push_back(mesh.vertices[mesh.indices[vertexIndex]]);
					vertices_out.push_back(mesh.vertices[mesh.indices[vertexIndex + 2]]);
					vertices_out.push_back(mesh.vertices[mesh.indices[vertexIndex + 1]]);
				}
				else
				{
					for (size_t index = 0; index < 3; ++index)
					{
						int i = static_cast<int>(mesh.indices[vertexIndex + index]);
						if (i >= 0 && i < mesh.vertices.size())
						{
							vertices_out.push_back(mesh.vertices[i]);
						}
					}
				}
			}
		}
		else
		{
			for (size_t vertexIndex = 0; vertexIndex < mesh.indices.size() - 2; ++vertexIndex)
			{
				vertices_out.push_back(mesh.vertices[vertexIndex]);
			}
		}
	}
}


bool dae::Renderer::FrustrumCulling(const std::vector<Vertex_Out>& vertices_in, const int indexStart)
{
	for (int i = 0; i < 3; ++i) {
		const auto& vertex = vertices_in[i + indexStart];

		// Check if the vertex is outside the clip
		if ((vertex.position.x >= 0.0f && vertex.position.x <= float(m_Width) &&
			vertex.position.y >= 0.0f && vertex.position.y <= float(m_Height) &&
			vertex.position.z >= 0.0f && vertex.position.z <= 1.0f) == false) {
			return true;
		}
	}

	return false;
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
