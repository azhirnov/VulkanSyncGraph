// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "src/LayerManager.h"
#include "src/SyncAnalyzer.h"

#include "stl/Containers/Singleton.h"
#include "stl/Algorithms/StringUtils.h"

#include "vulkan/vk_layer.h"

namespace VSA
{
	static const VkLayerProperties LayerProps = {
		VSA_LAYER_NAME,
		VK_MAKE_VERSION( 1, 0, VK_HEADER_VERSION ),
		1,
		"sync analysis layer",
	};
	
/*
=================================================
	constructor
=================================================
*/
	LayerManager::LayerInstance::LayerInstance ()
	{
		_RegisterSyncAnalyzer();
	}

/*
=================================================
	_Start
=================================================
*/
	void LayerManager::LayerInstance::_Start (uint frames)
	{
		_capturedFrames = frames;

		for (auto& an : _analyzers) {
			an->Start();
		}
	}
	
/*
=================================================
	_Update
=================================================
*/
	void LayerManager::LayerInstance::_Update ()
	{
		if ( _capturedFrames < 0 )
			return;

		if ( (--_capturedFrames) == 0 )
		{
			for (auto& an : _analyzers) {
				an->Stop();
			}
		}
	}

/*
=================================================
	_Init1
=================================================
*/
	void LayerManager::LayerInstance::_Init1 (VkInstance inst, PFN_vkGetInstanceProcAddr gpa)
	{
		_instance				= inst;
		_getInstanceProcAddr	= gpa;

		#define VISITOR( _name_ )	_instFn._name_ = BitCast<PFN_vk ## _name_>(gpa( inst, "vk" #_name_ ));
		INSTANCE_FN_LIST( VISITOR )
		#undef VISITOR
			
		#ifdef VK_USE_PLATFORM_WIN32_KHR
			_instFn.CreateWin32SurfaceKHR = BitCast<PFN_vkCreateWin32SurfaceKHR>(gpa( inst, "vkCreateWin32SurfaceKHR" ));
		#endif
		
		for (auto& an : _analyzers) {
			an->OnCreateInstance( _instance, _getInstanceProcAddr );
		}
	}

/*
=================================================
	_Init2
=================================================
*/
	void LayerManager::LayerInstance::_Init2 (VkPhysicalDevice pd, VkDevice ld, PFN_vkGetDeviceProcAddr gpa)
	{
		_physicalDevice		= pd;
		_logicalDevice		= ld;
		_getDeviceProcAddr	= gpa;

		#define VISITOR( _name_ )	_devFn._name_ = BitCast<PFN_vk ## _name_>(gpa( ld, "vk" #_name_ ));
		DEVICE_FN_LIST( VISITOR )
		#undef VISITOR
			
		for (auto& an : _analyzers) {
			an->OnCreateDevice( _instance, _physicalDevice, _logicalDevice, _getInstanceProcAddr, _getDeviceProcAddr );
		}
	}
	
/*
=================================================
	_RegisterSyncAnalyzer
=================================================
*/
	void LayerManager::LayerInstance::_RegisterSyncAnalyzer ()
	{
		auto	sa = MakeShared<SyncAnalyzer>();

		#define ADD_CB( _name_ ) \
			_fnTable._name_.push_back( [sa] (const auto& argsInTuple) { \
				std::apply( [sa] (auto... args) { sa->vki_ ## _name_( std::forward<decltype(args)>(args)... ); }, \
						    argsInTuple ); \
			})
		#define ADD_CB2( _name_ ) \
			_fnTable._name_.push_back( [sa] (const auto& argsInTuple, VkResult res) { \
				std::apply( [sa, res] (auto... args) { sa->vki_ ## _name_( std::forward<decltype(args)>(args)..., res ); }, \
						    argsInTuple ); \
			})

		ADD_CB( GetDeviceQueue );
		ADD_CB( GetDeviceQueue2 );
		ADD_CB2( QueueSubmit );
		ADD_CB2( QueueWaitIdle );
		ADD_CB2( DeviceWaitIdle );
		ADD_CB2( QueueBindSparse );
		ADD_CB2( ResetFences );
		ADD_CB2( GetFenceStatus );
		ADD_CB2( WaitForFences );
		ADD_CB2( AcquireNextImageKHR );
		ADD_CB2( AcquireNextImage2KHR );
		ADD_CB2( QueuePresentKHR );
		ADD_CB2( DebugMarkerSetObjectNameEXT );
		ADD_CB2( SetDebugUtilsObjectNameEXT );

		#undef ADD_CB
		#undef ADD_CB2

		_analyzers.push_back( sa );
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	constructor
=================================================
*/
	LayerManager::LayerManager ()
	{
		#ifdef VK_USE_PLATFORM_WIN32_KHR
			_wndHook = ::SetWindowsHookExA( WH_GETMESSAGE, &LayerManager::winapi_GetMsgProc,
											::GetModuleHandle( LPCSTR(null) ), ::GetCurrentThreadId() );
			CHECK( _wndHook );
		#endif

		#define VISITOR( _name_ )	_instanceTable["vk" #_name_ ] = BitCast<PFN_vkVoidFunction>( &vki_ ## _name_ );
		INSTANCE_FN_LIST( VISITOR )
		#undef VISITOR
		
		_instanceTable["vkGetInstanceProcAddr" ] = BitCast<PFN_vkVoidFunction>( &vki_GetInstanceProcAddr );
		_instanceTable["vkGetDeviceProcAddr" ] = BitCast<PFN_vkVoidFunction>( &vki_GetDeviceProcAddr );
		_instanceTable["vkEnumerateDeviceExtensionProperties" ] = BitCast<PFN_vkVoidFunction>( &vki_EnumerateDeviceExtensionProperties );
		_instanceTable["vkEnumerateInstanceLayerProperties" ] = BitCast<PFN_vkVoidFunction>( &vki_EnumerateInstanceLayerProperties );
		_instanceTable["vkEnumerateInstanceExtensionProperties" ] = BitCast<PFN_vkVoidFunction>( &vki_EnumerateInstanceExtensionProperties );
		_instanceTable["vkEnumerateDeviceLayerProperties" ] = BitCast<PFN_vkVoidFunction>( &vki_EnumerateDeviceLayerProperties );

		#ifdef VK_USE_PLATFORM_WIN32_KHR
		_instanceTable["vkCreateWin32SurfaceKHR"] = BitCast<PFN_vkVoidFunction>( &vki_CreateWin32SurfaceKHR );
		#endif
			
		#define VISITOR( _name_ )	_deviceTable["vk" #_name_ ] = BitCast<PFN_vkVoidFunction>( &vki_ ## _name_ );
		DEVICE_FN_LIST( VISITOR )
		#undef VISITOR
	}

/*
=================================================
	destructor
=================================================
*/
	LayerManager::~LayerManager ()
	{
		#ifdef VK_USE_PLATFORM_WIN32_KHR
		if ( _wndHook )
			::UnhookWindowsHookEx( _wndHook );
		#endif
	}

/*
=================================================
	GetInstanceFn
=================================================
*/
	PFN_vkVoidFunction  LayerManager::GetInstanceFn (const String &name) const
	{
		EXLOCK( _lock );
		auto	iter = _instanceTable.find( name );
		return iter != _instanceTable.end() ? iter->second : null;
	}
	
/*
=================================================
	GetDeviceFn
=================================================
*/
	PFN_vkVoidFunction  LayerManager::GetDeviceFn (const String &name) const
	{
		EXLOCK( _lock );
		auto	iter = _deviceTable.find( name );
		return iter != _deviceTable.end() ? iter->second : null;
	}

/*
=================================================
	Instance
=================================================
*/
	LayerManager&  LayerManager::Instance ()
	{
		return *Singleton<LayerManager>();
	}
	
/*
=================================================
	Layer
=================================================
*/
	LayerManager::LayerInstancePtr  LayerManager::Layer (VkInstance handle)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._instanceToLayer.find( handle );
		return iter != inst._instanceToLayer.end() ? iter->second : null;
	}

	LayerManager::LayerInstancePtr  LayerManager::Layer (VkPhysicalDevice handle)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._pdeviceToLayer.find( handle );
		return iter != inst._pdeviceToLayer.end() ? iter->second : null;
	}

	LayerManager::LayerInstancePtr  LayerManager::Layer (VkDevice handle)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._deviceToLayer.find( handle );
		return iter != inst._deviceToLayer.end() ? iter->second : null;
	}

