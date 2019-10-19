// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Defines.h"

namespace VSA
{
	

	//
	// Logger
	//

	struct Logger
	{
		enum class EResult {
			Continue,
			Break,
			Abort,
		};
		
		static EResult  Info (const char *msg, const char *func, const char *file, int line);
		static EResult  Info (std::string_view msg, std::string_view func, std::string_view file, int line);

		static EResult  Error (const char *msg, const char *func, const char *file, int line);
		static EResult  Error (std::string_view msg, std::string_view func, std::string_view file, int line);
	};

}	// VSA


#define VSA_PRIVATE_LOGX( _level_, _msg_, _file_, _line_ ) \
	BEGIN_ENUM_CHECKS() \
	{switch ( ::VSA::Logger::_level_( (_msg_), (VSA_FUNCTION_NAME), (_file_), (_line_) ) ) \
	{ \
		case ::VSA::Logger::EResult::Continue :	break; \
		case ::VSA::Logger::EResult::Break :	VSA_PRIVATE_BREAK_POINT();	break; \
		case ::VSA::Logger::EResult::Abort :	VSA_PRIVATE_EXIT();			break; \
	}} \
	END_ENUM_CHECKS()

#define VSA_PRIVATE_LOGI( _msg_, _file_, _line_ )	VSA_PRIVATE_LOGX( Info, (_msg_), (_file_), (_line_) )
#define VSA_PRIVATE_LOGE( _msg_, _file_, _line_ )	VSA_PRIVATE_LOGX( Error, (_msg_), (_file_), (_line_) )
