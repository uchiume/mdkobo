#include "pch.h"

#include "resource.h"
#include "mdkview.h"
#include "mdkscene.h"
#include <vulkan/vulkan_win32.h>
#include <algorithm>

CMDKView::CMDKView()
	: m_hWnd(NULL)
	, m_instance(VK_NULL_HANDLE)
	, m_surface(VK_NULL_HANDLE)
	, m_physicalDevice(VK_NULL_HANDLE)
	, m_device(VK_NULL_HANDLE)
	, m_nQueueFamily(0)
	, m_queue(VK_NULL_HANDLE)
	, m_surfaceFormat()
	, m_depthFormat(VK_FORMAT_D32_SFLOAT)
	, m_renderPass(VK_NULL_HANDLE)
	, m_commandPool(VK_NULL_HANDLE)
	, m_commandBuffer(VK_NULL_HANDLE)
	, m_imageAvailable(VK_NULL_HANDLE)
	, m_inFlight(VK_NULL_HANDLE)
	, m_swapchain(VK_NULL_HANDLE)
	, m_extent()
	, m_depthImage(VK_NULL_HANDLE)
	, m_depthMemory(VK_NULL_HANDLE)
	, m_depthImageView(VK_NULL_HANDLE)
	, m_pScene(NULL)
	, m_nWidth(0)
	, m_nHeight(0)
{
}

CMDKView::~CMDKView()
{
}

