// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Platforms/WindowsHeader.h"
#include "stl/CompileTime/FunctionInfo.h"

#include "src/IAnalyzer.h"

#include <mutex>

namespace VSA
{

	//
	// Layer Manager
	//

	class LayerManager
	{
	// types
	public:
		class LayerInstance;
		using LayerInstancePtr	= SharedPtr< LayerInstance >;

	private:
		template <typename T>
		using HandleToLayer		= HashMap< T, LayerInstancePtr >;
		using FnTable_t			= HashMap< String, PFN_vkVoidFunction >;


	// variables
	private:
		mutable std::mutex				_lock;
		HandleToLayer<VkInstance>		_instanceToLayer;
		HandleToLayer<VkPhysicalDevice>	_pdeviceToLayer;
		HandleToLayer<VkDevice>			_deviceToLayer;
		HandleToLayer<VkQueue>			_queueToLayer;
		HandleToLayer<VkCommandBuffer>	_cmdBufferToLayer;
		HandleToLayer<void*>			_windowToLayer;
		FnTable_t						_instanceTable;
		FnTable_t						_deviceTable;
		
		#ifdef VK_USE_PLATFORM_WIN32_KHR
			HHOOK						_wndHook	= null;
		#endif

		static constexpr uint			_captureSize = 5;	// frames


	// methods
	public:
		LayerManager ();
		~LayerManager ();

		ND_ PFN_vkVoidFunction  GetInstanceFn (const String &name) const;
		ND_ PFN_vkVoidFunction  GetDeviceFn (const String &name) const;

		ND_ static LayerManager&  Instance ();

		ND_ static LayerInstancePtr  Layer (VkInstance);
		ND_ static LayerInstancePtr  Layer (VkPhysicalDevice);
		ND_ static LayerInstancePtr  Layer (VkDevice);
		ND_ static LayerInstancePtr  Layer (VkQueue);
		ND_ static LayerInstancePtr  Layer (VkCommandBuffer);
		ND_ static LayerInstancePtr  LayerFromWnd (void* wnd);
		

	// vulkan interceptor
	public:
		static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vki_GetInstanceProcAddr(
			VkInstance                                  instance,
			const char*                                 pName);
		
