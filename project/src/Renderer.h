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
		bool TriangleHitTest(const std::vector<Vertex_Out>& vertices_in, std::vector<float>& weights, std::vector<float>& area, const Vector2 P,const int indexStart = 0);
		void BoundingBox(const std::vector<Vertex_Out>& vertices_in, std::vector<Vector2>& topLeft, std::vector<Vector2>& bottomRight, const int indexStart = 0);
		void IndexBuffer(std::vector<Vertex>& vertices_out, const std::vector<Mesh>& meshes_in);
		bool FrustrumCulling(const std::vector<Vertex_Out>& vertices_in, const int indexStart = 0);

		void ToggleDepthBuffer() { m_DepthColor = !m_DepthColor; };

	private:
		SDL_Window* m_pWindow{};

		SDL_Surface* m_pFrontBuffer{ nullptr };
		SDL_Surface* m_pBackBuffer{ nullptr };
		uint32_t* m_pBackBufferPixels{};

		float* m_pDepthBufferPixels{};

		Camera m_Camera{};

		//Texture;
		Texture* m_ptrTexture = Texture::LoadFromFile("resources/tuktuk.png");

		Matrix m_WorldMatrix{};

		int m_Width{};
		int m_Height{};
		bool m_DepthColor{ false };
	};
}