LRESULT CALLBACK CMDKView::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	CMDKView* thisPtr = (CMDKView*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (!thisPtr) {
		if (msg == WM_CREATE) {
			thisPtr = (CMDKView*)((LPCREATESTRUCT)lParam)->lpCreateParams;
			if (thisPtr) {
				::SetWindowLongPtr(hWnd, GWLP_USERDATA, (ULONG_PTR)thisPtr);
			}
		}
		if (!thisPtr) {
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	}

	switch (msg) {
	case WM_CREATE:
		break;
	case WM_CLOSE:
		::DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		thisPtr->TermVulkan();
		::PostQuitMessage(0);
		break;
	default:
		break;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

bool CMDKView::Init(int nWidth, int nHeight)
{
	WNDCLASSEX wndClass;
	memset(&wndClass, 0, sizeof(wndClass));
	wndClass.cbSize = sizeof(wndClass);
	wndClass.style = CS_OWNDC;
	wndClass.lpszClassName = L"MDK_VIEW_CLASS";
	wndClass.lpfnWndProc = this->WndProc;
	wndClass.hInstance = g_hInst;
	wndClass.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON1));
	wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	::RegisterClassEx(&wndClass);

	m_hWnd = ::CreateWindowEx(
		WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
		L"MDK_VIEW_CLASS",
		L"MDKOBO",
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX,
		0,
		0,
		nWidth,
		nHeight,
		nullptr,
		nullptr,
		g_hInst,
		this);

	if (m_hWnd == NULL)
	{
		return false;
	}

	m_nWidth = nWidth;	// companion window size
	m_nHeight = nHeight;

	if (!InitVulkan())
	{
		::DestroyWindow(m_hWnd);
		m_hWnd = NULL;
		return false;
	}

	::ShowWindow(m_hWnd, SW_SHOW);

	return true;
}

bool CMDKView::InitVulkan()
{
	// Instance
	const char* instanceExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "MDKOBO";
	appInfo.apiVersion = VK_API_VERSION_1_0;
	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = 2;
	instanceInfo.ppEnabledExtensionNames = instanceExtensions;
	if (vkCreateInstance(&instanceInfo, NULL, &m_instance) != VK_SUCCESS)
	{
		return false;
	}

	// Surface
	VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.hinstance = g_hInst;
	surfaceInfo.hwnd = m_hWnd;
	if (vkCreateWin32SurfaceKHR(m_instance, &surfaceInfo, NULL, &m_surface) != VK_SUCCESS)
	{
		return false;
	}

	// 描画と表示ができるキューを持つ GPU を選ぶ (ディスクリート GPU を優先)
	uint32_t nDeviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &nDeviceCount, NULL);
	std::vector<VkPhysicalDevice> devices(nDeviceCount);
	vkEnumeratePhysicalDevices(m_instance, &nDeviceCount, devices.data());
	for (VkPhysicalDevice device : devices)
	{
		uint32_t nFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &nFamilyCount, NULL);
		std::vector<VkQueueFamilyProperties> families(nFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &nFamilyCount, families.data());
		for (uint32_t i = 0; i < nFamilyCount; i++)
		{
			VkBool32 bPresent = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &bPresent);
			if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && bPresent)
			{
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(device, &properties);
				if (m_physicalDevice == VK_NULL_HANDLE || properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					m_physicalDevice = device;
					m_nQueueFamily = i;
				}
				break;
			}
		}
	}
	if (m_physicalDevice == VK_NULL_HANDLE)
	{
		return false;
	}

	// Logical device
	float fPriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = {};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = m_nQueueFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &fPriority;
	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	deviceInfo.enabledExtensionCount = 1;
	deviceInfo.ppEnabledExtensionNames = deviceExtensions;
	if (vkCreateDevice(m_physicalDevice, &deviceInfo, NULL, &m_device) != VK_SUCCESS)
	{
		return false;
	}
	vkGetDeviceQueue(m_device, m_nQueueFamily, 0, &m_queue);

	// 表示用の形式 (OpenGL のデフォルトと同じく sRGB 変換しない UNORM を優先)
	uint32_t nFormatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &nFormatCount, NULL);
	std::vector<VkSurfaceFormatKHR> formats(nFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &nFormatCount, formats.data());
	if (nFormatCount == 0)
	{
		return false;
	}
	m_surfaceFormat = formats[0];
	for (const VkSurfaceFormatKHR& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			m_surfaceFormat = format;
			break;
		}
	}

	// Render pass : 色と深度を毎回クリアする
	VkAttachmentDescription attachments[2] = {};
	attachments[0].format = m_surfaceFormat.format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachments[1].format = m_depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	if (vkCreateRenderPass(m_device, &renderPassInfo, NULL, &m_renderPass) != VK_SUCCESS)
	{
		return false;
	}

	// Command buffer
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_nQueueFamily;
	if (vkCreateCommandPool(m_device, &poolInfo, NULL, &m_commandPool) != VK_SUCCESS)
	{
		return false;
	}
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer) != VK_SUCCESS)
	{
		return false;
	}

	// Synchronization
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateSemaphore(m_device, &semaphoreInfo, NULL, &m_imageAvailable) != VK_SUCCESS
		|| vkCreateFence(m_device, &fenceInfo, NULL, &m_inFlight) != VK_SUCCESS)
	{
		return false;
	}

	if (!CreateSwapchain())
	{
		return false;
	}

	MDKVulkanContext context = { m_physicalDevice, m_device, m_queue, m_commandPool, m_renderPass };
	m_pScene = new CMDKScene();
	return m_pScene->Init(context, (int)m_extent.width, (int)m_extent.height);
}

void CMDKView::TermVulkan()
{
	if (m_device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_device);

		if (m_pScene != NULL)
		{
			m_pScene->Term();
			delete m_pScene;
			m_pScene = NULL;
		}

		DestroySwapchain();
		vkDestroyFence(m_device, m_inFlight, NULL);
		vkDestroySemaphore(m_device, m_imageAvailable, NULL);
		vkDestroyCommandPool(m_device, m_commandPool, NULL);
		vkDestroyRenderPass(m_device, m_renderPass, NULL);
		vkDestroyDevice(m_device, NULL);
		m_inFlight = VK_NULL_HANDLE;
		m_imageAvailable = VK_NULL_HANDLE;
		m_commandBuffer = VK_NULL_HANDLE;
		m_commandPool = VK_NULL_HANDLE;
		m_renderPass = VK_NULL_HANDLE;
		m_device = VK_NULL_HANDLE;
	}

	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_instance, m_surface, NULL);
		vkDestroyInstance(m_instance, NULL);
		m_surface = VK_NULL_HANDLE;
		m_instance = VK_NULL_HANDLE;
	}
}

