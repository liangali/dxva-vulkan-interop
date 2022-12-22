#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include <iostream>
#include <vector>

#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>

#include <aclapi.h>
#include <VersionHelpers.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

const bool isKMT = 0;
VkExternalMemoryHandleTypeFlagBits handleTypeBits = isKMT ? VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_KMT_BIT : VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
//VkExternalMemoryHandleTypeFlagBits handleTypeBits = isKMT? VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_KMT_BIT : VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
VkExternalMemoryHandleTypeFlags handleTypeFlags = handleTypeBits;

VkResult vkRes;
VkInstance instance;
VkPhysicalDevice physicalDevice;
uint32_t queueFamilyIndex;
VkDevice device;
VkQueue queue;

VkPipeline pipeline;
VkPipelineLayout pipelineLayout;
VkShaderModule computeShaderModule;
VkCommandPool commandPool;
VkCommandBuffer commandBuffer;

ID3D11Device *pD3D11Device = nullptr;
ID3D11DeviceContext *pDeviceContext = nullptr;
ID3D11Device1 *pD3D11Device1 = nullptr;

#define CHECK_FAIL_EXIT(hr, msg) \
    if (!SUCCEEDED(hr)) { printf("ERROR: Failed to call %s\n exit", msg); exit(-1); }

#define VK_CHECK_RESULT(f)                                                             \
{                                                                                      \
    VkResult res = (f);                                                                \
    if (res != VK_SUCCESS)                                                             \
    {                                                                                  \
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);                                                     \
    }                                                                                  \
}

uint32_t getComputeQueueFamilyIndex() {
    uint32_t queueFamilyCount;

    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);

    // Retrieve all queue families.
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Now find a family that supports compute.
    uint32_t i = 0;
    for (; i < queueFamilies.size(); ++i) {
        VkQueueFamilyProperties props = queueFamilies[i];

        if (props.queueCount > 0 && (props.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            // found a queue with compute. We're done!
            break;
        }
    }

    if (i == queueFamilies.size()) {
        throw std::runtime_error("could not find a queue family that supports operations");
    }

    return i;
}

uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
            return i;
    }
    return -1;
}

void initD3D11New()
{
    HRESULT hr = S_OK;
    D3D_FEATURE_LEVEL fl{};
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
    D3D_FEATURE_LEVEL maxSupportedFeatureLevel{};
    UINT creationFlags = D3D11_CREATE_DEVICE_DEBUG;

    IDXGIFactory1* pFactory = nullptr;
    IDXGIAdapter1* pRecommendedAdapter = nullptr;
    std::vector<IDXGIAdapter1*> adapterList;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)(&pFactory));
    if (SUCCEEDED(hr)) {
        IDXGIAdapter1* pAdapter;
        UINT index = 0;
        while (pFactory->EnumAdapters1(index, &pAdapter) != DXGI_ERROR_NOT_FOUND) {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);
            if (desc.VendorId == 0x00008086) {
                pRecommendedAdapter = pAdapter;
                //pRecommendedAdapter->AddRef();
                printf("Found adapter %ws, GPU Mem: %zu MiB, Sys Mem: %zu MiB, Shared Mem: %zu MiB\n",
                    desc.Description,
                    desc.DedicatedVideoMemory / (1024 * 1024),
                    desc.DedicatedSystemMemory / (1024 * 1024),
                    desc.SharedSystemMemory / (1024 * 1024));
                //break;
                adapterList.push_back(pRecommendedAdapter);
            }
            //pAdapter->Release();
            index++;
        }
        pFactory->Release();
    }

    //hr = D3D11CreateDevice(
    //    pRecommendedAdapter,
    //    (pRecommendedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE),
    //    nullptr,
    //    creationFlags,
    //    nullptr,
    //    0,
    //    D3D11_SDK_VERSION,
    //    nullptr,
    //    &maxSupportedFeatureLevel,
    //    nullptr);
    //CHECK_FAIL_EXIT(hr, "D3D11CreateDevice");

    hr = D3D11CreateDevice(
        adapterList[0], //pRecommendedAdapter,
        D3D_DRIVER_TYPE_UNKNOWN, // (pRecommendedAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE),
        nullptr,
        creationFlags,
        levels, //(maxSupportedFeatureLevel <= D3D_FEATURE_LEVEL_11_0 ? nullptr : levels),
        1, //(maxSupportedFeatureLevel <= D3D_FEATURE_LEVEL_11_0 ? 0 : 7),
        D3D11_SDK_VERSION,
        &pD3D11Device, &fl, &pDeviceContext);
    CHECK_FAIL_EXIT(hr, "D3D11CreateDevice");

    hr = pD3D11Device->QueryInterface(__uuidof (ID3D11Device1), (void **)&pD3D11Device1);
    CHECK_FAIL_EXIT(hr, "D3D11CreateDevice");
}

void initD3D11()
{
    HRESULT hr = S_OK;
    D3D_FEATURE_LEVEL fl{};
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
    UINT creationFlags = D3D11_CREATE_DEVICE_DEBUG;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags, levels, 1, D3D11_SDK_VERSION, &pD3D11Device, &fl, &pDeviceContext);
    CHECK_FAIL_EXIT(hr, "D3D11CreateDevice");

    hr = pD3D11Device->QueryInterface(__uuidof (ID3D11Device1), (void **)&pD3D11Device1);
    CHECK_FAIL_EXIT(hr, "D3D11CreateDevice");
}

