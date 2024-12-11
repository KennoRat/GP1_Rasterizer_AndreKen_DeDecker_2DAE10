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
#include <execution>

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
	m_Camera.Initialize(45.f, { 0.0f, 0.0f, 0.0f }, m_Width, m_Height);

	//Parse file
	Mesh parseMesh{};
	if (!Utils::ParseOBJ("resources/vehicle.obj", parseMesh.vertices, parseMesh.indices)) {
		std::cerr << "Failed to load OBJ file!" << std::endl;
	}
	m_Meshes.push_back(parseMesh);
	m_Meshes[0].worldMatrix = m_WorldMatrix;
	m_Meshes[0].primitiveTopology = PrimitiveTopology::TriangleList;

	//Reserve space
	m_VerticesWorld.reserve(m_Meshes[0].vertices.size());
	m_VerticesSP.reserve(m_VerticesWorld.size());

	m_AmountOfTriangles = m_Meshes[0].vertices.size() / 3;
}

Renderer::~Renderer()
{
	delete[] m_pDepthBufferPixels;
	delete m_ptrDiffuseTexture;
	delete m_ptrSpecularTexture;
	delete m_ptrNormalTexture;
	delete m_ptrGlossinessTexture;
}

void Renderer::Update(Timer* pTimer)
{
	m_Camera.Update(pTimer);

	//Spin Mesh
	if(m_IsRotating)
	{
		Matrix Pos = Matrix::CreateTranslation(0.0f, 0.0f, 50.f);
		m_YawAngle += (pTimer->GetElapsed() * M_PI * 0.5f);
		Matrix Ros = Matrix::CreateRotationY(m_YawAngle);
		m_WorldMatrix = Ros * Pos;
	}

}

void Renderer::Render()
{
	//@START
	//Lock BackBuffer
	SDL_LockSurface(m_pBackBuffer);

	//Clear Backbuffer
	Uint32 clearColor = SDL_MapRGB(m_pBackBuffer->format, 80, 80, 80);
	SDL_FillRect(m_pBackBuffer, NULL, clearColor);

	//Depth Buffer
	for (int index{}; index < (m_Width * m_Height); ++index) m_pDepthBufferPixels[index] = std::numeric_limits<float>::max();

	//Index Buffer List or Strip
	m_VerticesWorld.clear();
	IndexBuffer(m_VerticesWorld, m_Meshes);

	//Vertex transformation to screenspace
	m_VerticesSP.clear();
	VertexTransformationFunction(m_VerticesWorld, m_VerticesSP);

	ColorRGB finalColor{};

	//Bounding Boxes
	std::vector<Vector2> topLeft(m_AmountOfTriangles, Vector2{ float(m_Width), float(m_Height) });
	std::vector<Vector2> bottomRight(m_AmountOfTriangles);

	//RENDER LOGIC	
	std::vector<uint32_t> triangleIndices{};
	triangleIndices.reserve(m_AmountOfTriangles);
	for (uint32_t index{}; index < m_AmountOfTriangles; ++index) triangleIndices.emplace_back(index);

	std::for_each(std::execution::seq, triangleIndices.begin(), triangleIndices.end(), [&](int triangleIdx)
	{
			const int tIdx{ triangleIdx * 3 };

			// Frustum Culling
			if (FrustrumCulling(m_VerticesSP, tIdx)) return;

			// Bounding Box Calculation
			BoundingBox(m_VerticesSP, topLeft, bottomRight, triangleIdx);

			for (int px = int(topLeft[triangleIdx].x); px < int(bottomRight[triangleIdx].x); ++px)
			{
				for (int py = int(topLeft[triangleIdx].y); py < int(bottomRight[triangleIdx].y); ++py)
				{
					RenderPixel(finalColor, px, py, triangleIdx);
				}
			}
	});

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
	const float halfWidth = float(m_Width) / 2.0f;
	const float halfHeight = float(m_Height) / 2.0f;

	for (int vertexIndex = 0; vertexIndex < vertices_in.size(); ++vertexIndex)
	{
		//Transform the vertex
		Vector4 transformed = WVPMatrix.TransformPoint(
			vertices_in[vertexIndex].position.x,
			vertices_in[vertexIndex].position.y,
			vertices_in[vertexIndex].position.z,
			0);

		//Perspective divide by w (not z)
		transformed.x /= transformed.w;
		transformed.y /= transformed.w;
		transformed.z /= transformed.w;

		//Store w for later depth calculations and transform to ScreenSpace Vertices
		Vertex_Out sp_Vertex{ Vector4 {	(transformed.x + 1) * halfWidth,
								(1 - transformed.y) * halfHeight,
								transformed.z, 
								transformed.w },
								vertices_in[vertexIndex].color,
								vertices_in[vertexIndex].uv,
								Vector3{ m_WorldMatrix.TransformVector(vertices_in[vertexIndex].normal) },
								Vector3{ m_WorldMatrix.TransformVector(vertices_in[vertexIndex].tangent) },
								(vertices_in[vertexIndex].position - m_Camera.origin).Normalized()};

		vertices_out.push_back(sp_Vertex);
	}
}