bool CMDKView::CreateSwapchain()
{
	VkSurfaceCapabilitiesKHR caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);
	VkExtent2D extent = caps.currentExtent;
	if (extent.width == 0xFFFFFFFF)
	{
		RECT rcClient;
		::GetClientRect(m_hWnd, &rcClient);
		extent.width = (std::max)(caps.minImageExtent.width, (std::min)(caps.maxImageExtent.width, (uint32_t)(rcClient.right - rcClient.left)));
		extent.height = (std::max)(caps.minImageExtent.height, (std::min)(caps.maxImageExtent.height, (uint32_t)(rcClient.bottom - rcClient.top)));
	}
	if (extent.width == 0 || extent.height == 0)
	{
		return false;	// 最小化中は作れない
	}

	uint32_t nImageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && nImageCount > caps.maxImageCount)
	{
		nImageCount = caps.maxImageCount;
	}
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (!(caps.supportedCompositeAlpha & compositeAlpha))
	{
		compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}

	VkSwapchainCreateInfoKHR swapchainInfo = {};
	swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainInfo.surface = m_surface;
	swapchainInfo.minImageCount = nImageCount;
	swapchainInfo.imageFormat = m_surfaceFormat.format;
	swapchainInfo.imageColorSpace = m_surfaceFormat.colorSpace;
	swapchainInfo.imageExtent = extent;
	swapchainInfo.imageArrayLayers = 1;
	swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainInfo.preTransform = caps.currentTransform;
	swapchainInfo.compositeAlpha = compositeAlpha;
	swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainInfo.clipped = VK_TRUE;
	if (vkCreateSwapchainKHR(m_device, &swapchainInfo, NULL, &m_swapchain) != VK_SUCCESS)
	{
		m_swapchain = VK_NULL_HANDLE;
		return false;
	}
	m_extent = extent;

	uint32_t nCount = 0;
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &nCount, NULL);
	m_swapchainImages.resize(nCount);
	vkGetSwapchainImagesKHR(m_device, m_swapchain, &nCount, m_swapchainImages.data());

	// Depth buffer
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_depthFormat;
	imageInfo.extent = { extent.width, extent.height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (vkCreateImage(m_device, &imageInfo, NULL, &m_depthImage) != VK_SUCCESS)
	{
		return false;
	}
	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(m_device, m_depthImage, &memReq);
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (vkAllocateMemory(m_device, &allocInfo, NULL, &m_depthMemory) != VK_SUCCESS)
	{
		return false;
	}
	vkBindImageMemory(m_device, m_depthImage, m_depthMemory, 0);

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.image = m_depthImage;
	viewInfo.format = m_depthFormat;
	viewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
	if (vkCreateImageView(m_device, &viewInfo, NULL, &m_depthImageView) != VK_SUCCESS)
	{
		return false;
	}

	// スワップチェーンのイメージごとにビュー、フレームバッファ、描画完了のセマフォを作る
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	m_swapchainImageViews.resize(nCount, VK_NULL_HANDLE);
	m_framebuffers.resize(nCount, VK_NULL_HANDLE);
	m_renderFinished.resize(nCount, VK_NULL_HANDLE);
	for (uint32_t i = 0; i < nCount; i++)
	{
		viewInfo.image = m_swapchainImages[i];
		viewInfo.format = m_surfaceFormat.format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		if (vkCreateImageView(m_device, &viewInfo, NULL, &m_swapchainImageViews[i]) != VK_SUCCESS)
		{
			return false;
		}

		VkImageView views[2] = { m_swapchainImageViews[i], m_depthImageView };
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = views;
		framebufferInfo.width = extent.width;
		framebufferInfo.height = extent.height;
		framebufferInfo.layers = 1;
		if (vkCreateFramebuffer(m_device, &framebufferInfo, NULL, &m_framebuffers[i]) != VK_SUCCESS)
		{
			return false;
		}

		if (vkCreateSemaphore(m_device, &semaphoreInfo, NULL, &m_renderFinished[i]) != VK_SUCCESS)
		{
			return false;
		}
	}

	return true;
}

