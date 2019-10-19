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
#include "stl/Containers/ArrayView.h"
#include "stl/Algorithms/StringUtils.h"
#include "stl/Stream/FileStream.h"

#include <filesystem>

namespace VSA
{
	
/*
=================================================
	Start
=================================================
*/
	void SyncAnalyzer::Start ()
	{
		_enabled	= true;
		_startTime	= TimePoint_t::clock::now();
	}
	
/*
=================================================
	Stop
=================================================
*/
	void SyncAnalyzer::Stop ()
	{
		_SaveDotFile();
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

		if ( SUCCEEDED(hr) )
		{
			String	name;
			name.reserve( 128 );

			for (; *w_name; ++w_name) { 
				if ( (*w_name >= 0) & (*w_name < 128) )
					name.push_back( char(*w_name) );
			}

			_threadNames.insert_or_assign( tid, std::move(name) );
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
	vki_GetDeviceQueue
=================================================
*/
	void SyncAnalyzer::vki_GetDeviceQueue(
		VkDevice                                    device,
		uint32_t                                    queueFamilyIndex,
		uint32_t                                    queueIndex,
		VkQueue*                                    pQueue)
	{
		EXLOCK( _lock );

		auto&	d = _devices[ device ];
		d.queues.insert( *pQueue );

		auto&	q = _queues[ *pQueue ];
		q.id				= *pQueue;
		q.dev				= device;
		q.queueFamilyIndex	= queueFamilyIndex;
		q.queueIndex		= queueIndex;
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
		EXLOCK( _lock );

		auto&	d = _devices[ device ];
		d.queues.insert( *pQueue );
		
		auto&	q = _queues[ *pQueue ];
		q.id				= *pQueue;
		q.dev				= device;
		q.queueFamilyIndex	= pQueueInfo->queueFamilyIndex;
		q.queueIndex		= pQueueInfo->queueIndex;
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
		VkDevice                                    device,
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
	vki_WaitForFences
=================================================
*/
	void SyncAnalyzer::vki_WaitForFences(
		VkDevice                                    device,
		uint32_t                                    fenceCount,
		const VkFence*                              pFences,
		VkBool32                                    waitAll,
		uint64_t                                    timeout,
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
			ASSERT( arr.size() );
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
		uint64_t                                    timeout,
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
		cmd.swapchains.assign( pPresentInfo->pSwapchains, pPresentInfo->pSwapchains + pPresentInfo->swapchainCount );
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
	
=================================================
*/
	String  SyncAnalyzer::_ToCpuNodeName (UID id)
	{
		return "cn_"s << ToString( uint(id) );
	}

	String  SyncAnalyzer::_ToGpuNodeName (UID id)
	{
		return "gn_"s << ToString( uint(id) );
	}
	
	String  SyncAnalyzer::_ToNodeStyle (StringView name, const NodeStyle &style)
	{
		return String()
			<< " [label=\"" << name << '"'
			<< ", fontcolor=\"#" << ColToStr( style.labelColor ) << '"'
			<< (style.fontSize ? ", fontsize="s << ToString( style.fontSize ) : "")
			<< ", fillcolor=\"#" << ColToStr( style.bgColor ) << '"'
			<< ", style=filled];\n";
	}
	
	String  SyncAnalyzer::_MakeSemaphoreEdge (UID from, UID to)
	{
		return _ToGpuNodeName( from ) << ":e -> " << _ToGpuNodeName( to ) << ":w [color=\"#" << ColToStr( HtmlColor::Orange ) << "\"];\n";
	}

	String  SyncAnalyzer::_MakeCpuToGpuSyncEdge (UID fromCpu, UID toGpu)
	{
		return _ToCpuNodeName( fromCpu ) << " -> " << _ToGpuNodeName( toGpu ) << " [color=\"#" << ColToStr( HtmlColor::Blue ) << "\"];\n";
	}
	
	String  SyncAnalyzer::_MakeGpuToCpuSyncEdge (UID fromGpu, UID toCpu)
	{
		return _ToGpuNodeName( fromGpu ) << " -> " << _ToCpuNodeName( toCpu ) << " [color=\"#" << ColToStr( HtmlColor::Red ) << "\"];\n";
	}
	
/*
=================================================
	
=================================================
*/
	String  SyncAnalyzer::_QueueName (VkQueue q) const
	{
		auto	iter = _queues.find( q );
		
		if ( iter != _queues.end() )
			return iter->second.name;
		
		return ToString( uint64_t(q) );
	}

	String  SyncAnalyzer::_ThreadName (ThreadID tid) const
	{
		auto	iter = _threadNames.find( tid );
		
		if ( iter != _threadNames.end() )
			return iter->second;

		return ToString( uint(tid) );
	}

/*
=================================================
	_SaveDotFile
=================================================
*/
	bool SyncAnalyzer::_SaveDotFile () const
	{
		String	str;
		str << "digraph SyncAnalyzer {\n"
			<< "	rankdir = LR;\n"
			<< "	bgcolor = black;\n"
			//<< "	concentrate=true;\n"
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
				str << _ToCpuNodeName( iter->second ) << ":e -> " << _ToCpuNodeName( id ) << ":w";
			} else {
				str << "tid_" << ToString( uint(tid) ) << ":e -> " << _ToCpuNodeName( id ) << ":w";
			}
			active_threads.insert_or_assign( tid, id );
			return str << " [color=\"#d3d3d3\", style=dotted];\n";
		};

		HashMap<VkQueue, UID>	active_queues;
		auto	make_gpu_timeline = [&active_queues] (UID id, VkQueue queue)
		{
			String	str;
			auto	iter = active_queues.find( queue );
			if ( iter != active_queues.end() ) {
				str << _ToGpuNodeName( iter->second ) << ":e -> " << _ToGpuNodeName( id ) << ":w";
			} else {
				str << "queue_" << ToString( uint64_t(queue) ) << ":e -> " << _ToGpuNodeName( id ) << ":w";
			}
			active_queues.insert_or_assign( queue, id );
			return str << " [color=\"#d3d3d3\", style=dotted];\n";
		};
			
		for (auto& sync : _globalSyncs)
		{
			Visit( sync,
				[&] (const QueueSubmit& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Blue;
					style.labelColor	= HtmlColor::White;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Submit", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );

					for (auto& batch : sync.batches) {
						deps << "\t" << _MakeCpuToGpuSyncEdge( sync.uid, batch );
					}
				},

				[&] (const CmdBatch& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::DarkSlateGray;
					style.labelColor	= HtmlColor::Gainsboro;

					add_rank( sync.time );
					rank<< "\t\t" << _ToGpuNodeName( sync.uid ) << _ToNodeStyle( "CmdBatch", style );
					deps<< "\t" << make_gpu_timeline( sync.uid, sync.queue );

					for (auto& sem : sync.waitDeps) {
						deps << "\t" << _MakeSemaphoreEdge( sem, sync.uid );
					}
				},

				[&] (const FenceSignal& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Gold;
					style.labelColor	= HtmlColor::Black;

					add_rank( sync.time );
					rank<< "\t\t" << _ToGpuNodeName( sync.uid ) << _ToNodeStyle( "Fence", style );
					deps<< "\t" << make_gpu_timeline( sync.uid, sync.queue );

					for (auto& batch : sync.dependsOn) {
						deps << "\t" << _MakeSemaphoreEdge( batch, sync.uid );
					}
				},

				[&] (const QueueWaitIdle& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Red;
					style.labelColor	= HtmlColor::White;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Wait", style )
						<< "\t\t" << _ToGpuNodeName( sync.uid ) << _ToNodeStyle( "Wait", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << make_gpu_timeline( sync.uid, sync.queue )
						<< "\t" << _MakeGpuToCpuSyncEdge( sync.uid, sync.uid );
				},

				[&] (const DeviceWaitIdle& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Red;
					style.labelColor	= HtmlColor::White;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Wait", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );
					// TODO: insert node to all queues
				},

				[&] (const WaitForFences& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Red;
					style.labelColor	= HtmlColor::White;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Wait", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId );

					for (auto& fence : sync.fenceDeps) {
						deps << "\t" << _MakeGpuToCpuSyncEdge( fence, sync.uid );
					}
				},

				[&] (const AcquireImage& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Lime;
					style.labelColor	= HtmlColor::Black;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Acquire", style )
						<< "\t\t" << _ToGpuNodeName( sync.uid ) << _ToNodeStyle( "Acquire", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << _MakeCpuToGpuSyncEdge( sync.uid, sync.uid );
				},

				[&] (const QueuePresent& sync) {
					NodeStyle	style;
					style.bgColor		= HtmlColor::Lime;
					style.labelColor	= HtmlColor::Black;

					add_rank( sync.time );
					rank<< "\t\t" << _ToCpuNodeName( sync.uid ) << _ToNodeStyle( "Present", style )
						<< "\t\t" << _ToGpuNodeName( sync.uid ) << _ToNodeStyle( "Present", style );
					deps<< "\t" << make_cpu_timeline( sync.uid, sync.threadId )
						<< "\t" << make_gpu_timeline( sync.uid, sync.queue )
						<< "\t" << _MakeCpuToGpuSyncEdge( sync.uid, sync.uid );

					for (auto& sem : sync.waitDeps) {
						deps << "\t" << _MakeSemaphoreEdge( sem, sync.uid );
					}
				}
			);
		}

		for (auto& item : active_threads)
		{
			NodeStyle	style;
			style.bgColor		= HtmlColor::Indigo;
			style.labelColor	= HtmlColor::White;

			rank_names	<< "\t\ttid_" << ToString( uint(item.first) )
						<< _ToNodeStyle( _ThreadName( item.first ), style );
		}

		for (auto& item : active_queues)
		{
			NodeStyle	style;
			style.bgColor		= HtmlColor::DarkSlateGray;
			style.labelColor	= HtmlColor::Gainsboro;

			rank_names	<< "\t\tqueue_" << ToString( uint64_t(item.first) )
						<< _ToNodeStyle( _QueueName( item.first ), style );
		}

		if ( EndsWith( rank_decl, " -> " ) )
			rank_decl.erase( rank_decl.size() - 4, 4 );

		str	<< "\t{\n"
			<< "\t\tnode [shape=plaintext, fontname=\"helvetica\", fontsize=5, fontcolor=white];\n\t\t"
			<< rank_decl << ";\n"
			<< "\t}\n\n"
			<< "\t{\n"
			<< "\t\trank = same; \"init\";\n"
			<< "\t\tnode [shape=rectangle, fontname=\"helvetica\", penwidth=0.0];\n"
			<< "\t\tedge [fontname=\"helvetica\", fontcolor=white, minlen=2];\n"
			<< rank_names
			<< "\t}\n\n"
			<< "\tnode [shape=rectangle, fontname=\"helvetica\", penwidth=0.0];\n"
			<< "\tedge [fontname=\"helvetica\", fontcolor=white, minlen=2];\n"
			<< rank
			<< "\t}\n\n"
			<< deps
			<< "}\n";

		return _Visualize( str, "C:\\Projects\\sync_graph.dot", "png" );
	}
	
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

		//std::this_thread::sleep_for( std::chrono::milliseconds(1) );
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
		}

		return true;
	}

}	// VSA
