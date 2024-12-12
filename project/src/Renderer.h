#pragma once

#include <cstdint>
#include <vector>

#include "Camera.h"
#include "Texture.h"

struct SDL_Window;
struct SDL_Surface;

namespace dae
{
	class Texture;
	struct Mesh;
	struct Vertex;
	struct Vertex_Out;
	class Timer;
	class Scene;

	enum class ShadingMode
	{
		ObservedArea, Diffuse, Specular, Combined
	};

	class Renderer final
	{
	public:
		Renderer(SDL_Window* pWindow);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer(Renderer&&) noexcept = delete;
		Renderer& operator=(const Renderer&) = delete;
		Renderer& operator=(Renderer&&) noexcept = delete;

		void Update(Timer* pTimer);
		void Render();

		bool SaveBufferToImage() const;

		void VertexTransformationFunction(const std::vector<Vertex>& vertices_in, std::vector<Vertex_Out>& vertices_out) const;
		bool TriangleHitTest(const std::vector<Vertex_Out>& vertices_in, std::vector<float>& weights,float& area, const Vector2 P,const int indexStart = 0);
		void BoundingBox(const std::vector<Vertex_Out>& vertices_in, Vector2& topLeft, Vector2& bottomRight, const int indexStart = 0);
		void IndexBuffer(std::vector<Vertex>& vertices_out, const std::vector<Mesh>& meshes_in);
		inline bool FrustrumCulling(const std::vector<Vertex_Out>& vertices_in, const int indexStart = 0);
		Vertex_Out InterpolateVertices(const std::vector<float>& weights, const int indexStart = 0);
		ColorRGB PixelShading(const Vertex_Out& v);
		void RenderPixel(ColorRGB& finalColor, int px, int py, int tiangleIdx = 0 );


		//Keybinds
		void ToggleDepthBuffer() { m_DepthColor = !m_DepthColor; };
		void ToggleRotating() { m_IsRotating = !m_IsRotating; };
		void ToggleNormalMap() { m_UseNormalMap = !m_UseNormalMap; };
		void CycleShadingMode();

	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		//Texture;
		Texture* m_ptrDiffuseTexture = Texture::LoadFromFile("resources/vehicle_diffuse.png");
		Texture* m_ptrSpecularTexture = Texture::LoadFromFile("resources/vehicle_specular.png");
		Texture* m_ptrNormalTexture = Texture::LoadFromFile("resources/vehicle_normal.png");
		Texture* m_ptrGlossinessTexture = Texture::LoadFromFile("resources/vehicle_gloss.png");

		Matrix m_WorldMatrix{};
		std::vector<Mesh>		m_Meshes;
		std::vector<Vertex>		m_VerticesWorld;
		std::vector<Vertex_Out> m_VerticesSP;

		int m_Width{};
		int m_Height{};
		int m_AmountOfTriangles{};
		float m_YawAngle{};
		float m_HalfWidth{};
		float m_HalfHeight{};

		//Keybinds
		bool m_DepthColor{ false };
		bool m_IsRotating{ true };
		bool m_UseNormalMap{ true };
		bool m_UseParallelThreading{ true };
		ShadingMode m_ShadingMode{ ShadingMode::Combined };
	};
}