		static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vki_GetDeviceProcAddr(
			VkDevice                                    device,
			const char*                                 pName);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateInstance(
			const VkInstanceCreateInfo*                 pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkInstance*                                 pInstance);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroyInstance(
			VkInstance                                  instance,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_EnumeratePhysicalDevices(
			VkInstance                                  instance,
			uint32_t*                                   pPhysicalDeviceCount,
			VkPhysicalDevice*                           pPhysicalDevices);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_EnumerateInstanceLayerProperties(
			uint32_t*                                   pPropertyCount,
			VkLayerProperties*                          pProperties);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_EnumerateInstanceExtensionProperties(
			const char*                                 pLayerName,
			uint32_t*                                   pPropertyCount,
			VkExtensionProperties*                      pProperties);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_EnumerateDeviceExtensionProperties(
			VkPhysicalDevice                            physicalDevice,
			const char*                                 pLayerName,
			uint32_t*                                   pPropertyCount,
			VkExtensionProperties*                      pProperties);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_EnumerateDeviceLayerProperties(
			VkPhysicalDevice                            physicalDevice,
			uint32_t*                                   pPropertyCount,
			VkLayerProperties*                          pProperties);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateDevice(
			VkPhysicalDevice                            physicalDevice,
			const VkDeviceCreateInfo*                   pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkDevice*                                   pDevice);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroyDevice(
			VkDevice                                    device,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR void VKAPI_CALL vki_GetDeviceQueue(
			VkDevice                                    device,
			uint32_t                                    queueFamilyIndex,
			uint32_t                                    queueIndex,
			VkQueue*                                    pQueue);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_QueueSubmit(
			VkQueue                                     queue,
			uint32_t                                    submitCount,
			const VkSubmitInfo*                         pSubmits,
			VkFence                                     fence);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_QueueWaitIdle(
			VkQueue                                     queue);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_DeviceWaitIdle(
			VkDevice                                    device);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_QueueBindSparse(
			VkQueue                                     queue,
			uint32_t                                    bindInfoCount,
			const VkBindSparseInfo*                     pBindInfo,
			VkFence                                     fence);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateFence(
			VkDevice                                    device,
			const VkFenceCreateInfo*                    pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkFence*                                    pFence);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroyFence(
			VkDevice                                    device,
			VkFence                                     fence,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_ResetFences(
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_GetFenceStatus(
			VkDevice                                    device,
			VkFence                                     fence);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_WaitForFences(
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences,
			VkBool32                                    waitAll,
			uint64_t                                    timeout);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateSemaphore(
			VkDevice                                    device,
			const VkSemaphoreCreateInfo*                pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSemaphore*                                pSemaphore);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroySemaphore(
			VkDevice                                    device,
			VkSemaphore                                 semaphore,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateEvent(
			VkDevice                                    device,
			const VkEventCreateInfo*                    pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkEvent*                                    pEvent);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroyEvent(
			VkDevice                                    device,
			VkEvent                                     event,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_SetEvent(
			VkDevice                                    device,
			VkEvent                                     event);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_ResetEvent(
			VkDevice                                    device,
			VkEvent                                     event);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateCommandPool(
			VkDevice                                    device,
			const VkCommandPoolCreateInfo*              pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkCommandPool*                              pCommandPool);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroyCommandPool(
			VkDevice                                    device,
			VkCommandPool                               commandPool,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_ResetCommandPool(
			VkDevice                                    device,
			VkCommandPool                               commandPool,
			VkCommandPoolResetFlags                     flags);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_AllocateCommandBuffers(
			VkDevice                                    device,
			const VkCommandBufferAllocateInfo*          pAllocateInfo,
			VkCommandBuffer*                            pCommandBuffers);

		static VKAPI_ATTR void VKAPI_CALL vki_FreeCommandBuffers(
			VkDevice                                    device,
			VkCommandPool                               commandPool,
			uint32_t                                    commandBufferCount,
			const VkCommandBuffer*                      pCommandBuffers);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_BeginCommandBuffer(
			VkCommandBuffer                             commandBuffer,
			const VkCommandBufferBeginInfo*             pBeginInfo);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_EndCommandBuffer(
			VkCommandBuffer                             commandBuffer);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_ResetCommandBuffer(
			VkCommandBuffer                             commandBuffer,
			VkCommandBufferResetFlags                   flags);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdSetEvent(
			VkCommandBuffer                             commandBuffer,
			VkEvent                                     event,
			VkPipelineStageFlags                        stageMask);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdResetEvent(
			VkCommandBuffer                             commandBuffer,
			VkEvent                                     event,
			VkPipelineStageFlags                        stageMask);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdWaitEvents(
			VkCommandBuffer                             commandBuffer,
			uint32_t                                    eventCount,
			const VkEvent*                              pEvents,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        dstStageMask,
			uint32_t                                    memoryBarrierCount,
			const VkMemoryBarrier*                      pMemoryBarriers,
			uint32_t                                    bufferMemoryBarrierCount,
			const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
			uint32_t                                    imageMemoryBarrierCount,
			const VkImageMemoryBarrier*                 pImageMemoryBarriers);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdPipelineBarrier(
			VkCommandBuffer                             commandBuffer,
			VkPipelineStageFlags                        srcStageMask,
			VkPipelineStageFlags                        dstStageMask,
			VkDependencyFlags                           dependencyFlags,
			uint32_t                                    memoryBarrierCount,
			const VkMemoryBarrier*                      pMemoryBarriers,
			uint32_t                                    bufferMemoryBarrierCount,
			const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
			uint32_t                                    imageMemoryBarrierCount,
			const VkImageMemoryBarrier*                 pImageMemoryBarriers);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdBeginRenderPass(
			VkCommandBuffer                             commandBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			VkSubpassContents                           contents);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdNextSubpass(
			VkCommandBuffer                             commandBuffer,
			VkSubpassContents                           contents);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdEndRenderPass(
			VkCommandBuffer                             commandBuffer);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdExecuteCommands(
			VkCommandBuffer                             commandBuffer,
			uint32_t                                    commandBufferCount,
			const VkCommandBuffer*                      pCommandBuffers);

		static VKAPI_ATTR void VKAPI_CALL vki_TrimCommandPool(
			VkDevice                                    device,
			VkCommandPool                               commandPool,
			VkCommandPoolTrimFlags                      flags);

		static VKAPI_ATTR void VKAPI_CALL vki_GetDeviceQueue2(
			VkDevice                                    device,
			const VkDeviceQueueInfo2*                   pQueueInfo,
			VkQueue*                                    pQueue);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateSwapchainKHR(
			VkDevice                                    device,
			const VkSwapchainCreateInfoKHR*             pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSwapchainKHR*                             pSwapchain);

		static VKAPI_ATTR void VKAPI_CALL vki_DestroySwapchainKHR(
			VkDevice                                    device,
			VkSwapchainKHR                              swapchain,
			const VkAllocationCallbacks*                pAllocator);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_AcquireNextImageKHR(
			VkDevice                                    device,
			VkSwapchainKHR                              swapchain,
			uint64_t                                    timeout,
			VkSemaphore                                 semaphore,
			VkFence                                     fence,
			uint32_t*                                   pImageIndex);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_AcquireNextImage2KHR(
			VkDevice                                    device,
			const VkAcquireNextImageInfoKHR*            pAcquireInfo,
			uint32_t*                                   pImageIndex);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_QueuePresentKHR(
			VkQueue                                     queue,
			const VkPresentInfoKHR*                     pPresentInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdBeginRenderPass2KHR(
			VkCommandBuffer                             commandBuffer,
			const VkRenderPassBeginInfo*                pRenderPassBegin,
			const VkSubpassBeginInfoKHR*                pSubpassBeginInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdNextSubpass2KHR(
			VkCommandBuffer                             commandBuffer,
			const VkSubpassBeginInfoKHR*                pSubpassBeginInfo,
			const VkSubpassEndInfoKHR*                  pSubpassEndInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdEndRenderPass2KHR(
			VkCommandBuffer                             commandBuffer,
			const VkSubpassEndInfoKHR*                  pSubpassEndInfo);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_DebugMarkerSetObjectTagEXT(
			VkDevice                                    device,
			const VkDebugMarkerObjectTagInfoEXT*        pTagInfo);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_DebugMarkerSetObjectNameEXT(
			VkDevice                                    device,
			const VkDebugMarkerObjectNameInfoEXT*       pNameInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdDebugMarkerBeginEXT(
			VkCommandBuffer                             commandBuffer,
			const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdDebugMarkerEndEXT(
			VkCommandBuffer                             commandBuffer);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdDebugMarkerInsertEXT(
			VkCommandBuffer                             commandBuffer,
			const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo);
		
		static VKAPI_ATTR VkResult VKAPI_CALL vki_SetDebugUtilsObjectNameEXT(
			VkDevice                                    device,
			const VkDebugUtilsObjectNameInfoEXT*        pNameInfo);

		static VKAPI_ATTR VkResult VKAPI_CALL vki_SetDebugUtilsObjectTagEXT(
			VkDevice                                    device,
			const VkDebugUtilsObjectTagInfoEXT*         pTagInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_QueueBeginDebugUtilsLabelEXT(
			VkQueue                                     queue,
			const VkDebugUtilsLabelEXT*                 pLabelInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_QueueEndDebugUtilsLabelEXT(
			VkQueue                                     queue);

		static VKAPI_ATTR void VKAPI_CALL vki_QueueInsertDebugUtilsLabelEXT(
			VkQueue                                     queue,
			const VkDebugUtilsLabelEXT*                 pLabelInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdBeginDebugUtilsLabelEXT(
			VkCommandBuffer                             commandBuffer,
			const VkDebugUtilsLabelEXT*                 pLabelInfo);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdEndDebugUtilsLabelEXT(
			VkCommandBuffer                             commandBuffer);

		static VKAPI_ATTR void VKAPI_CALL vki_CmdInsertDebugUtilsLabelEXT(
			VkCommandBuffer                             commandBuffer,
			const VkDebugUtilsLabelEXT*                 pLabelInfo);

	#ifdef VK_USE_PLATFORM_WIN32_KHR
		static VKAPI_ATTR VkResult VKAPI_CALL vki_CreateWin32SurfaceKHR(
			VkInstance                                  instance,
			const VkWin32SurfaceCreateInfoKHR*          pCreateInfo,
			const VkAllocationCallbacks*                pAllocator,
			VkSurfaceKHR*                               pSurface);

		static LRESULT CALLBACK winapi_GetMsgProc(
		  _In_ int    code,
		  _In_ WPARAM wParam,
		  _In_ LPARAM lParam);
	#endif
	};

	
	#define INSTANCE_FN_LIST( _visitor_ ) \
		_visitor_( CreateInstance ) \
		_visitor_( DestroyInstance ) \
		_visitor_( EnumeratePhysicalDevices ) \
		_visitor_( EnumerateDeviceExtensionProperties ) \
		_visitor_( CreateDevice )

	#define DEVICE_FN_LIST( _visitor_ ) \
		_visitor_( DestroyDevice ) \
		_visitor_( GetDeviceQueue ) \
		_visitor_( QueueSubmit ) \
		_visitor_( QueueWaitIdle ) \
		_visitor_( DeviceWaitIdle ) \
		_visitor_( QueueBindSparse ) \
		_visitor_( CreateFence ) \
		_visitor_( DestroyFence ) \
		_visitor_( ResetFences ) \
		_visitor_( GetFenceStatus ) \
		_visitor_( WaitForFences ) \
		_visitor_( CreateSemaphore ) \
		_visitor_( DestroySemaphore ) \
		_visitor_( CreateEvent ) \
		_visitor_( DestroyEvent ) \
		_visitor_( SetEvent ) \
		_visitor_( ResetEvent ) \
		_visitor_( CreateCommandPool ) \
		_visitor_( DestroyCommandPool ) \
		_visitor_( ResetCommandPool ) \
		_visitor_( AllocateCommandBuffers ) \
		_visitor_( FreeCommandBuffers ) \
		_visitor_( BeginCommandBuffer ) \
		_visitor_( EndCommandBuffer ) \
		_visitor_( ResetCommandBuffer ) \
		_visitor_( CmdSetEvent ) \
		_visitor_( CmdResetEvent ) \
		_visitor_( CmdWaitEvents ) \
		_visitor_( CmdPipelineBarrier ) \
		_visitor_( CmdBeginRenderPass ) \
		_visitor_( CmdNextSubpass ) \
		_visitor_( CmdEndRenderPass ) \
		_visitor_( CmdExecuteCommands ) \
		_visitor_( TrimCommandPool ) \
		_visitor_( GetDeviceQueue2 ) \
		_visitor_( CreateSwapchainKHR ) \
		_visitor_( DestroySwapchainKHR ) \
		_visitor_( AcquireNextImageKHR ) \
		_visitor_( AcquireNextImage2KHR ) \
		_visitor_( QueuePresentKHR ) \
		_visitor_( CmdBeginRenderPass2KHR ) \
		_visitor_( CmdNextSubpass2KHR ) \
		_visitor_( CmdEndRenderPass2KHR ) \
		_visitor_( DebugMarkerSetObjectTagEXT ) \
		_visitor_( DebugMarkerSetObjectNameEXT ) \
		_visitor_( CmdDebugMarkerBeginEXT ) \
		_visitor_( CmdDebugMarkerEndEXT ) \
		_visitor_( CmdDebugMarkerInsertEXT ) \
		_visitor_( SetDebugUtilsObjectNameEXT ) \
		_visitor_( SetDebugUtilsObjectTagEXT ) \
		_visitor_( QueueBeginDebugUtilsLabelEXT ) \
		_visitor_( QueueEndDebugUtilsLabelEXT ) \
		_visitor_( QueueInsertDebugUtilsLabelEXT ) \
		_visitor_( CmdBeginDebugUtilsLabelEXT ) \
		_visitor_( CmdEndDebugUtilsLabelEXT ) \
		_visitor_( CmdInsertDebugUtilsLabelEXT )
	


	//
	// Layer Instance
	//

	class LayerManager::LayerInstance final : public std::enable_shared_from_this< LayerInstance >
	{
		friend class LayerManager;

	// types
	private:
		template <typename Fn>
		struct _Callback2 {
			using _FI = FunctionInfo< Fn >;
			using type = Function< void (const typename _FI::args::AsTuple &) >;
		};

		template <typename Fn>
		struct _Callback3 {
			using _FI = FunctionInfo< Fn >;
			using type = Function< void (const typename _FI::args::AsTuple &, typename _FI::result) >;
		};

		template <typename Fn>
		struct Callback {
			using fn_type = typename Conditional<
								std::is_void_v< typename FunctionInfo< Fn >::result >,
								_Callback2< Fn> , 
								_Callback3< Fn > >::type;
			using type = Array< fn_type >;
		};

		template <typename Fn>
		struct FnWrap {
			using _FI = FunctionInfo< Fn >;

			Fn _fn = null;

			void operator = (Fn fn) {
				_fn = fn;
			}

			ND_ explicit operator bool () const {
				return _fn != null;
			}

			template <typename ...Args>
			typename _FI::result  operator () (Args&& ...args) const {
				CHECK( _fn );
				return _fn( std::forward<Args>(args)... );
			}
		};

		using Analyzers_t = Array< SharedPtr< IAnalyzer >>;


	// variables
	private:
		VkInstance					_instance				= VK_NULL_HANDLE;
		VkPhysicalDevice			_physicalDevice			= VK_NULL_HANDLE;
		VkDevice					_logicalDevice			= VK_NULL_HANDLE;
		PFN_vkGetInstanceProcAddr	_getInstanceProcAddr	= null;
		PFN_vkGetDeviceProcAddr		_getDeviceProcAddr		= null;
		Analyzers_t					_analyzers;
		int							_capturedFrames			= -1;

		struct {
			#ifdef VK_USE_PLATFORM_WIN32_KHR
			HINSTANCE					hinstance	= null;
			HWND						hwnd		= null;
			#endif
		}							_os;

		struct {
			#define VISITOR( _name_ )	typename Callback< PFN_vk ## _name_ >::type  _name_;
			INSTANCE_FN_LIST( VISITOR )
			DEVICE_FN_LIST( VISITOR )
			#undef VISITOR
		}							_fnTable;
		
		struct {
			#define VISITOR( _name_ )	FnWrap< PFN_vk ## _name_ >  _name_;
			INSTANCE_FN_LIST( VISITOR )
			#undef VISITOR

			#ifdef VK_USE_PLATFORM_WIN32_KHR
			FnWrap<PFN_vkCreateWin32SurfaceKHR>  CreateWin32SurfaceKHR;
			#endif
		}							_instFn;

		struct {
			#define VISITOR( _name_ )	FnWrap< PFN_vk ## _name_ >  _name_;
			DEVICE_FN_LIST( VISITOR )
			#undef VISITOR
		}							_devFn;


	// methods
	public:
		LayerInstance ();

		ND_ bool						IsStarted ()		const	{ return _capturedFrames > 0; }

		ND_ VkInstance					Instance ()			const	{ return _instance; }
		ND_ VkPhysicalDevice			PhysicalDevice ()	const	{ return _physicalDevice; }
		ND_ VkDevice					LogicalDevice ()	const	{ return _logicalDevice; }
			
		ND_ PFN_vkGetInstanceProcAddr	InstanceProcAddr ()	const	{ return _getInstanceProcAddr; }
		ND_ PFN_vkGetDeviceProcAddr		DeviceProcAddr ()	const	{ return _getDeviceProcAddr; }

	private:
		void _Init1 (VkInstance inst, PFN_vkGetInstanceProcAddr gpa);
		void _Init2 (VkPhysicalDevice pd, VkDevice ld, PFN_vkGetDeviceProcAddr gpa);

		void _RegisterSyncAnalyzer ();

		void _Start (uint frames);
		void _Update ();
	};

}	// VSA
