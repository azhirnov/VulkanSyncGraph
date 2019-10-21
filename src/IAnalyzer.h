// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "vulkan/vulkan.h"
#include "stl/Common.h"

namespace VSA
{

	//
	// Analyzer interface
	//

	class IAnalyzer : public std::enable_shared_from_this< IAnalyzer >
	{
	public:
		virtual void OnCreateInstance (VkInstance, PFN_vkGetInstanceProcAddr) {}

		virtual void OnCreateDevice (VkInstance, VkPhysicalDevice, VkDevice,
									 PFN_vkGetInstanceProcAddr, PFN_vkGetDeviceProcAddr) {}

		virtual void Start () = 0;
		virtual void Stop () = 0;
	};


}	// VSA