//HANDLE getSharedHandleFromD3D11Texture()
//{
//    uint32_t width = 512, height = 512;
//    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
//    D3d11Dxva* dxvactx = new D3d11Dxva(width, height, format);
//
//    dxvactx->init();
//    dxvactx->execute();
//
//    ID3D11Texture2D* d3d11Surf = dxvactx->getTexture2D();
//    IDXGIResource1* dxgiRes = dxvactx->getDXGIResource();
//    HANDLE sharedHandle = dxvactx->getSharedHandle();
//
//    printf("INFO: d3d11Surf = %p, dxgiRes = %p, sharedHandle = %p, width = %d, height = %d, format = 0x%x\n",
//        d3d11Surf, dxgiRes, sharedHandle, width, height, format);
//
//    return sharedHandle;
//
//    //dxvactx->destory();
//    //delete dxvactx;
//}

void createSharedImage(uint32_t width, uint32_t height, VkFormat format, VkImage& vkImg, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;// | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkExternalMemoryImageCreateInfo vkExternalMemImageCreateInfo = {};
    vkExternalMemImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    vkExternalMemImageCreateInfo.pNext = NULL;
    vkExternalMemImageCreateInfo.handleTypes = handleTypeFlags;
    imageInfo.pNext = &vkExternalMemImageCreateInfo;

    if (vkCreateImage(device, &imageInfo, nullptr, &vkImg) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, vkImg, &memRequirements);

    //WindowsSecurityAttributes winSecurityAttributes;
    VkExportMemoryWin32HandleInfoKHR vulkanExportMemoryWin32HandleInfoKHR = {};
    vulkanExportMemoryWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    vulkanExportMemoryWin32HandleInfoKHR.pNext = NULL;
    vulkanExportMemoryWin32HandleInfoKHR.pAttributes = nullptr;
    vulkanExportMemoryWin32HandleInfoKHR.dwAccess = DXGI_SHARED_RESOURCE_READ;// | DXGI_SHARED_RESOURCE_WRITE;
    vulkanExportMemoryWin32HandleInfoKHR.name = (LPCWSTR)NULL;

    VkExportMemoryAllocateInfoKHR vulkanExportMemoryAllocateInfoKHR = {};
    vulkanExportMemoryAllocateInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    vulkanExportMemoryAllocateInfoKHR.pNext = &vulkanExportMemoryWin32HandleInfoKHR;
    vulkanExportMemoryAllocateInfoKHR.handleTypes = handleTypeFlags;

    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.pNext = &vulkanExportMemoryAllocateInfoKHR;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, vkImg, imageMemory, 0);
}

void createSharedBuffer(size_t bufSize, VkBuffer& vkBuf, VkDeviceMemory& bufferMem)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufSize;
    bufferInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkExternalMemoryImageCreateInfo vkExternalMemBufCreateInfo = {};
    vkExternalMemBufCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    vkExternalMemBufCreateInfo.pNext = NULL;
    vkExternalMemBufCreateInfo.handleTypes = handleTypeFlags;
    bufferInfo.pNext = &vkExternalMemBufCreateInfo;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vkBuf) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vkBuf, &memRequirements);

    VkExportMemoryWin32HandleInfoKHR vulkanExportMemoryWin32HandleInfoKHR = {};
    vulkanExportMemoryWin32HandleInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    vulkanExportMemoryWin32HandleInfoKHR.pNext = NULL;
    vulkanExportMemoryWin32HandleInfoKHR.pAttributes = nullptr;
    vulkanExportMemoryWin32HandleInfoKHR.dwAccess = DXGI_SHARED_RESOURCE_READ;// | DXGI_SHARED_RESOURCE_WRITE;
    vulkanExportMemoryWin32HandleInfoKHR.name = (LPCWSTR)NULL;

    VkExportMemoryAllocateInfoKHR vulkanExportMemoryAllocateInfoKHR = {};
    vulkanExportMemoryAllocateInfoKHR.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR;
    vulkanExportMemoryAllocateInfoKHR.pNext = &vulkanExportMemoryWin32HandleInfoKHR;
    vulkanExportMemoryAllocateInfoKHR.handleTypes = handleTypeFlags;

    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.pNext = &vulkanExportMemoryAllocateInfoKHR;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMem) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, vkBuf, bufferMem, 0);
}

