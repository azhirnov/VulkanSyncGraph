// Copyright (c) 2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "stl/Platforms/WindowsHeader.h"

# ifdef COMPILER_MSVC
#	pragma warning (push)
#	pragma warning (disable: 4668)
#	include <processthreadsapi.h>
#	pragma warning (pop)

# else
#	include <processthreadsapi.h>
# endif

#include "src/SyncAnalyzer.h"
#include "stl/Algorithms/StringUtils.h"
#include "stl/Containers/ArrayView.h"
#include "stl/Stream/FileStream.h"

#include <filesystem>

namespace VSA
{
	
/*
=================================================
	OnCreateDevice
=================================================
*/
	void SyncAnalyzer::OnCreateDevice (VkInstance inst, VkPhysicalDevice pd, VkDevice ld,
									   PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr)
	{
		EXLOCK( _lock );

		auto	get_queue_family_props = BitCast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>( gipa( inst, "vkGetPhysicalDeviceQueueFamilyProperties" ));
		if ( not get_queue_family_props )
			return;

		auto&	dev		= _devices[ ld ];
		uint	count	= uint(dev.queue_props.size());

		get_queue_family_props( pd, INOUT &count, null );	// to shutup validation warnings

		dev.queue_props.resize( count );
		get_queue_family_props( pd, INOUT &count, OUT dev.queue_props.data() );
	}

/*
=================================================
	Start
=================================================
*/
	void SyncAnalyzer::Start ()
	{
		_enabled	= true;
		_startTime	= TimePoint_t::clock::now();
		_Clear();
	}
	
/*
=================================================
	Stop
=================================================
*/
	void SyncAnalyzer::Stop ()
	{
		_SaveDotFile_v1();
		//_SaveDotFile_v2();
		_Clear();
		_enabled = false;
	}
	
/*
=================================================
	_Clear
=================================================
*/
	void SyncAnalyzer::_Clear ()
	{
		_globalSyncs.clear();
		_signalSemaphores.clear();
		_signalFences.clear();
		_swapchains.clear();
		_uidCounter = 0;
	}

/*
=================================================
	_GetThreadID
=================================================
*/
	SyncAnalyzer::ThreadID  SyncAnalyzer::_GetThreadID ()
	{
		EXLOCK( _lock );

		auto	id		= std::this_thread::get_id();
		auto	iter	= _threadIds.find( id );

		if ( iter != _threadIds.end() )
			return iter->second;
		
		ThreadID	tid		{++_threadIdCounter};
		PWSTR		w_name;
		HRESULT		hr		= ::GetThreadDescription( ::GetCurrentThread(), OUT &w_name );	// Win10 only	// TODO: use dynamic linking

		if ( SUCCEEDED(hr) and w_name and *w_name )
		{
			String	name;
			name.reserve( 128 );

			for (; *w_name; ++w_name) { 
				if ( (*w_name >= 0) & (*w_name < 128) )
					name.push_back( char(*w_name) );
			}

			_threadNames.insert_or_assign( tid, std::move(name) );
		}
		else
		{
			_threadNames.insert_or_assign( tid, "Thread_"s << ToString( uint(tid) ));
		}

		return _threadIds.insert_or_assign( id, tid ).first->second;
	}
	
/*
=================================================
	_GetTimePoint
=================================================
*/
	SyncAnalyzer::TimePoint  SyncAnalyzer::_GetTimePoint ()
	{
		auto	time = TimePoint_t::clock::now();
		return TimePoint{ std::chrono::duration_cast< std::chrono::microseconds >( time - _startTime ).count() };
	}
	
/*
=================================================
	_GetUID
=================================================
*/
	SyncAnalyzer::UID  SyncAnalyzer::_GetUID ()
	{
		return UID{ ++_uidCounter };
	}
	
/*
=================================================
	DefaultQueueName
=================================================
*/
	ND_ static String  DefaultQueueName (const VkQueueFamilyProperties &props, uint queueIndex)
	{
		String	str;

		if ( EnumEq( props.queueFlags, VK_QUEUE_GRAPHICS_BIT ) )
			str = "Graphics_";
		else
		if ( EnumEq( props.queueFlags, VK_QUEUE_COMPUTE_BIT ) )
			str = "Compute_";
		else
		if ( EnumEq( props.queueFlags, VK_QUEUE_TRANSFER_BIT ) )
			str = "Transfer_";
		else
			str = "Queue_";
		
		return str << ToString( queueIndex );
	}

/*
=================================================
	vki_GetDeviceQueue
=================================================
*/
	void SyncAnalyzer::vki_GetDeviceQueue(
		VkDevice                                    device,
		uint32_t                                    queueFamilyIndex,
		uint32_t                                    queueIndex,
		VkQueue*                                    pQueue)
	{
		if ( not pQueue or not *pQueue )
			return;

		EXLOCK( _lock );

		auto&	d = _devices[ device ];
		d.queues.insert( *pQueue );

		auto&	q = _queues[ *pQueue ];
		q.id				= *pQueue;
		q.dev				= device;
		q.queueFamilyIndex	= queueFamilyIndex;
		q.queueIndex		= queueIndex;
		q.name				= DefaultQueueName( d.queue_props[q.queueFamilyIndex], q.queueIndex );
	}

/*
=================================================
	vki_GetDeviceQueue2
=================================================
*/
	void SyncAnalyzer::vki_GetDeviceQueue2(
		VkDevice                                    device,
		const VkDeviceQueueInfo2*                   pQueueInfo,
		VkQueue*                                    pQueue)
	{
		if ( not pQueue or not *pQueue )
			return;

		EXLOCK( _lock );

		auto&	d = _devices[ device ];
		d.queues.insert( *pQueue );
		
		auto&	q = _queues[ *pQueue ];
		q.id				= *pQueue;
		q.dev				= device;
		q.queueFamilyIndex	= pQueueInfo->queueFamilyIndex;
		q.queueIndex		= pQueueInfo->queueIndex;
		q.name				= DefaultQueueName( d.queue_props[q.queueFamilyIndex], q.queueIndex );
	}

/*
=================================================
	vki_QueueSubmit
=================================================
*/
	void SyncAnalyzer::vki_QueueSubmit(
		VkQueue                                     queue,
		uint32_t                                    submitCount,
		const VkSubmitInfo*                         pSubmits,
		VkFence                                     fence,
		VkResult                                    result)
	{
		if ( result != VK_SUCCESS )
			return;

		EXLOCK( _lock );

		if ( not _enabled )
			return;

		const auto	tid		= _GetThreadID();
		const auto	time	= _GetTimePoint();
		const auto	dev		= _queues[queue].dev;
		
		if ( fence )
			_signalFences[ fence ].clear();

		QueueSubmit		cmd_submit;
		cmd_submit.threadId	= tid;
		cmd_submit.time		= time;
		cmd_submit.dev		= dev;
		cmd_submit.uid		= _GetUID();

		FenceSignal		cmd_signal;
		cmd_signal.threadId	= tid;
		cmd_signal.time		= TimePoint( uint(time) + 1);
		cmd_signal.dev		= dev;
		cmd_signal.uid		= _GetUID();
		cmd_signal.queue	= queue;

		for (uint i = 0; i < submitCount; ++i)
		{
			const auto&	submit = pSubmits[i];

			CmdBatch		cmd;
			cmd.threadId	= tid;
			cmd.time		= time;
			cmd.dev			= dev;
			cmd.uid			= _GetUID();
			cmd.queue		= queue;
			cmd.signalSemaphores.assign( submit.pSignalSemaphores, submit.pSignalSemaphores + submit.signalSemaphoreCount );
			cmd.waitSemaphores.assign( submit.pWaitSemaphores, submit.pWaitSemaphores + submit.waitSemaphoreCount );

			for (uint j = 0; j < submit.waitSemaphoreCount; ++j)
			{
				auto	iter = _signalSemaphores.find( submit.pWaitSemaphores[j] );
				if ( iter != _signalSemaphores.end() )
				{
					cmd.waitDeps.push_back( iter->second );
					_signalSemaphores.erase( iter );
				}
			}

			for (uint j = 0; j < submit.signalSemaphoreCount; ++j) {
				_signalSemaphores[ submit.pSignalSemaphores[j] ] = cmd.uid;
			}
			
			cmd_submit.batches.push_back( cmd.uid );
			cmd_signal.dependsOn.push_back( cmd.uid );

			_globalSyncs.push_back( cmd );
		}

		_globalSyncs.push_back( cmd_submit );
		
		if ( fence )
		{
			_globalSyncs.push_back( cmd_signal );
			_signalFences[ fence ] = { cmd_signal.uid };
		}
	}
	
/*
=================================================
	vki_QueueWaitIdle
=================================================
*/
	void SyncAnalyzer::vki_QueueWaitIdle(
		VkQueue                                     queue,
		VkResult                                    result)
	{
		if ( result != VK_SUCCESS )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		QueueWaitIdle	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= _queues[queue].dev;
		cmd.uid			= _GetUID();
		cmd.queue		= queue;
		
		_globalSyncs.push_back( cmd );
	}
	
/*
=================================================
	vki_DeviceWaitIdle
=================================================
*/
	void SyncAnalyzer::vki_DeviceWaitIdle(
		VkDevice                                    device,
		VkResult                                    result)
	{
		if ( result != VK_SUCCESS )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		DeviceWaitIdle	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= device;
		cmd.uid			= _GetUID();
		
		_globalSyncs.push_back( cmd );
	}
	
/*
=================================================
	vki_QueueBindSparse
=================================================
*/
	void SyncAnalyzer::vki_QueueBindSparse(
		VkQueue                                     queue,
		uint32_t                                    bindInfoCount,
		const VkBindSparseInfo*                     pBindInfo,
		VkFence                                     fence,
		VkResult                                    result)
	{
		/*if ( result != VK_SUCCESS )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		const auto	tid		= _GetThreadID();
		const auto	time	= _GetTimePoint();
		const auto	dev		= _queues[queue].dev;
		
		if ( fence )
			_signalFences[ fence ].clear();

		for (uint i = 0; i < bindInfoCount; ++i)
		{
			const auto&	submit = pBindInfo[i];

			BindSparse		cmd;
			cmd.threadId	= tid;
			cmd.time		= time;
			cmd.dev			= dev;
			cmd.uid			= _GetUID();
			cmd.queue		= queue;
			cmd.fence		= fence;
			cmd.signalSemaphores.assign( submit.pSignalSemaphores, submit.pSignalSemaphores + submit.signalSemaphoreCount );
			cmd.waitSemaphores.assign( submit.pWaitSemaphores, submit.pWaitSemaphores + submit.waitSemaphoreCount );
			
			for (uint j = 0; j < submit.waitSemaphoreCount; ++j)
			{
				auto	iter = _signalSemaphores.find( submit.pWaitSemaphores[j] );
				ASSERT( iter != _signalSemaphores.end() );

				if ( iter != _signalSemaphores.end() )
				{
					cmd.waitDeps.push_back( iter->second );
					_signalSemaphores.erase( iter );
				}
			}

			for (uint j = 0; j < submit.signalSemaphoreCount; ++j) {
				_signalSemaphores[ submit.pSignalSemaphores[j] ] = cmd.uid;
			}

			if ( fence )
				_signalFences[ fence ].push_back( cmd.uid );

			_globalSyncs.push_back( cmd );
		}*/
	}
	
/*
=================================================
	vki_ResetFences
=================================================
*/
	void SyncAnalyzer::vki_ResetFences(
		VkDevice                                    ,
		uint32_t                                    fenceCount,
		const VkFence*                              pFences,
		VkResult                                    result)
	{
		if ( result != VK_SUCCESS )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		for (uint i = 0; i < fenceCount; ++i) {
			_signalFences.erase( pFences[i] );
		}
	}
	
/*
=================================================
	vki_GetFenceStatus
=================================================
*/
	void SyncAnalyzer::vki_GetFenceStatus(
		VkDevice                                    device,
		VkFence                                     fence,
		VkResult                                    result)
	{
		if ( not (result == VK_SUCCESS or result == VK_TIMEOUT) )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		WaitForFences	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= device;
		cmd.uid			= _GetUID();
		cmd.waitForAll	= true;
		cmd.timeout		= result == VK_TIMEOUT;
		cmd.fences.push_back( fence );
		
		{
			auto& arr = _signalFences[ fence ];
			cmd.fenceDeps.insert( cmd.fenceDeps.end(), arr.begin(), arr.end() );
		}

		_globalSyncs.push_back( cmd );
	}

/*
=================================================
	vki_WaitForFences
=================================================
*/
	void SyncAnalyzer::vki_WaitForFences(
		VkDevice                                    device,
		uint32_t                                    fenceCount,
		const VkFence*                              pFences,
		VkBool32                                    waitAll,
		uint64_t                                    ,
		VkResult                                    result)
	{
		if ( not (result == VK_SUCCESS or result == VK_TIMEOUT) )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		WaitForFences	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= device;
		cmd.uid			= _GetUID();
		cmd.waitForAll	= waitAll;
		cmd.timeout		= result == VK_TIMEOUT;
		cmd.fences.assign( pFences, pFences + fenceCount );
		
		for (uint i = 0; i < fenceCount; ++i)
		{
			auto& arr = _signalFences[ pFences[i] ];
			cmd.fenceDeps.insert( cmd.fenceDeps.end(), arr.begin(), arr.end() );
		}

		_globalSyncs.push_back( cmd );
	}
		
/*
=================================================
	vki_AcquireNextImageKHR
=================================================
*/
	void SyncAnalyzer::vki_AcquireNextImageKHR(
		VkDevice                                    device,
		VkSwapchainKHR                              swapchain,
		uint64_t                                    ,
		VkSemaphore                                 semaphore,
		VkFence                                     fence,
		uint32_t*                                   pImageIndex,
		VkResult                                    result)
	{
		if ( not (result == VK_SUCCESS or result == VK_TIMEOUT) )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		AcquireImage	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= device;
		cmd.uid			= _GetUID();
		cmd.swapchain	= swapchain;
		cmd.sem			= semaphore;
		cmd.fence		= fence;

		if ( semaphore )
			_signalSemaphores[ semaphore ] = cmd.uid;

		if ( fence )
			_signalFences[ fence ].push_back( cmd.uid );

		{
			auto&	sw = _swapchains[ swapchain ];
			sw.resize( Max( sw.size(), *pImageIndex + 1 ));
			sw[ *pImageIndex ] = cmd.uid;
		}

		_globalSyncs.push_back( cmd );
	}
		
/*
=================================================
	vki_AcquireNextImage2KHR
=================================================
*/
	void SyncAnalyzer::vki_AcquireNextImage2KHR(
		VkDevice                                    device,
		const VkAcquireNextImageInfoKHR*            pAcquireInfo,
		uint32_t*                                   pImageIndex,
		VkResult                                    result)
	{
		if ( not (result == VK_SUCCESS or result == VK_TIMEOUT) )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		AcquireImage	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= device;
		cmd.uid			= _GetUID();
		cmd.swapchain	= pAcquireInfo->swapchain;
		cmd.sem			= pAcquireInfo->semaphore;
		cmd.fence		= pAcquireInfo->fence;
		
		if ( pAcquireInfo->semaphore )
			_signalSemaphores[ pAcquireInfo->semaphore ] = cmd.uid;

		if ( pAcquireInfo->fence )
			_signalFences[ pAcquireInfo->fence ].push_back( cmd.uid );
		
		{
			auto&	sw = _swapchains[ pAcquireInfo->swapchain ];
			sw.resize( Max( sw.size(), *pImageIndex + 1 ));
			sw[ *pImageIndex ] = cmd.uid;
		}

		_globalSyncs.push_back( cmd );
	}
	
/*
=================================================
	vki_QueuePresentKHR
=================================================
*/
	void SyncAnalyzer::vki_QueuePresentKHR(
		VkQueue                                     queue,
		const VkPresentInfoKHR*                     pPresentInfo,
		VkResult                                    result)
	{
		if ( not (result == VK_SUCCESS or result == VK_SUBOPTIMAL_KHR) )
			return;

		EXLOCK( _lock );
		
		if ( not _enabled )
			return;

		QueuePresent	cmd;
		cmd.threadId	= _GetThreadID();
		cmd.time		= _GetTimePoint();
		cmd.dev			= _queues[queue].dev;
		cmd.uid			= _GetUID();
		cmd.queue		= queue;
		cmd.waitSemaphores.assign( pPresentInfo->pWaitSemaphores, pPresentInfo->pWaitSemaphores + pPresentInfo->waitSemaphoreCount );
		
		for (uint j = 0; j < pPresentInfo->waitSemaphoreCount; ++j)
		{
			auto	iter = _signalSemaphores.find( pPresentInfo->pWaitSemaphores[j] );
			ASSERT( iter != _signalSemaphores.end() );

			if ( iter != _signalSemaphores.end() )
			{
				cmd.waitDeps.push_back( iter->second );
				_signalSemaphores.erase( iter );
			}
		}

		for (uint i = 0; i < pPresentInfo->swapchainCount; ++i)
		{
			auto	iter = _swapchains.find( pPresentInfo->pSwapchains[i] );
			ASSERT( iter != _swapchains.end() );

			if ( iter != _swapchains.end() and pPresentInfo->pImageIndices[i] < iter->second.size() )
			{
				cmd.swapchains.emplace_back( pPresentInfo->pSwapchains[i], iter->second[ pPresentInfo->pImageIndices[i] ] );
			}
		}

		_globalSyncs.push_back( cmd );
	}

/*
=================================================
	vki_DebugMarkerSetObjectNameEXT
=================================================
*/
	void SyncAnalyzer::vki_DebugMarkerSetObjectNameEXT(
		VkDevice                                    ,
		const VkDebugMarkerObjectNameInfoEXT*       pNameInfo,
		VkResult                                    )
	{
		if ( pNameInfo and pNameInfo->objectType == VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT )
		{
			_queues[ VkQueue(pNameInfo->object) ].name = pNameInfo->pObjectName ? String(pNameInfo->pObjectName) : "";
		}
	}
	
/*
=================================================
	vki_SetDebugUtilsObjectNameEXT
=================================================
*/
	void SyncAnalyzer::vki_SetDebugUtilsObjectNameEXT(
		VkDevice                                    ,
		const VkDebugUtilsObjectNameInfoEXT*        pNameInfo,
		VkResult                                    )
	{
		if ( pNameInfo and pNameInfo->objectType == VK_OBJECT_TYPE_QUEUE )
		{
			_queues[ VkQueue(pNameInfo->objectHandle) ].name = pNameInfo->pObjectName ? String(pNameInfo->pObjectName) : "";
		}
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	ColToStr
=================================================
*/
	ND_ inline String  ColToStr (RGBA8u col)
	{
		uint	val = (uint(col.r) << 16) | (uint(col.g) << 8) | uint(col.b);
		String	str = ToString<16>( val );

		for (; str.length() < 6;) {
			str.insert( str.begin(), '0' );
		}
		return str;
	}
	
/*
=================================================
	_QueueName
=================================================
*/
	String  SyncAnalyzer::_QueueName (VkQueue q) const
	{
		auto	iter = _queues.find( q );
		
		if ( iter != _queues.end() )
			return iter->second.name;
		
		return ToString( uint64_t(q) );
	}
	
/*
=================================================
	_ThreadName
=================================================
*/
	String  SyncAnalyzer::_ThreadName (ThreadID tid) const
	{
		auto	iter = _threadNames.find( tid );
		
		if ( iter != _threadNames.end() )
			return iter->second;

		return ToString( uint(tid) );
	}
//-----------------------------------------------------------------------------


/*
=================================================
	V1
=================================================
*/
	String  SyncAnalyzer::V1::_ToCpuNodeName (UID id)
	{
		return "cn_"s << ToString( uint(id) );
	}

	String  SyncAnalyzer::V1::_ToGpuNodeName (UID id)
	{
		return "gn_"s << ToString( uint(id) );
	}
	
	String  SyncAnalyzer::V1::_ToThreadNodeName (ThreadID id)
	{
		return "tid_"s << ToString( uint(id) );
	}
	
	String  SyncAnalyzer::V1::_ToQueueNodeName (VkQueue id)
	{
		return "queue_"s << ToString( uint64_t(id) );
	}

	String  SyncAnalyzer::V1::_ToNodeStyle (StringView name, const NodeStyle &style)
	{
		return String()
			<< " [label=\"" << name << '"'
			<< ", fontcolor=\"#" << ColToStr( style.labelColor ) << '"'
			<< (style.fontSize ? ", fontsize="s << ToString( style.fontSize ) : "")
			<< ", fillcolor=\"#" << ColToStr( style.bgColor ) << '"'
			<< ", style=filled];\n";
	}
	
	String  SyncAnalyzer::V1::_AcquirePresentNodeStyle (StringView name)
	{
		NodeStyle	style;
		style.bgColor		= HtmlColor::Lime;
		style.labelColor	= HtmlColor::Black;
		return V1::_ToNodeStyle( name, style );
	}

	String  SyncAnalyzer::V1::_WaitOnHostNodeStyle (StringView name)
	{
		NodeStyle	style;
		style.bgColor		= HtmlColor::Red;
		style.labelColor	= HtmlColor::White;
		return V1::_ToNodeStyle( name, style );
	}

	String  SyncAnalyzer::V1::_SubmitNodeStyle (StringView name)
	{
		NodeStyle	style;
		style.bgColor		= HtmlColor::Blue;
		style.labelColor	= HtmlColor::White;
		return V1::_ToNodeStyle( name, style );
	}

	String  SyncAnalyzer::V1::_CmdBatchNodeStyle (StringView name)
	{
		NodeStyle	style;
		style.bgColor		= HtmlColor::DarkSlateGray;
		style.labelColor	= HtmlColor::Gainsboro;
		return V1::_ToNodeStyle( name, style );
	}

	String  SyncAnalyzer::V1::_FenceNodeStyle (StringView name)
	{
		NodeStyle	style;
		style.bgColor		= HtmlColor::Gold;
		style.labelColor	= HtmlColor::Black;
		style.fontSize		= 8;
		return String()
			<< " [label=\"" << name << '"'
			<< ", fontcolor=\"#" << ColToStr( style.labelColor ) << '"'
			<< (style.fontSize ? ", fontsize="s << ToString( style.fontSize ) : "")
			<< ", fillcolor=\"#" << ColToStr( style.bgColor ) << '"'
			<< ", style=filled"
			<< ", margin=0, nojustify=true];\n";
	}

	String  SyncAnalyzer::V1::_MakeSemaphoreEdge (UID from, UID to)
	{
		return V1::_ToGpuNodeName( from ) << ":e -> " << V1::_ToGpuNodeName( to ) << ":w [color=\"#" << ColToStr( HtmlColor::Orange ) << "\"];\n";
	}
	
	String  SyncAnalyzer::V1::_MakeSwapchainEdge (UID from, UID to)
	{
		return V1::_ToGpuNodeName( from ) << ":e -> " << V1::_ToGpuNodeName( to ) << ":w [color=\"#" << ColToStr( HtmlColor::Lime ) << "\"];\n";
	}

	String  SyncAnalyzer::V1::_MakeCpuToGpuSyncEdge (UID fromCpu, UID toGpu)
	{
		return V1::_ToCpuNodeName( fromCpu ) << " -> " << V1::_ToGpuNodeName( toGpu ) << " [color=\"#" << ColToStr( HtmlColor::DeepSkyBlue ) << "\", minlen=2];\n";
	}
	
	String  SyncAnalyzer::V1::_MakeGpuToCpuSyncEdge (UID fromGpu, UID toCpu)
	{
		return V1::_ToGpuNodeName( fromGpu ) << " -> " << V1::_ToCpuNodeName( toCpu ) << " [color=\"#" << ColToStr( HtmlColor::Red ) << "\", minlen=2];\n";
	}
	
	String  SyncAnalyzer::V1::_MakeStrongHiddenEdge (StringView from, StringView to)
	{
		return String(from) << " -> " << to << " [minlen=0, style=invis];\n";
	}

/*
=================================================
	_SaveDotFile_v1
=================================================
*/
	bool SyncAnalyzer::_SaveDotFile_v1 () const
	{
		String	str;
		str << "digraph SyncAnalyzer {\n"
			<< "	rankdir = LR;\n"
			<< "	bgcolor = black;\n"
			<< "	compound=true;\n\n";

		String	rank_decl;
		String	rank;
		String	deps;
		String	rank_names;

		rank_decl << "\"init\" -> ";
		auto	add_rank = [&rank_decl, &rank, last = TimePoint{~0u}] (TimePoint time) mutable
		{
			if ( last != time ) {
				rank_decl << '"' << ToString( uint(time) ) << "\" -> ";
				rank << (rank.empty() ? "" : "\t}\n")
						<< "\t{\n"
						<< "\t	rank = same; \"" << ToString( uint(time) ) << "\";\n";
				last = time;
			}
		};

		HashMap<ThreadID, UID>	active_threads;
		auto	make_cpu_timeline = [&active_threads] (UID id, ThreadID tid)
		{
			String	str;
			auto	iter = active_threads.find( tid );
			if ( iter != active_threads.end() ) {
				str << V1::_ToCpuNodeName( iter->second ) << ":e -> " << V1::_ToCpuNodeName( id ) << ":w";
			} else {
				str << V1::_ToThreadNodeName( tid ) << ":e -> " << V1::_ToCpuNodeName( id ) << ":w";
			}
			active_threads.insert_or_assign( tid, id );
			return str << " [color=\"#" << ColToStr( HtmlColor::DeepSkyBlue ) << "\", style=dotted, penwidth=2];\n";
		};

		HashMap<VkQueue, UID>	active_queues;
		auto	make_gpu_timeline = [&active_queues] (UID id, VkQueue queue)
		{
			String	str;
			auto	iter = active_queues.find( queue );
			if ( iter != active_queues.end() ) {
				str << V1::_ToGpuNodeName( iter->second ) << ":e -> " << V1::_ToGpuNodeName( id ) << ":w";
			} else {
				str << V1::_ToQueueNodeName( queue ) << ":e -> " << V1::_ToGpuNodeName( id ) << ":w";
			}
			active_queues.insert_or_assign( queue, id );
			return str << " [color=\"#" << ColToStr( HtmlColor::DarkGreen ) << "\", style=dotted, penwidth=2];\n";
		};
			
		for (auto& sync : _globalSyncs)
		{
			Visit( sync,
				[&] (const QueueSubmit& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_SubmitNodeStyle( "Submit" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );

					for (auto& batch : sync.batches) {
						deps << "\t" << V1::_MakeCpuToGpuSyncEdge( sync.uid, batch );
					}
				},

				[&] (const CmdBatch& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToGpuNodeName( sync.uid ) << V1::_CmdBatchNodeStyle( "CmdBatch" );
					deps<< "\t" << make_gpu_timeline( sync.uid, sync.queue );

					for (auto& sem : sync.waitDeps) {
						deps << "\t" << V1::_MakeSemaphoreEdge( sem, sync.uid );
					}
				},

				[&] (const FenceSignal& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToGpuNodeName( sync.uid ) << V1::_FenceNodeStyle( "Fence" );

					for (auto& batch : sync.dependsOn) {
						deps << "\t" << V1::_MakeSemaphoreEdge( batch, sync.uid );
					}
				},

				[&] (const QueueWaitIdle& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_WaitOnHostNodeStyle( "Wait" )
						<< "\t\t" << V1::_ToGpuNodeName( sync.uid ) << V1::_WaitOnHostNodeStyle( "Wait" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << make_gpu_timeline( sync.uid, sync.queue )
						<< "\t" << V1::_MakeGpuToCpuSyncEdge( sync.uid, sync.uid );
				},

				[&] (const DeviceWaitIdle& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_WaitOnHostNodeStyle( "Wait" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );
					// TODO: insert node to all queues
				},

				[&] (const WaitForFences& sync) {
					if ( sync.timeout )
						return;

					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_WaitOnHostNodeStyle( "Wait" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );

					for (auto& fence : sync.fenceDeps) {
						deps << "\t" << V1::_MakeGpuToCpuSyncEdge( fence, sync.uid );
					}
				},

				[&] (const AcquireImage& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_AcquirePresentNodeStyle( "Acquire" )
						<< "\t\t" << V1::_ToGpuNodeName( sync.uid ) << V1::_AcquirePresentNodeStyle( "Acquire" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << V1::_MakeCpuToGpuSyncEdge( sync.uid, sync.uid );
				},

				[&] (const QueuePresent& sync) {
					add_rank( sync.time );
					rank<< "\t\t" << V1::_ToCpuNodeName( sync.uid ) << V1::_AcquirePresentNodeStyle( "Present" )
						<< "\t\t" << V1::_ToGpuNodeName( sync.uid ) << V1::_AcquirePresentNodeStyle( "Present" );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << make_gpu_timeline( sync.uid, sync.queue )
						<< "\t" << V1::_MakeCpuToGpuSyncEdge( sync.uid, sync.uid );

					for (auto&[sw, acq] : sync.swapchains) {
						deps << "\t" << V1::_MakeSwapchainEdge( acq, sync.uid );
					}
					for (auto& sem : sync.waitDeps) {
						deps << "\t" << V1::_MakeSemaphoreEdge( sem, sync.uid );
					}
				}
			);
		}

		{
			String	prev_thread;
			for (auto& item : active_threads)
			{
				NodeStyle	style;
				style.bgColor		= HtmlColor::Indigo;
				style.labelColor	= HtmlColor::White;

				String		curr = V1::_ToThreadNodeName( item.first );
				rank_names << "\t\t" << curr << V1::_ToNodeStyle( _ThreadName( item.first ), style );

				if ( prev_thread.size() )
					deps << V1::_MakeStrongHiddenEdge( prev_thread, curr );

				prev_thread = std::move(curr);
			}
		}{
			String	prev_queue;
			for (auto& item : active_queues)
			{
				NodeStyle	style;
				style.bgColor		= HtmlColor::DarkSlateGray;
				style.labelColor	= HtmlColor::Gainsboro;
			
				String		curr = V1::_ToQueueNodeName( item.first );
				rank_names << "\t\t" << curr << V1::_ToNodeStyle( _QueueName( item.first ), style );
			
				if ( prev_queue.size() )
					deps << V1::_MakeStrongHiddenEdge( prev_queue, curr );

				prev_queue = std::move(curr);
			}
		}

		if ( EndsWith( rank_decl, " -> " ) )
			rank_decl.erase( rank_decl.size() - 4, 4 );

		str	<< "	{\n"
			<< "		node [shape=plaintext, fontname=\"helvetica\", fontsize=5, fontcolor=white];\n"
			<< "		" << rank_decl << ";\n"
			<< "	}\n\n"
			<< "	{\n"
			<< "		rank = same; \"init\";\n"
			<< "		node [shape=rectangle, fontname=\"helvetica\", penwidth=0.0];\n"
			<< "		edge [fontname=\"helvetica\", fontcolor=white, minlen=1];\n"
			<<			rank_names
			<< "	}\n\n"
			<< "	node [shape=rectangle, fontname=\"helvetica\", penwidth=0.0];\n"
			<< "	edge [fontname=\"helvetica\", fontcolor=white, minlen=1];\n"
			<<		rank
			<< "	}\n\n"
			<<		deps
			<< "}\n";

		return _Visualize( str, "C:\\Projects\\sync_graph_v1.dot", "png" );
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	Execute
=================================================
*/
	static bool Execute (StringView commands, uint timeoutMS)
	{
#	ifdef PLATFORM_WINDOWS
		char	buf[MAX_PATH] = {};
		::GetSystemDirectoryA( buf, UINT(CountOf(buf)) );
		
		String		command_line;
		command_line << '"' << buf << "\\cmd.exe\" /C " << commands;

		STARTUPINFOA			startup_info = {};
		PROCESS_INFORMATION		proc_info	 = {};
		
		bool process_created = ::CreateProcessA(
			NULL,
			command_line.data(),
			NULL,
			NULL,
			FALSE,
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			OUT &startup_info,
			OUT &proc_info
		);

		if ( not process_created )
			return false;

		if ( ::WaitForSingleObject( proc_info.hThread, timeoutMS ) != WAIT_OBJECT_0 )
			return false;
		
		DWORD process_exit;
		::GetExitCodeProcess( proc_info.hProcess, OUT &process_exit );

		return true;
#	else
		return false;
#	endif
	}

/*
=================================================
	_Visualize
=================================================
*/
	bool SyncAnalyzer::_Visualize (StringView graph, StringView path, StringView format) const
	{
		namespace FS = std::filesystem;

		const FS::path	filepath	{ path };
		const FS::path	graph_path	= FS::path{filepath}.concat( "."s << format );

		CHECK_ERR( filepath.extension() == ".dot" );
		FS::create_directories( filepath.parent_path() );


		// space in path is not supported
		CHECK_ERR( path.find( ' ' ) == String::npos );

		// save to '.dot' file
		{
			FileWStream		wfile{ path };
			CHECK_ERR( wfile.IsOpen() );
			CHECK_ERR( wfile.Write( graph ));

			VSA_LOGI( String(VSA_LAYER_NAME) << ": Dot-file saved into '"s << path << "'" );
		}

		// generate graph
		{
			// delete previous version
			if ( FS::exists( graph_path ) )
			{
				CHECK( FS::remove( graph_path ));
				std::this_thread::sleep_for( std::chrono::milliseconds(1) );
			}
			
			CHECK_ERR( Execute( "\""s << VSA_GRAPHVIZ_DOT_EXECUTABLE << "\" -T" << format << " -O " << path, 30'000 ));
			
			VSA_LOGI( String(VSA_LAYER_NAME) << ": Graph saved into '"s << graph_path.string() << "'" );
		}

		return true;
	}

}	// VSA