	LayerManager::LayerInstancePtr  LayerManager::Layer (VkQueue handle)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._queueToLayer.find( handle );
		return iter != inst._queueToLayer.end() ? iter->second : null;
	}

	LayerManager::LayerInstancePtr  LayerManager::Layer (VkCommandBuffer handle)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._cmdBufferToLayer.find( handle );
		return iter != inst._cmdBufferToLayer.end() ? iter->second : null;
	}
	
	LayerManager::LayerInstancePtr  LayerManager::LayerFromWnd (void* wnd)
	{
		auto&	inst = Instance();
		EXLOCK( inst._lock );
		auto	iter = inst._windowToLayer.find( wnd );
		return iter != inst._windowToLayer.end() ? iter->second : null;
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	GetLoaderInstanceCI
=================================================
*/
	static const VkLayerInstanceCreateInfo* GetLoaderInstanceCI (const VkInstanceCreateInfo* pCreateInfo,
																 VkLayerFunction             func)
	{
		const VkLayerInstanceCreateInfo* chain_info = BitCast<const VkLayerInstanceCreateInfo *>(pCreateInfo->pNext);

		while ( chain_info and (chain_info->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO or chain_info->function != func) )
		{
			chain_info = BitCast<const VkLayerInstanceCreateInfo *>(chain_info->pNext);
		}

		return chain_info;
	}
	
/*
=================================================
	GetLoaderDeviceCI
=================================================
*/
	static const VkLayerDeviceCreateInfo* GetLoaderDeviceCI (const VkDeviceCreateInfo* pCreateInfo, VkLayerFunction func)
	{
		const VkLayerDeviceCreateInfo* chain_info = BitCast<const VkLayerDeviceCreateInfo*>(pCreateInfo->pNext);

		while ( chain_info and (chain_info->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO or chain_info->function != func) )
		{
			chain_info = BitCast<const VkLayerDeviceCreateInfo*>(chain_info->pNext);
		}

		return chain_info;
	}
	
/*
=================================================
	Call
=================================================
*/
	template <typename Fn, typename ...Args>
	inline void Call (const Fn &listeners, Args&& ...args)
	{
		for (auto& fn : listeners) {
			fn( std::forward<Args>(args)... );
		}
	}
//-----------------------------------------------------------------------------


/*
=================================================
	vki_CreateInstance
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateInstance (
		const VkInstanceCreateInfo*  pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkInstance*                  pInstance)
	{
		VkLayerInstanceCreateInfo*	inst_ci = const_cast<VkLayerInstanceCreateInfo *>( GetLoaderInstanceCI( pCreateInfo, VK_LAYER_LINK_INFO ));
		
		if ( not inst_ci or not inst_ci->u.pLayerInfo )
			return VK_ERROR_INITIALIZATION_FAILED;
		
		PFN_vkGetInstanceProcAddr	get_instance_proc_addr = inst_ci->u.pLayerInfo->pfnNextGetInstanceProcAddr;

		if ( not get_instance_proc_addr )
			return VK_ERROR_INITIALIZATION_FAILED;
		
		PFN_vkCreateInstance	create_instance = BitCast<PFN_vkCreateInstance>( get_instance_proc_addr( VK_NULL_HANDLE, "vkCreateInstance" ));

		if ( not create_instance )
			return VK_ERROR_INITIALIZATION_FAILED;

		inst_ci->u.pLayerInfo = inst_ci->u.pLayerInfo->pNext;

		VkResult	result = create_instance( pCreateInfo, pAllocator, OUT pInstance );
		
		if ( result == VK_SUCCESS and pInstance and *pInstance )
		{
			auto&	inst = Instance();
			EXLOCK( inst._lock );
			auto	iter = inst._instanceToLayer.insert_or_assign( *pInstance, MakeShared<LayerInstance>() ).first;
			iter->second->_Init1( *pInstance, get_instance_proc_addr );
			
			Call( iter->second->_fnTable.CreateInstance,
				  MakeTuple( pCreateInfo, pAllocator, pInstance ), result );

			VSA_LOGI( String(VSA_LAYER_NAME) << ": CreateInstance" );
		}

		return result;
	}

/*
=================================================
	vki_GetInstanceProcAddr
=================================================
*/
	VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LayerManager::vki_GetInstanceProcAddr(
		VkInstance                                  instance,
		const char*                                 pName)
	{
		if ( pName and StringView{pName} == "vkCreateInstance" )
		{
			return BitCast<PFN_vkVoidFunction>( &vki_CreateInstance );
		}

		auto&	inst = Instance();
		auto	res  = inst.GetInstanceFn( pName );

		if ( res )
			return res;

		if ( auto layer = Layer( instance ) )
		{
			return layer->InstanceProcAddr()( instance, pName );
		}

		return null;
	}
	
/*
=================================================
	vki_GetDeviceProcAddr
=================================================
*/
	VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL LayerManager::vki_GetDeviceProcAddr(
		VkDevice                                    device,
		const char*                                 pName)
	{
		auto&	inst = Instance();
		auto	res  = inst.GetDeviceFn( pName );

		if ( res )
			return res;

		if ( auto layer = Layer( device ) )
		{
			return layer->DeviceProcAddr()( device, pName );
		}

		return null;
	}
	
/*
=================================================
	vki_DestroyInstance
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroyInstance(
		VkInstance                                  instance,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( instance ) )
		{
			Call( layer->_fnTable.DestroyInstance, MakeTuple( instance, pAllocator ));
			
			VSA_LOGI( String(VSA_LAYER_NAME) << ": DestroyInstance" );

			layer->_instFn.DestroyInstance( instance, pAllocator );
		}

		auto&	inst = Instance();
		EXLOCK( inst._lock );
		inst._instanceToLayer.erase( instance );
	}
	
/*
=================================================
	vki_EnumeratePhysicalDevices
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EnumeratePhysicalDevices(
		VkInstance                                  instance,
		uint32_t*                                   pPhysicalDeviceCount,
		VkPhysicalDevice*                           pPhysicalDevices)
	{
		if ( auto layer = Layer( instance ) )
		{
			VkResult result = layer->_instFn.EnumeratePhysicalDevices( instance, INOUT pPhysicalDeviceCount, OUT pPhysicalDevices );

			if ( result == VK_SUCCESS and pPhysicalDevices )
			{
				auto&	inst = Instance();
				EXLOCK( inst._lock );

				for (uint i = 0; i < *pPhysicalDeviceCount; ++i) {
					inst._pdeviceToLayer.insert_or_assign( pPhysicalDevices[i], layer );
				}
			}
			return result;
		}
		
		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_EnumerateInstanceExtensionProperties
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EnumerateInstanceExtensionProperties(
		const char*                                 pLayerName,
		uint32_t*                                   pPropertyCount,
		VkExtensionProperties*                      )
	{
		if ( pLayerName and StringView{pLayerName} == LayerProps.layerName )
		{
			if ( not pPropertyCount )
			{
				*pPropertyCount = 0;
			}
			return VK_SUCCESS;
		}

		return VK_ERROR_LAYER_NOT_PRESENT;
	}
	
/*
=================================================
	vki_EnumerateInstanceLayerProperties
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EnumerateInstanceLayerProperties(
		uint32_t*                                   pPropertyCount,
		VkLayerProperties*                          pProperties)
	{
		if ( not pProperties )
		{
			if ( pPropertyCount )
			{
				*pPropertyCount = 1;
			}
			return VK_SUCCESS;
		}

		if ( not pPropertyCount and *pPropertyCount >= 1 )
		{
			memcpy( pProperties, &LayerProps, sizeof(LayerProps) );
			*pPropertyCount = 1;
			return VK_SUCCESS;
		}

		return VK_INCOMPLETE;
	}

/*
=================================================
	vki_EnumerateDeviceExtensionProperties
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EnumerateDeviceExtensionProperties(
		VkPhysicalDevice                            physicalDevice,
		const char*                                 pLayerName,
		uint32_t*                                   pPropertyCount,
		VkExtensionProperties*                      pProperties)
	{
		if ( pLayerName and StringView{pLayerName} == LayerProps.layerName )
		{
			if ( not pPropertyCount )
			{
				*pPropertyCount = 0;
			}
			return VK_SUCCESS;
		}

		if ( auto layer = Layer( physicalDevice ) )
		{
			return layer->_instFn.EnumerateDeviceExtensionProperties( physicalDevice, pLayerName, pPropertyCount, pProperties );
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_EnumerateDeviceLayerProperties
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EnumerateDeviceLayerProperties(
		VkPhysicalDevice                            ,
		uint32_t*                                   pPropertyCount,
		VkLayerProperties*                          pProperties)
	{
		return vki_EnumerateInstanceLayerProperties( pPropertyCount, pProperties );
	}
	
/*
=================================================
	vki_CreateDevice
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateDevice (
		VkPhysicalDevice             physicalDevice,
		const VkDeviceCreateInfo*    pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDevice*                    pDevice)
	{
		VkLayerDeviceCreateInfo*	dev_ci = const_cast<VkLayerDeviceCreateInfo *>( GetLoaderDeviceCI( pCreateInfo, VK_LAYER_LINK_INFO ));
		
		if ( not dev_ci or not dev_ci->u.pLayerInfo )
			return VK_ERROR_INITIALIZATION_FAILED;
		
		auto						layer					= LayerManager::Layer( physicalDevice );
		PFN_vkGetInstanceProcAddr	get_instance_proc_addr	= dev_ci->u.pLayerInfo->pfnNextGetInstanceProcAddr;
		PFN_vkGetDeviceProcAddr		get_device_proc_addr	= dev_ci->u.pLayerInfo->pfnNextGetDeviceProcAddr;

		if ( not layer or not get_instance_proc_addr or not get_device_proc_addr )
			return VK_ERROR_INITIALIZATION_FAILED;

		PFN_vkCreateDevice	create_device = BitCast<PFN_vkCreateDevice>( get_instance_proc_addr( layer->Instance(), "vkCreateDevice" ));

		if ( not create_device )
			return VK_ERROR_INITIALIZATION_FAILED;
		
		dev_ci->u.pLayerInfo = dev_ci->u.pLayerInfo->pNext;
			
		VkResult	result = create_device( physicalDevice, pCreateInfo, pAllocator, OUT pDevice );
		
		if ( result == VK_SUCCESS and pDevice and *pDevice )
		{
			auto&	inst = Instance();
			EXLOCK( inst._lock );
			inst._deviceToLayer.insert_or_assign( *pDevice, layer );
			layer->_Init2( physicalDevice, *pDevice, get_device_proc_addr );
			
			VSA_LOGI( String(VSA_LAYER_NAME) << ": CreateDevice" );
		}

		Call( layer->_fnTable.CreateDevice,
				MakeTuple( physicalDevice, pCreateInfo, pAllocator, pDevice ), result );

		return result;
	}
	
/*
=================================================
	vki_DestroyDevice
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroyDevice(
		VkDevice                                    device,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroyDevice, MakeTuple( device, pAllocator ));

			layer->_devFn.DestroyDevice( device, pAllocator );
			
			VSA_LOGI( String(VSA_LAYER_NAME) << ": DestroyDevice" );

			auto&	inst = Instance();
			EXLOCK( inst._lock );
			inst._deviceToLayer.erase( device );
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_GetDeviceQueue
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_GetDeviceQueue(
		VkDevice                                    device,
		uint32_t                                    queueFamilyIndex,
		uint32_t                                    queueIndex,
		VkQueue*                                    pQueue)
	{
		if ( auto layer = Layer( device ) )
		{
			layer->_devFn.GetDeviceQueue( device, queueFamilyIndex, queueIndex, OUT pQueue );

			if ( pQueue and *pQueue )
			{
				Call( layer->_fnTable.GetDeviceQueue, MakeTuple( device, queueFamilyIndex, queueIndex, pQueue ));

				auto&	inst = Instance();
				EXLOCK( inst._lock );
				inst._queueToLayer.insert_or_assign( *pQueue, layer );
			}
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_QueueSubmit
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_QueueSubmit(
		VkQueue                                     queue,
		uint32_t                                    submitCount,
		const VkSubmitInfo*                         pSubmits,
		VkFence                                     fence)
	{
		if ( auto layer = Layer( queue ) )
		{
			VkResult result = layer->_devFn.QueueSubmit( queue, submitCount, pSubmits, fence );

			Call( layer->_fnTable.QueueSubmit, MakeTuple( queue, submitCount, pSubmits, fence ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_QueueWaitIdle
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_QueueWaitIdle(
		VkQueue                                     queue)
	{
		if ( auto layer = Layer( queue ) )
		{
			VkResult result = layer->_devFn.QueueWaitIdle( queue );

			Call( layer->_fnTable.QueueWaitIdle, MakeTuple( queue ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DeviceWaitIdle
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_DeviceWaitIdle(
		VkDevice                                    device)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.DeviceWaitIdle( device );

			Call( layer->_fnTable.DeviceWaitIdle, MakeTuple( device ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_QueueBindSparse
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_QueueBindSparse(
		VkQueue                                     queue,
		uint32_t                                    bindInfoCount,
		const VkBindSparseInfo*                     pBindInfo,
		VkFence                                     fence)
	{
		if ( auto layer = Layer( queue ) )
		{
			VkResult result = layer->_devFn.QueueBindSparse( queue, bindInfoCount, pBindInfo, fence );

			Call( layer->_fnTable.QueueBindSparse, MakeTuple( queue, bindInfoCount, pBindInfo, fence ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_CreateFence
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateFence(
		VkDevice                                    device,
		const VkFenceCreateInfo*                    pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkFence*                                    pFence)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.CreateFence( device, pCreateInfo, pAllocator, OUT pFence );

			Call( layer->_fnTable.CreateFence, MakeTuple( device, pCreateInfo, pAllocator, pFence ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DestroyFence
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroyFence(
		VkDevice                                    device,
		VkFence                                     fence,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroyFence, MakeTuple( device, fence, pAllocator ));

			return layer->_devFn.DestroyFence( device, fence, pAllocator );
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_ResetFences
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_ResetFences(
		VkDevice                                    device,
		uint32_t                                    fenceCount,
		const VkFence*                              pFences)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.ResetFences( device, fenceCount, pFences );

			Call( layer->_fnTable.ResetFences, MakeTuple( device, fenceCount, pFences ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_GetFenceStatus
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_GetFenceStatus(
		VkDevice                                    device,
		VkFence                                     fence)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.GetFenceStatus( device, fence );

			Call( layer->_fnTable.GetFenceStatus, MakeTuple( device, fence ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}

/*
=================================================
	vki_WaitForFences
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_WaitForFences(
		VkDevice                                    device,
		uint32_t                                    fenceCount,
		const VkFence*                              pFences,
		VkBool32                                    waitAll,
		uint64_t                                    timeout)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.WaitForFences( device, fenceCount, pFences, waitAll, timeout );

			Call( layer->_fnTable.WaitForFences, MakeTuple( device, fenceCount, pFences, waitAll, timeout ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_CreateSemaphore
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateSemaphore(
		VkDevice                                    device,
		const VkSemaphoreCreateInfo*                pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkSemaphore*                                pSemaphore)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.CreateSemaphore( device, pCreateInfo, pAllocator, OUT pSemaphore );

			Call( layer->_fnTable.CreateSemaphore, MakeTuple( device, pCreateInfo, pAllocator, pSemaphore ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DestroySemaphore
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroySemaphore(
		VkDevice                                    device,
		VkSemaphore                                 semaphore,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroySemaphore, MakeTuple( device, semaphore, pAllocator ));

			return layer->_devFn.DestroySemaphore( device, semaphore, pAllocator );
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CreateEvent
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateEvent(
		VkDevice                                    device,
		const VkEventCreateInfo*                    pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkEvent*                                    pEvent)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.CreateEvent( device, pCreateInfo, pAllocator, OUT pEvent );

			Call( layer->_fnTable.CreateEvent, MakeTuple( device, pCreateInfo, pAllocator, pEvent ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DestroyEvent
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroyEvent(
		VkDevice                                    device,
		VkEvent                                     event,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroyEvent, MakeTuple( device, event, pAllocator ));

			return layer->_devFn.DestroyEvent( device, event, pAllocator );
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_SetEvent
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_SetEvent(
		VkDevice                                    device,
		VkEvent                                     event)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.SetEvent( device, event );

			Call( layer->_fnTable.SetEvent, MakeTuple( device, event ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_ResetEvent
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_ResetEvent(
		VkDevice                                    device,
		VkEvent                                     event)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.ResetEvent( device, event );

			Call( layer->_fnTable.ResetEvent, MakeTuple( device, event ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_CreateCommandPool
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateCommandPool(
		VkDevice                                    device,
		const VkCommandPoolCreateInfo*              pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkCommandPool*                              pCommandPool)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.CreateCommandPool( device, pCreateInfo, pAllocator, OUT pCommandPool );

			Call( layer->_fnTable.CreateCommandPool, MakeTuple( device, pCreateInfo, pAllocator, pCommandPool ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DestroyCommandPool
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroyCommandPool(
		VkDevice                                    device,
		VkCommandPool                               commandPool,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroyCommandPool, MakeTuple( device, commandPool, pAllocator ));

			return layer->_devFn.DestroyCommandPool( device, commandPool, pAllocator );
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_ResetCommandPool
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_ResetCommandPool(
		VkDevice                                    device,
		VkCommandPool                               commandPool,
		VkCommandPoolResetFlags                     flags)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.ResetCommandPool( device, commandPool, flags );

			Call( layer->_fnTable.ResetCommandPool, MakeTuple( device, commandPool, flags ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_AllocateCommandBuffers
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_AllocateCommandBuffers(
		VkDevice                                    device,
		const VkCommandBufferAllocateInfo*          pAllocateInfo,
		VkCommandBuffer*                            pCommandBuffers)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = layer->_devFn.AllocateCommandBuffers( device, pAllocateInfo, OUT pCommandBuffers );

			if ( result == VK_SUCCESS and pCommandBuffers )
			{
				auto&	inst = Instance();
				EXLOCK( inst._lock );

				for (uint i = 0; i < pAllocateInfo->commandBufferCount; ++i) {
					inst._cmdBufferToLayer.insert_or_assign( pCommandBuffers[i], layer );
				}
			}

			Call( layer->_fnTable.AllocateCommandBuffers, MakeTuple( device, pAllocateInfo, pCommandBuffers ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_FreeCommandBuffers
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_FreeCommandBuffers(
		VkDevice                                    device,
		VkCommandPool                               commandPool,
		uint32_t                                    commandBufferCount,
		const VkCommandBuffer*                      pCommandBuffers)
	{
		if ( auto layer = Layer( device ) )
		{
			layer->_devFn.FreeCommandBuffers( device, commandPool, commandBufferCount, pCommandBuffers );
			
			auto&	inst = Instance();
			EXLOCK( inst._lock );

			for (uint i = 0; i < commandBufferCount; ++i) {
				inst._cmdBufferToLayer.erase( pCommandBuffers[i] );
			}

			Call( layer->_fnTable.FreeCommandBuffers, MakeTuple( device, commandPool, commandBufferCount, pCommandBuffers ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_BeginCommandBuffer
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_BeginCommandBuffer(
		VkCommandBuffer                             commandBuffer,
		const VkCommandBufferBeginInfo*             pBeginInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			VkResult result = layer->_devFn.BeginCommandBuffer( commandBuffer, pBeginInfo );

			Call( layer->_fnTable.BeginCommandBuffer, MakeTuple( commandBuffer, pBeginInfo ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_EndCommandBuffer
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_EndCommandBuffer(
		VkCommandBuffer                             commandBuffer)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			Call( layer->_fnTable.EndCommandBuffer, MakeTuple( commandBuffer ), VK_SUCCESS );	// TODO ?

			return layer->_devFn.EndCommandBuffer( commandBuffer );
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_ResetCommandBuffer
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_ResetCommandBuffer(
		VkCommandBuffer                             commandBuffer,
		VkCommandBufferResetFlags                   flags)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			VkResult result = layer->_devFn.ResetCommandBuffer( commandBuffer, flags );

			Call( layer->_fnTable.ResetCommandBuffer, MakeTuple( commandBuffer, flags ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_CmdSetEvent
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdSetEvent(
		VkCommandBuffer                             commandBuffer,
		VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdSetEvent( commandBuffer, event, stageMask );

			Call( layer->_fnTable.CmdSetEvent, MakeTuple( commandBuffer, event, stageMask ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdResetEvent
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdResetEvent(
		VkCommandBuffer                             commandBuffer,
		VkEvent                                     event,
		VkPipelineStageFlags                        stageMask)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdResetEvent( commandBuffer, event, stageMask );

			Call( layer->_fnTable.CmdResetEvent, MakeTuple( commandBuffer, event, stageMask ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdWaitEvents
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdWaitEvents(
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
		const VkImageMemoryBarrier*                 pImageMemoryBarriers)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdWaitEvents( commandBuffer,
										 eventCount, pEvents,
										 srcStageMask, dstStageMask,
										 memoryBarrierCount, pMemoryBarriers,
										 bufferMemoryBarrierCount, pBufferMemoryBarriers,
										 imageMemoryBarrierCount, pImageMemoryBarriers );

			Call( layer->_fnTable.CmdWaitEvents,
				  MakeTuple( commandBuffer,
							 eventCount, pEvents,
							 srcStageMask, dstStageMask,
							 memoryBarrierCount, pMemoryBarriers,
							 bufferMemoryBarrierCount, pBufferMemoryBarriers,
							 imageMemoryBarrierCount, pImageMemoryBarriers ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdPipelineBarrier
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdPipelineBarrier(
		VkCommandBuffer                             commandBuffer,
		VkPipelineStageFlags                        srcStageMask,
		VkPipelineStageFlags                        dstStageMask,
		VkDependencyFlags                           dependencyFlags,
		uint32_t                                    memoryBarrierCount,
		const VkMemoryBarrier*                      pMemoryBarriers,
		uint32_t                                    bufferMemoryBarrierCount,
		const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
		uint32_t                                    imageMemoryBarrierCount,
		const VkImageMemoryBarrier*                 pImageMemoryBarriers)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdPipelineBarrier( commandBuffer,
											  srcStageMask, dstStageMask, dependencyFlags,
											  memoryBarrierCount, pMemoryBarriers,
											  bufferMemoryBarrierCount, pBufferMemoryBarriers,
											  imageMemoryBarrierCount, pImageMemoryBarriers );

			Call( layer->_fnTable.CmdPipelineBarrier,
				  MakeTuple( commandBuffer,
							 srcStageMask, dstStageMask, dependencyFlags,
							 memoryBarrierCount, pMemoryBarriers,
							 bufferMemoryBarrierCount, pBufferMemoryBarriers,
							 imageMemoryBarrierCount, pImageMemoryBarriers ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdBeginRenderPass
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdBeginRenderPass(
		VkCommandBuffer                             commandBuffer,
		const VkRenderPassBeginInfo*                pRenderPassBegin,
		VkSubpassContents                           contents)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdBeginRenderPass( commandBuffer, pRenderPassBegin, contents );

			Call( layer->_fnTable.CmdBeginRenderPass, MakeTuple( commandBuffer, pRenderPassBegin, contents ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdNextSubpass
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdNextSubpass(
		VkCommandBuffer                             commandBuffer,
		VkSubpassContents                           contents)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdNextSubpass( commandBuffer, contents );

			Call( layer->_fnTable.CmdNextSubpass, MakeTuple( commandBuffer, contents ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdEndRenderPass
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdEndRenderPass(
		VkCommandBuffer                             commandBuffer)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			Call( layer->_fnTable.CmdEndRenderPass, MakeTuple( commandBuffer ));

			return layer->_devFn.CmdEndRenderPass( commandBuffer );
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdExecuteCommands
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdExecuteCommands(
		VkCommandBuffer                             commandBuffer,
		uint32_t                                    commandBufferCount,
		const VkCommandBuffer*                      pCommandBuffers)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			layer->_devFn.CmdExecuteCommands( commandBuffer, commandBufferCount, pCommandBuffers );

			Call( layer->_fnTable.CmdExecuteCommands, MakeTuple( commandBuffer, commandBufferCount, pCommandBuffers ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_TrimCommandPool
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_TrimCommandPool(
		VkDevice                                    device,
		VkCommandPool                               commandPool,
		VkCommandPoolTrimFlags                      flags)
	{
		if ( auto layer = Layer( device ) )
		{
			layer->_devFn.TrimCommandPool( device, commandPool, flags );

			Call( layer->_fnTable.TrimCommandPool, MakeTuple( device, commandPool, flags ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_GetDeviceQueue2
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_GetDeviceQueue2(
		VkDevice                                    device,
		const VkDeviceQueueInfo2*                   pQueueInfo,
		VkQueue*                                    pQueue)
	{
		if ( auto layer = Layer( device ) )
		{
			layer->_devFn.GetDeviceQueue2( device, pQueueInfo, OUT pQueue );

			if ( pQueue and *pQueue )
			{
				auto&	inst = Instance();
				EXLOCK( inst._lock );
				inst._queueToLayer.insert_or_assign( *pQueue, layer );
			}

			Call( layer->_fnTable.GetDeviceQueue2, MakeTuple( device, pQueueInfo, pQueue ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CreateSwapchainKHR
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateSwapchainKHR(
		VkDevice                                    device,
		const VkSwapchainCreateInfoKHR*             pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkSwapchainKHR*                             pSwapchain)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_RESULT_MAX_ENUM;
			
			if ( layer->_devFn.CreateSwapchainKHR )
				result = layer->_devFn.CreateSwapchainKHR( device, pCreateInfo, pAllocator, OUT pSwapchain );

			Call( layer->_fnTable.CreateSwapchainKHR, MakeTuple( device, pCreateInfo, pAllocator, pSwapchain ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DestroySwapchainKHR
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_DestroySwapchainKHR(
		VkDevice                                    device,
		VkSwapchainKHR                              swapchain,
		const VkAllocationCallbacks*                pAllocator)
	{
		if ( auto layer = Layer( device ) )
		{
			Call( layer->_fnTable.DestroySwapchainKHR, MakeTuple( device, swapchain, pAllocator ));

			if ( layer->_devFn.DestroySwapchainKHR )
				layer->_devFn.DestroySwapchainKHR( device, swapchain, pAllocator );

			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_AcquireNextImageKHR
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_AcquireNextImageKHR(
		VkDevice                                    device,
		VkSwapchainKHR                              swapchain,
		uint64_t                                    timeout,
		VkSemaphore                                 semaphore,
		VkFence                                     fence,
		uint32_t*                                   pImageIndex)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_RESULT_MAX_ENUM;
			
			if ( layer->_devFn.AcquireNextImageKHR )
				result = layer->_devFn.AcquireNextImageKHR( device, swapchain, timeout, semaphore, fence, OUT pImageIndex );

			Call( layer->_fnTable.AcquireNextImageKHR,
				  MakeTuple( device, swapchain, timeout, semaphore, fence, pImageIndex ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
		
/*
=================================================
	vki_AcquireNextImage2KHR
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_AcquireNextImage2KHR(
		VkDevice                                    device,
		const VkAcquireNextImageInfoKHR*            pAcquireInfo,
		uint32_t*                                   pImageIndex)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_RESULT_MAX_ENUM;
			
			if ( layer->_devFn.AcquireNextImage2KHR )
				result = layer->_devFn.AcquireNextImage2KHR( device, pAcquireInfo, OUT pImageIndex );

			Call( layer->_fnTable.AcquireNextImage2KHR,
				  MakeTuple( device, pAcquireInfo, pImageIndex ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_QueuePresentKHR
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_QueuePresentKHR(
		VkQueue                                     queue,
		const VkPresentInfoKHR*                     pPresentInfo)
	{
		if ( auto layer = Layer( queue ) )
		{
			VkResult result = VK_RESULT_MAX_ENUM;
			
			if ( layer->_devFn.QueuePresentKHR )
				result = layer->_devFn.QueuePresentKHR( queue, pPresentInfo );

			Call( layer->_fnTable.QueuePresentKHR, MakeTuple( queue, pPresentInfo ), result );
			layer->_Update();

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}

/*
=================================================
	vki_CmdBeginRenderPass2KHR
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdBeginRenderPass2KHR(
		VkCommandBuffer                             commandBuffer,
		const VkRenderPassBeginInfo*                pRenderPassBegin,
		const VkSubpassBeginInfoKHR*                pSubpassBeginInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdBeginRenderPass2KHR )
				layer->_devFn.CmdBeginRenderPass2KHR( commandBuffer, pRenderPassBegin, pSubpassBeginInfo );

			Call( layer->_fnTable.CmdBeginRenderPass2KHR,
				  MakeTuple( commandBuffer, pRenderPassBegin, pSubpassBeginInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdNextSubpass2KHR
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdNextSubpass2KHR(
		VkCommandBuffer                             commandBuffer,
		const VkSubpassBeginInfoKHR*                pSubpassBeginInfo,
		const VkSubpassEndInfoKHR*                  pSubpassEndInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdNextSubpass2KHR )
				layer->_devFn.CmdNextSubpass2KHR( commandBuffer, pSubpassBeginInfo, pSubpassEndInfo );

			Call( layer->_fnTable.CmdNextSubpass2KHR,
				  MakeTuple( commandBuffer, pSubpassBeginInfo, pSubpassEndInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdEndRenderPass2KHR
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdEndRenderPass2KHR(
		VkCommandBuffer                             commandBuffer,
		const VkSubpassEndInfoKHR*                  pSubpassEndInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			Call( layer->_fnTable.CmdEndRenderPass2KHR, MakeTuple( commandBuffer, pSubpassEndInfo ));

			if ( layer->_devFn.CmdEndRenderPass2KHR )
				layer->_devFn.CmdEndRenderPass2KHR( commandBuffer, pSubpassEndInfo );

			return;
		}

		CHECK( false );
	}
		
/*
=================================================
	vki_DebugMarkerSetObjectTagEXT
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_DebugMarkerSetObjectTagEXT(
		VkDevice                                    device,
		const VkDebugMarkerObjectTagInfoEXT*        pTagInfo)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_SUCCESS;

			if ( layer->_devFn.DebugMarkerSetObjectTagEXT )
				result = layer->_devFn.DebugMarkerSetObjectTagEXT( device, pTagInfo );
			
			Call( layer->_fnTable.DebugMarkerSetObjectTagEXT, MakeTuple( device, pTagInfo ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_DebugMarkerSetObjectNameEXT
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_DebugMarkerSetObjectNameEXT(
		VkDevice                                    device,
		const VkDebugMarkerObjectNameInfoEXT*       pNameInfo)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_SUCCESS;

			if ( layer->_devFn.DebugMarkerSetObjectNameEXT )
				result = layer->_devFn.DebugMarkerSetObjectNameEXT( device, pNameInfo );
			
			Call( layer->_fnTable.DebugMarkerSetObjectNameEXT, MakeTuple( device, pNameInfo ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_CmdDebugMarkerBeginEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdDebugMarkerBeginEXT(
		VkCommandBuffer                             commandBuffer,
		const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdDebugMarkerBeginEXT )
				layer->_devFn.CmdDebugMarkerBeginEXT( commandBuffer, pMarkerInfo );

			Call( layer->_fnTable.CmdDebugMarkerBeginEXT, MakeTuple( commandBuffer, pMarkerInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdDebugMarkerEndEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdDebugMarkerEndEXT(
		VkCommandBuffer                             commandBuffer)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			Call( layer->_fnTable.CmdDebugMarkerEndEXT, MakeTuple( commandBuffer ));

			if ( layer->_devFn.CmdDebugMarkerEndEXT )
				layer->_devFn.CmdDebugMarkerEndEXT( commandBuffer );

			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdDebugMarkerInsertEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdDebugMarkerInsertEXT(
		VkCommandBuffer                             commandBuffer,
		const VkDebugMarkerMarkerInfoEXT*           pMarkerInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdDebugMarkerInsertEXT )
				layer->_devFn.CmdDebugMarkerInsertEXT( commandBuffer, pMarkerInfo );

			Call( layer->_fnTable.CmdDebugMarkerInsertEXT, MakeTuple( commandBuffer, pMarkerInfo ));
			return;
		}

		CHECK( false );
	}
		
/*
=================================================
	vki_SetDebugUtilsObjectNameEXT
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_SetDebugUtilsObjectNameEXT(
		VkDevice                                    device,
		const VkDebugUtilsObjectNameInfoEXT*        pNameInfo)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_SUCCESS;
				
			if ( layer->_devFn.SetDebugUtilsObjectNameEXT )
				result = layer->_devFn.SetDebugUtilsObjectNameEXT( device, pNameInfo );

			Call( layer->_fnTable.SetDebugUtilsObjectNameEXT, MakeTuple( device, pNameInfo ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_SetDebugUtilsObjectTagEXT
=================================================
*/
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_SetDebugUtilsObjectTagEXT(
		VkDevice                                    device,
		const VkDebugUtilsObjectTagInfoEXT*         pTagInfo)
	{
		if ( auto layer = Layer( device ) )
		{
			VkResult result = VK_SUCCESS;

			if ( layer->_devFn.SetDebugUtilsObjectTagEXT )
				result = layer->_devFn.SetDebugUtilsObjectTagEXT( device, pTagInfo );

			Call( layer->_fnTable.SetDebugUtilsObjectTagEXT, MakeTuple( device, pTagInfo ), result );

			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
	
/*
=================================================
	vki_QueueBeginDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_QueueBeginDebugUtilsLabelEXT(
		VkQueue                                     queue,
		const VkDebugUtilsLabelEXT*                 pLabelInfo)
	{
		if ( auto layer = Layer( queue ) )
		{
			if ( layer->_devFn.QueueBeginDebugUtilsLabelEXT )
				layer->_devFn.QueueBeginDebugUtilsLabelEXT( queue, pLabelInfo );

			Call( layer->_fnTable.QueueBeginDebugUtilsLabelEXT, MakeTuple( queue, pLabelInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_QueueEndDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_QueueEndDebugUtilsLabelEXT(
		VkQueue                                     queue)
	{
		if ( auto layer = Layer( queue ) )
		{
			Call( layer->_fnTable.QueueEndDebugUtilsLabelEXT, MakeTuple( queue ));

			if ( layer->_devFn.QueueEndDebugUtilsLabelEXT )
				layer->_devFn.QueueEndDebugUtilsLabelEXT( queue );

			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_QueueInsertDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_QueueInsertDebugUtilsLabelEXT(
		VkQueue                                     queue,
		const VkDebugUtilsLabelEXT*                 pLabelInfo)
	{
		if ( auto layer = Layer( queue ) )
		{
			if ( layer->_devFn.QueueInsertDebugUtilsLabelEXT )
				layer->_devFn.QueueInsertDebugUtilsLabelEXT( queue, pLabelInfo );

			Call( layer->_fnTable.QueueInsertDebugUtilsLabelEXT, MakeTuple( queue, pLabelInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdBeginDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdBeginDebugUtilsLabelEXT(
		VkCommandBuffer                             commandBuffer,
		const VkDebugUtilsLabelEXT*                 pLabelInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdBeginDebugUtilsLabelEXT )
				layer->_devFn.CmdBeginDebugUtilsLabelEXT( commandBuffer, pLabelInfo );

			Call( layer->_fnTable.CmdBeginDebugUtilsLabelEXT, MakeTuple( commandBuffer, pLabelInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdEndDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdEndDebugUtilsLabelEXT(
		VkCommandBuffer                             commandBuffer)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			Call( layer->_fnTable.CmdEndDebugUtilsLabelEXT, MakeTuple( commandBuffer ));

			if ( layer->_devFn.CmdEndDebugUtilsLabelEXT )
				layer->_devFn.CmdEndDebugUtilsLabelEXT( commandBuffer );

			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CmdInsertDebugUtilsLabelEXT
=================================================
*/
	VKAPI_ATTR void VKAPI_CALL LayerManager::vki_CmdInsertDebugUtilsLabelEXT(
		VkCommandBuffer                             commandBuffer,
		const VkDebugUtilsLabelEXT*                 pLabelInfo)
	{
		if ( auto layer = Layer( commandBuffer ) )
		{
			if ( layer->_devFn.CmdInsertDebugUtilsLabelEXT )
				layer->_devFn.CmdInsertDebugUtilsLabelEXT( commandBuffer, pLabelInfo );

			Call( layer->_fnTable.CmdInsertDebugUtilsLabelEXT, MakeTuple( commandBuffer, pLabelInfo ));
			return;
		}

		CHECK( false );
	}
	
/*
=================================================
	vki_CreateWin32SurfaceKHR
=================================================
*/
#ifdef VK_USE_PLATFORM_WIN32_KHR
	VKAPI_ATTR VkResult VKAPI_CALL LayerManager::vki_CreateWin32SurfaceKHR(
		VkInstance                                  instance,
		const VkWin32SurfaceCreateInfoKHR*          pCreateInfo,
		const VkAllocationCallbacks*                pAllocator,
		VkSurfaceKHR*                               pSurface)
	{
		if ( auto layer = Layer( instance ) )
		{
			VkResult result = VK_RESULT_MAX_ENUM;

			if ( layer->_instFn.CreateWin32SurfaceKHR )
				result = layer->_instFn.CreateWin32SurfaceKHR( instance, pCreateInfo, pAllocator, pSurface );

			if ( result == VK_SUCCESS )
			{
				layer->_os.hinstance	= pCreateInfo->hinstance;
				layer->_os.hwnd			= pCreateInfo->hwnd;

				auto&	inst = Instance();
				EXLOCK( inst._lock );
				inst._windowToLayer.insert_or_assign( pCreateInfo->hwnd, layer );
			}
			return result;
		}

		CHECK( false );
		return VK_RESULT_MAX_ENUM;
	}
#endif

/*
=================================================
	winapi_GetMsgProc
=================================================
*/
#ifdef VK_USE_PLATFORM_WIN32_KHR
	LRESULT CALLBACK LayerManager::winapi_GetMsgProc(
		_In_ int    code,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam)
	{
		auto*	msg = BitCast< PMSG >( lParam );

		if ( msg->message == WM_KEYDOWN and msg->wParam == VK_F11 )
		{
			if ( auto layer = LayerFromWnd( msg->hwnd ); layer and not layer->IsStarted() )
			{
				layer->_Start( _captureSize );
			}
		}

		return ::CallNextHookEx( null, code, wParam, lParam );
	}
#endif

}	// VSA