bool dae::Renderer::TriangleHitTest(const std::vector<Vertex_Out>& vertices_in, std::vector<float>& weights, float& area, const Vector2 P, const int indexStart)
{
	const int tIdx{ indexStart * 3 };

	for (int edgeIdx{}; edgeIdx < 3; ++edgeIdx)
	{
		Vector3 e{};
		int nextIdx{ (edgeIdx + 1) % 3 };

		e = vertices_in[nextIdx + tIdx].position
			- vertices_in[edgeIdx + tIdx].position;

		Vector2 pV{ P.x - vertices_in[edgeIdx + tIdx].position.x,
			P.y - vertices_in[edgeIdx + tIdx].position.y };

		float result{ Vector2::Cross(Vector2{ e.x, e.y }, pV) };
		if (result < 0)
		{
			return false;
		}
		else
		{
			if (edgeIdx == 1) weights[0] = result;
			else if (edgeIdx == 2) weights[1] = result;
			else weights[2] = result;
			area += result;
		}
	}
	return true;
}

void dae::Renderer::BoundingBox(const std::vector<Vertex_Out>& vertices_in, std::vector<Vector2>& topLeft, std::vector<Vector2>& bottomRight, const int indexStart)
{
	const int tIdx{ indexStart * 3 };

	for (int edgeIdx{}; edgeIdx < 3; ++edgeIdx)
	{
		topLeft[indexStart].x = std::clamp(std::min(topLeft[indexStart].x, vertices_in[tIdx + edgeIdx].position.x), 0.0f, float(m_Width));
		topLeft[indexStart].y = std::clamp(std::min(topLeft[indexStart].y, vertices_in[tIdx + edgeIdx].position.y), 0.0f, float(m_Height));
		bottomRight[indexStart].x = std::clamp(std::max(bottomRight[indexStart].x, vertices_in[tIdx + edgeIdx].position.x), 0.0f, float(m_Width));
		bottomRight[indexStart].y = std::clamp(std::max(bottomRight[indexStart].y, vertices_in[tIdx + edgeIdx].position.y), 0.0f, float(m_Height));
	}
}

void dae::Renderer::IndexBuffer(std::vector<Vertex>& vertices_out, const std::vector<Mesh>& meshes_in)
{
	vertices_out.clear();

	for (size_t meshIndex = 0; meshIndex < meshes_in.size(); ++meshIndex)
	{
		const auto& mesh = meshes_in[meshIndex];

		//Check if there's enough data
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

		//Check if the vertex is outside the clip
		if ((vertex.position.x >= 0.0f && vertex.position.x <= m_Width &&
			vertex.position.y >= 0.0f && vertex.position.y <= m_Height &&
			vertex.position.z >= 0.0f && vertex.position.z <= 1.0f) == false) {
			return true;
		}
	}

	return false;
}

Vertex_Out dae::Renderer::InterpolateVertices(const std::vector<float>& weights, const int indexStart)
{
	const int tIdx{ indexStart * 3 };
	Vertex_Out vertex{};

	//Interpolate attributes
	Vector2 UV_Numerator{};
	ColorRGB Color_Numerator{};
	Vector3 Normal_Numerator, Tangent_Numerator, Position_Numerator, ViewDirection_Numerator{};
	float Denominator{};

	for (int weightIdx = 0; weightIdx < 3; ++weightIdx) {
		float invW = 1.f / m_VerticesSP[weightIdx + tIdx].position.w;

		UV_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].uv * invW);

		Color_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].color * invW);

		Normal_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].normal * invW);

		Tangent_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].tangent * invW);

		Position_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].position * invW);

		ViewDirection_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].viewDirection * invW);

		Denominator += weights[weightIdx] * invW;
	}

	//Final
	vertex.uv = UV_Numerator / Denominator;
	vertex.uv.x = std::clamp(vertex.uv.x, 0.0f, 1.0f);
	vertex.uv.y = std::clamp(vertex.uv.y, 0.0f, 1.0f);

	vertex.color = Color_Numerator / Denominator;

	vertex.normal = Normal_Numerator / Denominator;
	vertex.normal.Normalize();

	vertex.tangent = Tangent_Numerator / Denominator;
	vertex.tangent.Normalize();

	Vector3 tempPosition{ Position_Numerator / Denominator };
	vertex.position = Vector4{ tempPosition.x, tempPosition.y, tempPosition.z, 0.f};

	vertex.viewDirection = ViewDirection_Numerator / Denominator;
	vertex.viewDirection.Normalize();
	
	return vertex;
}

