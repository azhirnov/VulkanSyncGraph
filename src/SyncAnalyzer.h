// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "vulkan/vulkan.h"
#include <thread>
#include <mutex>

#include "stl/Containers/Ptr.h"
#include "stl/Containers/Union.h"
#include "stl/Math/Color.h"

#include "src/IAnalyzer.h"

namespace VSA
{

	//
	// Sync Analyzer
	//

	class SyncAnalyzer final : public IAnalyzer
	{
	// types
	private:
		enum class ThreadID : uint {};
		enum class TimePoint : uint {};
		enum class UID : uint {};

		struct BaseCpuSyncEvent {
			VkDevice			dev			= VK_NULL_HANDLE;
			ThreadID			threadId;
			TimePoint			time;
			UID					uid;
		};

		struct QueueSubmit : BaseCpuSyncEvent
		{
			Array<UID>			batches;
		};

		struct FenceSignal : BaseCpuSyncEvent
		{
			Array<UID>			dependsOn;
			VkQueue				queue		= VK_NULL_HANDLE;
			VkFence				fence		= VK_NULL_HANDLE;
		};

		struct CmdBatch : BaseCpuSyncEvent
		{
			VkQueue				queue		= VK_NULL_HANDLE;
			Array<VkSemaphore>	signalSemaphores;
			Array<VkSemaphore>	waitSemaphores;
			Array<UID>			waitDeps;
		};

		struct QueueWaitIdle : BaseCpuSyncEvent
		{
			VkQueue				queue		= VK_NULL_HANDLE;
		};

		struct DeviceWaitIdle : BaseCpuSyncEvent
		{
		};

		struct WaitForFences : BaseCpuSyncEvent
		{
			Array<UID>			fenceDeps;
			Array<VkFence>		fences;
			bool				waitForAll;
			bool				timeout;
		};

		struct AcquireImage : BaseCpuSyncEvent
		{
			VkSwapchainKHR		swapchain	= VK_NULL_HANDLE;
			VkSemaphore			sem			= VK_NULL_HANDLE;
			VkFence				fence		= VK_NULL_HANDLE;
		};

		struct QueuePresent : BaseCpuSyncEvent
		{
			VkQueue					queue		= VK_NULL_HANDLE;
			Array<VkSemaphore>		waitSemaphores;
			Array<VkSwapchainKHR>	swapchains;
			Array<UID>				waitDeps;
		};

		using GlobalSyncs_t = Array< Union< QueueSubmit, CmdBatch, FenceSignal, QueueWaitIdle, DeviceWaitIdle, WaitForFences, AcquireImage, QueuePresent >>;


	private:
		struct DeviceInfo
		{
			VkDevice			id		= VK_NULL_HANDLE;
			HashSet<VkQueue>	queues;
		};

		struct QueueInfo
		{
			VkQueue				id		= VK_NULL_HANDLE;
			VkDevice			dev		= VK_NULL_HANDLE;
			uint32_t			queueFamilyIndex;
			uint32_t			queueIndex;
			String				name;
		};

		/*struct CommandPoolInfo
		{
			VkCommandPool			id			= VK_NULL_HANDLE;
			Array<VkCommandBuffer>	cmdBuffers;
		};*/

		using DeviceMap_t	= HashMap< VkDevice, DeviceInfo >;
		using QueueMap_t	= HashMap< VkQueue, QueueInfo >;
		using ThreadIDs_t	= HashMap< std::thread::id, ThreadID >;
		using ThreadNames_t	= HashMap< ThreadID, String >;
		using TimePoint_t	= std::chrono::high_resolution_clock::time_point;
		
		using SignalSemaphores_t= HashMap< VkSemaphore, UID >;
		using SignalFences_t	= HashMap< VkFence, Array<UID> >;
		
		struct NodeStyle {
			uint		fontSize	= 10;
			RGBA8u		bgColor		= HtmlColor::White;
			RGBA8u		labelColor	= HtmlColor::Black;
		};


