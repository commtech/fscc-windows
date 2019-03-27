/*
    Copyright (C) 2018  Commtech, Inc.

    This file is part of fscc-windows.

    fscc-windows is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    fscc-windows is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details.

    You should have received a copy of the GNU General Public License along
    with fscc-windows.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <evntrace.h> // For TRACE_LEVEL definitions

#if !defined(EVENT_TRACING)

//
// TODO: These defines are missing in evntrace.h
// in some DDK build environments (XP).
//
#if !defined(TRACE_LEVEL_NONE)
	#define TRACE_LEVEL_NONE        0
	#define TRACE_LEVEL_CRITICAL    1
	#define TRACE_LEVEL_FATAL       1
	#define TRACE_LEVEL_ERROR       2
	#define TRACE_LEVEL_WARNING     3
	#define TRACE_LEVEL_INFORMATION 4
	#define TRACE_LEVEL_VERBOSE     5
	#define TRACE_LEVEL_RESERVED6   6
	#define TRACE_LEVEL_RESERVED7   7
	#define TRACE_LEVEL_RESERVED8   8
	#define TRACE_LEVEL_RESERVED9   9
#endif

//
// Define Debug Flags
//
#define TRACE_DRIVER                0x00000001
#define TRACE_DEBUG                 0x00000002
#define TRACE_QUEUE					0x00000004


VOID
TraceEvents(
_In_ ULONG   DebugPrintLevel,
_In_ ULONG   DebugPrintFlag,
_Printf_format_string_
_In_ PCSTR   DebugMessage,
...
);

#define WPP_INIT_TRACING(DriverObject, RegistryPath)
#define WPP_CLEANUP(DriverObject)

#else

#define WPP_CHECK_FOR_NULL_STRING  //to prevent exceptions due to NULL strings

//
// Define the tracing flags.
//
// Tracing GUID - be83efb0-76a2-4b9c-adfb-1238d1f8c60a
//


#define WPP_CONTROL_GUIDS                                              \
    WPP_DEFINE_CONTROL_GUID(                                           \
        FSCCTraceGuid, (be83efb0,76a2,4b9c,adfb,1238d1f8c60a), \
                                                                            \
        WPP_DEFINE_BIT(MYDRIVER_ALL_INFO)                              \
        WPP_DEFINE_BIT(TRACE_DRIVER)                                   \
        WPP_DEFINE_BIT(TRACE_DEVICE)                                   \
        WPP_DEFINE_BIT(TRACE_QUEUE)                                    \
        )

#define WPP_FLAG_LEVEL_LOGGER(flag, level)                                  \
    WPP_LEVEL_LOGGER(flag)

#define WPP_FLAG_LEVEL_ENABLED(flag, level)                                 \
    (WPP_LEVEL_ENABLED(flag) &&                                             \
     WPP_CONTROL(WPP_BIT_ ## flag).Level >= level)

#define WPP_LEVEL_FLAGS_LOGGER(lvl,flags) \
           WPP_LEVEL_LOGGER(flags)

#define WPP_LEVEL_FLAGS_ENABLED(lvl, flags) \
           (WPP_LEVEL_ENABLED(flags) && WPP_CONTROL(WPP_BIT_ ## flags).Level >= lvl)

#endif
//
// This comment block is scanned by the trace preprocessor to define our
// Trace function.
//
// begin_wpp config
// FUNC Trace{FLAG=MYDRIVER_ALL_INFO}(LEVEL, MSG, ...);
// FUNC TraceEvents(LEVEL, FLAGS, MSG, ...);
// end_wpp
//