void shareVkImage2Dd311()
{
    HRESULT hr = S_OK;
    uint32_t width = 1024;
    uint32_t height = 1024;
    VkFormat format = VK_FORMAT_R8G8B8A8_UINT;
    uint32_t size = width * height * 4;
    VkImage vkImg;
    VkDeviceMemory imageMem;
    createSharedImage(width, height, format, vkImg, imageMem);

    size_t bufSize = 1024 * 1024;
    VkBuffer vkBuf;
    VkDeviceMemory bufMem;
    createSharedBuffer(bufSize, vkBuf, bufMem);

    bool sharedBuffer = true;

    //fillImage(vkImg, format, width, height, size);
    //if (1)
    //{
    //    void* mappedMemory = NULL;
    //    vkRes = vkMapMemory(device, imageMem, 0, size, 0, &mappedMemory);
    //    uint8_t* pdata = (uint8_t *)mappedMemory;
    //    saveBmpImage(width, height, pdata, "copy");
    //    vkUnmapMemory(device, bufferMemory);
    //}

    //auto fpGetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR");
    auto fpGetMemoryWin32HandleKHR = (PFN_vkGetMemoryWin32HandleKHR)vkGetInstanceProcAddr(instance, "vkGetMemoryWin32HandleKHR");

    if (!fpGetMemoryWin32HandleKHR)
    {
        printf("ERROR: fpGetMemoryWin32HandleKHR is null! exit");
        exit(-1);
    }

    HANDLE sharedHandle = INVALID_HANDLE_VALUE;
    if (1)
    {
        VkMemoryGetWin32HandleInfoKHR getWin32HandleInfo{};
        getWin32HandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
        getWin32HandleInfo.pNext = nullptr;
        getWin32HandleInfo.memory = (sharedBuffer) ? bufMem : imageMem;
        getWin32HandleInfo.handleType = handleTypeBits;
        VK_CHECK_RESULT(fpGetMemoryWin32HandleKHR(device, &getWin32HandleInfo, &sharedHandle));
    }
    //else
    //{
    //    sharedHandle = getSharedHandleFromD3D11Texture();
    //}

    //bool ret = 0;
    //DWORD handleFlags = 0;
    ////ret = CloseHandle(sharedHandle);
    //ret = GetHandleInformation(sharedHandle, &handleFlags);

    //VkExportMemoryWin32HandleInfoKHR exportWin32HandleInfo{};
    //exportWin32HandleInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
    //exportWin32HandleInfo.pNext = nullptr;
    //exportWin32HandleInfo.pAttributes;
    //exportWin32HandleInfo.dwAccess;
    //exportWin32HandleInfo.name;

    ID3D11Resource* d3d11TextureFromVkImage = nullptr;
    //if (isKMT)
    //{
    //    hr = pD3D11Device1->OpenSharedResource1(sharedHandle, __uuidof(ID3D11Resource), (void**)&d3d11TextureFromVkImage);
    //} 
    //else
    //{
    //    hr = pD3D11Device->OpenSharedResource(sharedHandle, __uuidof(ID3D11Resource), (void**)&d3d11TextureFromVkImage);
    //}

    hr = pD3D11Device->OpenSharedResource(sharedHandle, __uuidof(ID3D11Resource), (void**)&d3d11TextureFromVkImage);
    if (!SUCCEEDED(hr))
    {
        hr = pD3D11Device1->OpenSharedResource1(sharedHandle, __uuidof(ID3D11Resource), (void**)&d3d11TextureFromVkImage);
        CHECK_FAIL_EXIT(hr, "OpenSharedResource1");
    }

    printf("INFO: d3d11TextureFromVkImage = %p\n", d3d11TextureFromVkImage);

    IDXGIKeyedMutex* pKeyedMutex;
    hr = d3d11TextureFromVkImage->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&pKeyedMutex);
    CHECK_FAIL_EXIT(hr, "QueryInterface IDXGIKeyedMutex");

    UINT  acqKey = 1;
    UINT  relKey = 0;
    DWORD result = pKeyedMutex->AcquireSync(acqKey, INFINITE);
    CHECK_FAIL_EXIT(hr, "AcquireSync");

    ID3D11Texture2D* pd3d11SharedTexture = nullptr;
    hr = d3d11TextureFromVkImage->QueryInterface(__uuidof(ID3D11Texture2D), (void**)(&pd3d11SharedTexture));
    CHECK_FAIL_EXIT(hr, "QueryInterface ID3D11Texture2D");

    return;
}

int main()
{
    //initD3D11();

    initD3D11New();

    // create instance
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "vkd3d11";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);;
    applicationInfo.pEngineName = "No Engine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    applicationInfo.apiVersion = VK_API_VERSION_1_1; // VK_API_VERSION_1_0;

    std::vector<const char*> extensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    VK_CHECK_RESULT(vkCreateInstance(&createInfo, NULL, &instance));

    uint32_t deviceCount;
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    if (deviceCount == 0) {
        throw std::runtime_error("could not find a device with vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    for (VkPhysicalDevice device : devices) {
        physicalDevice = device;
        break;
    }

    float queuePriorities = 1.0;
    queueFamilyIndex = getComputeQueueFamilyIndex(); // find queue family with compute capability.
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1; // create one queue in this family. We don't need more.
    queueCreateInfo.pQueuePriorities = &queuePriorities;

    VkDeviceCreateInfo deviceCreateInfo = {};
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo; // when creating the logical device, we also specify what queues it has.
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
        VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
        //VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        //VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
        VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
    };
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)); // create logical device.

    // Get a handle to the only member of the queue family.
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

    shareVkImage2Dd311();

    printf("done!\n");
    return 0;
}
