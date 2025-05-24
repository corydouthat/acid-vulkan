// Acid Game Engine - Vulkan (Ver 1.3-1.4)
// Configuration

// TODO: make most of these things configurable?

// Debug
constexpr bool use_validation_layers = true;

// Number of swapchain images
constexpr unsigned int FRAME_BUFFER_COUNT = 2;

// Swapchain mode
constexpr VkPresentModeKHR SWAPCHAIN_MODE =
	//VK_PRESENT_MODE_IMMEDIATE_KHR
	//VK_PRESENT_MODE_MAILBOX_KHR
	VK_PRESENT_MODE_FIFO_KHR;			// Vsync
	//VK_PRESENT_MODE_FIFO_RELAXED_KHR

