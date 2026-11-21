#ifndef MDKVIEW_H
#define MDKVIEW_H

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>

extern HINSTANCE g_hInst;

class CMDKScene;

class CMDKView
{
public:
    CMDKView();
    virtual ~CMDKView();

	bool Init(int nWidth, int nHeight);
	void Draw();
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	bool InitVulkan();
	void TermVulkan();
	bool CreateSwapchain();
	void DestroySwapchain();
	void RecreateSwapchain();
	uint32_t FindMemoryType(uint32_t nTypeBits, VkMemoryPropertyFlags properties);

	// マウスによるビュー操作 (Tutorial10 の controls.cpp から移植)
	void InitializeVP();
	void OnLButtonDown(int pixelX, int pixelY);
	void OnMouseMove(int pixelX, int pixelY, DWORD flags);
	void OnLButtonUp(int pixelX, int pixelY);
	void OnMouseWheel(int nDelta);
	void computeMatricesFromInputs(int pixelX, int pixelY);
	glm::mat4 getViewMatrix();
	glm::mat4 getProjectionMatrix();

	HWND m_hWnd;
	VkInstance m_instance;
	VkSurfaceKHR m_surface;
	VkPhysicalDevice m_physicalDevice;
	VkDevice m_device;
	uint32_t m_nQueueFamily;
	VkQueue m_queue;
	VkSurfaceFormatKHR m_surfaceFormat;
	VkFormat m_depthFormat;
	VkRenderPass m_renderPass;
	VkCommandPool m_commandPool;
	VkCommandBuffer m_commandBuffer;
	VkSemaphore m_imageAvailable;
	VkFence m_inFlight;

	// ウィンドウサイズが変わったら作り直すもの
	VkSwapchainKHR m_swapchain;
	VkExtent2D m_extent;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	std::vector<VkFramebuffer> m_framebuffers;
	std::vector<VkSemaphore> m_renderFinished;
	VkImage m_depthImage;
	VkDeviceMemory m_depthMemory;
	VkImageView m_depthImageView;

	CMDKScene* m_pScene;

	glm::mat4 m_ViewMatrix;
	glm::mat4 m_ProjectionMatrix;
	float m_fHorizontalAngle;
	float m_fVerticalAngle;
	float m_fDistance;			// 視点から注視点までの距離
	float m_fInitialFoV;		// Initial Field of View
	float m_fMouseSpeed;
	double m_dPrevX;
	double m_dPrevY;

	int m_nWidth;
	int m_nHeight;
};

#endif //MDKVIEW_H
