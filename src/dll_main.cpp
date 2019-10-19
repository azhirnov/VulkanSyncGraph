// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "src/LayerManager.h"
#include "vulkan/vk_layer.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
		case DLL_PROCESS_ATTACH:
			break;

		case DLL_PROCESS_DETACH:
			break;
	}

	return TRUE;
}

extern "C"
{
	#define VK_LAYER_EXPORT

	VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
										vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* pVersionStruct)
	{
		ASSERT( pVersionStruct );
		ASSERT( pVersionStruct->sType == LAYER_NEGOTIATE_INTERFACE_STRUCT );

		if ( pVersionStruct->loaderLayerInterfaceVersion >= CURRENT_LOADER_LAYER_INTERFACE_VERSION )
		{
			pVersionStruct->pfnGetInstanceProcAddr       = VSA::LayerManager::vki_GetInstanceProcAddr;
			pVersionStruct->pfnGetDeviceProcAddr         = VSA::LayerManager::vki_GetDeviceProcAddr;
			pVersionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
		}

		if ( pVersionStruct->loaderLayerInterfaceVersion > CURRENT_LOADER_LAYER_INTERFACE_VERSION )
		{
			pVersionStruct->loaderLayerInterfaceVersion = CURRENT_LOADER_LAYER_INTERFACE_VERSION;
		}

		return VK_SUCCESS;
	}

	VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName)
	{
		return VSA::LayerManager::vki_GetInstanceProcAddr( instance, pName );
	}

	VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* pName)
	{
		return VSA::LayerManager::vki_GetDeviceProcAddr( device, pName );
	}

	VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
																						const char*      pLayerName,
																						uint32_t*        pPropertyCount,
																						VkExtensionProperties* pProperties)
	{
		ASSERT( physicalDevice == VK_NULL_HANDLE );
		return VSA::LayerManager::vki_EnumerateDeviceExtensionProperties( physicalDevice, pLayerName, pPropertyCount, pProperties );
	}

	VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
		const char* pLayerName, uint32_t* pPropertyCount, VkExtensionProperties* pProperties)
	{
		return VSA::LayerManager::vki_EnumerateInstanceExtensionProperties( pLayerName, pPropertyCount, pProperties );
	}

	VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t*          pPropertyCount,
																					  VkLayerProperties* pProperties)
	{
		return VSA::LayerManager::vki_EnumerateInstanceLayerProperties( pPropertyCount, pProperties );
	}

	VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(VkPhysicalDevice   physicalDevice,
																					uint32_t*          pPropertyCount,
																					VkLayerProperties* pProperties)
	{
		ASSERT( physicalDevice == VK_NULL_HANDLE );
		return VSA::LayerManager::vki_EnumerateDeviceLayerProperties( physicalDevice, pPropertyCount, pProperties );
	}

} // extern "C"