void CMDKView::DestroySwapchain()
{
	for (VkSemaphore semaphore : m_renderFinished)
	{
		vkDestroySemaphore(m_device, semaphore, NULL);
	}
	for (VkFramebuffer framebuffer : m_framebuffers)
	{
		vkDestroyFramebuffer(m_device, framebuffer, NULL);
	}
	for (VkImageView view : m_swapchainImageViews)
	{
		vkDestroyImageView(m_device, view, NULL);
	}
	m_renderFinished.clear();
	m_framebuffers.clear();
	m_swapchainImageViews.clear();
	m_swapchainImages.clear();

	vkDestroyImageView(m_device, m_depthImageView, NULL);
	vkDestroyImage(m_device, m_depthImage, NULL);
	vkFreeMemory(m_device, m_depthMemory, NULL);
	// 仕様では NULL でもよいが、NULL を渡すと落ちるレイヤー (Bandicam など) があるので呼ばない
	if (m_swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapchain, NULL);
	}
	m_depthImageView = VK_NULL_HANDLE;
	m_depthImage = VK_NULL_HANDLE;
	m_depthMemory = VK_NULL_HANDLE;
	m_swapchain = VK_NULL_HANDLE;
}

void CMDKView::RecreateSwapchain()
{
	vkDeviceWaitIdle(m_device);
	DestroySwapchain();
	if (!CreateSwapchain())
	{
		DestroySwapchain();	// 次の Draw でもう一度作る
	}
}

uint32_t CMDKView::FindMemoryType(uint32_t nTypeBits, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((nTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	return 0;
}

void CMDKView::Draw()
{
	if (m_device == VK_NULL_HANDLE)
	{
		return;
	}
	if (m_swapchain == VK_NULL_HANDLE)
	{
		RecreateSwapchain();
		if (m_swapchain == VK_NULL_HANDLE)
		{
			return;	// 最小化中
		}
	}

	// 前のフレームの描画が終わるのを待つ
	vkWaitForFences(m_device, 1, &m_inFlight, VK_TRUE, UINT64_MAX);

	uint32_t nImageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_imageAvailable, VK_NULL_HANDLE, &nImageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		RecreateSwapchain();
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		return;
	}
	vkResetFences(m_device, 1, &m_inFlight);

	vkResetCommandBuffer(m_commandBuffer, 0);
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

	// Clear the screen
	VkClearValue clearValues[2] = {};
	clearValues[0].color = { { 0.0f, 0.0f, 0.4f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };
	VkRenderPassBeginInfo renderPassBegin = {};
	renderPassBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBegin.renderPass = m_renderPass;
	renderPassBegin.framebuffer = m_framebuffers[nImageIndex];
	renderPassBegin.renderArea.extent = m_extent;
	renderPassBegin.clearValueCount = 2;
	renderPassBegin.pClearValues = clearValues;
	vkCmdBeginRenderPass(m_commandBuffer, &renderPassBegin, VK_SUBPASS_CONTENTS_INLINE);

	m_pScene->Draw(m_commandBuffer, m_extent);

	vkCmdEndRenderPass(m_commandBuffer);
	vkEndCommandBuffer(m_commandBuffer);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailable;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinished[nImageIndex];
	vkQueueSubmit(m_queue, 1, &submitInfo, m_inFlight);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinished[nImageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.pImageIndices = &nImageIndex;
	result = vkQueuePresentKHR(m_queue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		RecreateSwapchain();
	}
}