	// variables
	private:
		std::recursive_mutex	_lock;
		VkInstance				_instance	= VK_NULL_HANDLE;
		DeviceMap_t				_devices;
		QueueMap_t				_queues;
		GlobalSyncs_t			_globalSyncs;
		SignalSemaphores_t		_signalSemaphores;
		SignalFences_t			_signalFences;

		ThreadIDs_t				_threadIds;
		ThreadNames_t			_threadNames;
		uint					_threadIdCounter	= 0;
		bool					_enabled			= false;
		TimePoint_t				_startTime;
		uint					_uidCounter			= 0;


	// methods
	public:
		SyncAnalyzer () {}
		
		void Start () override;
		void Stop () override;

		void vki_GetDeviceQueue(
			VkDevice                                    device,
			uint32_t                                    queueFamilyIndex,
			uint32_t                                    queueIndex,
			VkQueue*                                    pQueue);
		
		void vki_GetDeviceQueue2(
			VkDevice                                    device,
			const VkDeviceQueueInfo2*                   pQueueInfo,
			VkQueue*                                    pQueue);

		void vki_QueueSubmit(
			VkQueue                                     queue,
			uint32_t                                    submitCount,
			const VkSubmitInfo*                         pSubmits,
			VkFence                                     fence,
			VkResult                                    result);

		void vki_QueueWaitIdle(
			VkQueue                                     queue,
			VkResult                                    result);

		void vki_DeviceWaitIdle(
			VkDevice                                    device,
			VkResult                                    result);

		void vki_QueueBindSparse(
			VkQueue                                     queue,
			uint32_t                                    bindInfoCount,
			const VkBindSparseInfo*                     pBindInfo,
			VkFence                                     fence,
			VkResult                                    result);
		
		void vki_ResetFences(
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences,
			VkResult                                    result);

		void vki_WaitForFences(
			VkDevice                                    device,
			uint32_t                                    fenceCount,
			const VkFence*                              pFences,
			VkBool32                                    waitAll,
			uint64_t                                    timeout,
			VkResult                                    result);
		
		void vki_AcquireNextImageKHR(
			VkDevice                                    device,
			VkSwapchainKHR                              swapchain,
			uint64_t                                    timeout,
			VkSemaphore                                 semaphore,
			VkFence                                     fence,
			uint32_t*                                   pImageIndex,
			VkResult                                    result);
		
		void vki_AcquireNextImage2KHR(
			VkDevice                                    device,
			const VkAcquireNextImageInfoKHR*            pAcquireInfo,
			uint32_t*                                   pImageIndex,
			VkResult                                    result);

		void vki_QueuePresentKHR(
			VkQueue                                     queue,
			const VkPresentInfoKHR*                     pPresentInfo,
			VkResult                                    result);
		
		void vki_DebugMarkerSetObjectNameEXT(
			VkDevice                                    device,
			const VkDebugMarkerObjectNameInfoEXT*       pNameInfo,
			VkResult                                    result);

		void vki_SetDebugUtilsObjectNameEXT(
			VkDevice                                    device,
			const VkDebugUtilsObjectNameInfoEXT*        pNameInfo,
			VkResult                                    result);

	private:
		ND_ ThreadID  _GetThreadID ();
		ND_ TimePoint  _GetTimePoint ();
		ND_ UID  _GetUID ();

		ND_ String  _QueueName (VkQueue q) const;
		ND_ String  _ThreadName (ThreadID tid) const;
		
		static String  _ToCpuNodeName (UID id);
		static String  _ToGpuNodeName (UID id);
		static String  _ToNodeStyle (StringView name, const NodeStyle &style);
		static String  _MakeSemaphoreEdge (UID from, UID to);
		static String  _MakeCpuToGpuSyncEdge (UID fromCpu, UID toGpu);
		static String  _MakeGpuToCpuSyncEdge (UID fromGpu, UID toCpu);

		bool _SaveDotFile () const;
		void _Clear ();
		
		bool _Visualize (StringView graph, StringView filepath, StringView format) const;
	};


}	// VSA