ColorRGB dae::Renderer::PixelShading(const Vertex_Out& v)
{
	//Ambient
	ColorRGB Ambient{0.025f, 0.025f, 0.025f };

	//Light
	Vector3 lightDirection{0.577f, -0.577f, 0.577f};

	//Normal
	Vector3 nVector{v.normal};

	if(m_UseNormalMap)
	{
		//Sample normal
		ColorRGB normalColor{ m_ptrNormalTexture->Sample(v.uv) };
		nVector = Vector3{ normalColor.r, normalColor.g, normalColor.b };

		//Remap from [0,255] to [0,1]
		nVector = (2.f * nVector) - Vector3{ 1.f, 1.f, 1.f };

		//Transform to world space
		Vector3 binormal{ Vector3::Cross(v.normal, v.tangent) };
		Matrix tangentSpaceAxis{ v.tangent, binormal, v.normal, Vector3::Zero };
		nVector = tangentSpaceAxis.TransformVector(nVector);
	}


	//Lambert
	float lambertsLaw{ Vector3::Dot(nVector, -lightDirection) };

	if(lambertsLaw > 0)
	{
		//Sample Diffuse
		float lightIntensity{ 7.f };
		ColorRGB Diffuse{ (lightIntensity * m_ptrDiffuseTexture->Sample(v.uv)) / M_PI };

		//Phong
		float shininess{ 25.f };
		Vector3 reflect{ lightDirection - (2 * (Vector3::Dot(nVector,lightDirection)) * nVector) };
		float cosAngle{ std::max(0.f, Vector3::Dot(reflect, v.viewDirection)) };
		ColorRGB Specular{ m_ptrSpecularTexture->Sample(v.uv) * powf(cosAngle, m_ptrGlossinessTexture->Sample(v.uv).r * shininess) };

		switch (m_ShadingMode)
		{
		case dae::ShadingMode::ObservedArea:
			return ColorRGB{ lambertsLaw, lambertsLaw , lambertsLaw };
			break;
		case dae::ShadingMode::Diffuse:
			return Diffuse;
			break;
		case dae::ShadingMode::Specular:
			return Specular;
			break;
		case dae::ShadingMode::Combined:
			return Diffuse * lambertsLaw + Specular + Ambient;
			break;
		}
	}
	
	return { Ambient };
}

void dae::Renderer::CycleShadingMode()
{
	switch (m_ShadingMode)
	{
	case ShadingMode::ObservedArea:
		m_ShadingMode = ShadingMode::Diffuse;
		break;
	case ShadingMode::Diffuse:
		m_ShadingMode = ShadingMode::Specular;
		break;
	case ShadingMode::Specular:
		m_ShadingMode = ShadingMode::Combined;
		break;
	case ShadingMode::Combined:
		m_ShadingMode = ShadingMode::ObservedArea;
		break;
	}
}

void dae::Renderer::RenderPixel(ColorRGB& finalColor, int px, int py, int triangleIdx)
{
	bool changedColor{ false };
	const int tIdx{ triangleIdx * 3 };

	//Pixel
	Vector2 P{ px + 0.5f, py + 0.5f };

	//Area Of Parallelogram and Weights
	float a_parallelogram{};
	std::vector<float> weights(3);

	//Triangle Hit Test and Barycentric Coordinates
	if (TriangleHitTest(m_VerticesSP, weights, a_parallelogram, P, triangleIdx))
	{
		//depth interpolation
		float z_Numerator = 0.f;
		float z_Denominator = 0.f;

		for (int weightIdx = 0; weightIdx < 3; ++weightIdx) {
			float invW = 1.f / m_VerticesSP[weightIdx + tIdx].position.w;

			//Depth (z) Interpolation
			z_Numerator += weights[weightIdx] * (m_VerticesSP[weightIdx + tIdx].position.z * invW);
			z_Denominator += weights[weightIdx] * invW;
		}

		//Final perspective-correct depth (z)
		float z_interpolated = z_Numerator / z_Denominator;

		const int bufferIndex = px + py * m_Width;
		float& depthBufferValue = m_pDepthBufferPixels[bufferIndex];

		//Depth buffer comparison
		if ((z_interpolated >= 0 && z_interpolated <= 1) &&
			(depthBufferValue > z_interpolated))
		{
			depthBufferValue = z_interpolated;
			changedColor = true;

			//Interpolated Vertex
			Vertex_Out IntVer{ InterpolateVertices(weights, triangleIdx) };

			if (m_DepthColor) {
				finalColor.r = Utils::Remap(z_interpolated, 0.995f, 1.f);
				finalColor.g = finalColor.r;
				finalColor.b = finalColor.r;
			}
			else {
				//finalColor = m_ptrTexture->Sample(UV);
				finalColor = PixelShading(IntVer);
			}
		}

	}

	//Update Color in Buffer
	if (changedColor)
	{
		finalColor.MaxToOne();

		m_pBackBufferPixels[px + (py * m_Width)] = SDL_MapRGB(m_pBackBuffer->format,
			static_cast<uint8_t>(finalColor.r * 255),
			static_cast<uint8_t>(finalColor.g * 255),
			static_cast<uint8_t>(finalColor.b * 255));
	}
}

bool Renderer::SaveBufferToImage() const
{
	return SDL_SaveBMP(m_pBackBuffer, "Rasterizer_ColorBuffer.bmp");
}
