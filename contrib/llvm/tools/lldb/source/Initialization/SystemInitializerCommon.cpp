//===-- SystemInitializerCommon.cpp -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Initialization/SystemInitializerCommon.h"

#include "lldb/Host/Host.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Timer.h"
#include "Plugins/Instruction/ARM/EmulateInstructionARM.h"
#include "Plugins/Instruction/MIPS/EmulateInstructionMIPS.h"
#include "Plugins/Instruction/MIPS64/EmulateInstructionMIPS64.h"
#include "Plugins/ObjectContainer/BSD-Archive/ObjectContainerBSDArchive.h"
#include "Plugins/ObjectFile/ELF/ObjectFileELF.h"
#include "Plugins/Process/gdb-remote/ProcessGDBRemoteLog.h"

#if defined(__APPLE__)
#include "Plugins/ObjectFile/Mach-O/ObjectFileMachO.h"
#endif

#if defined(__linux__)
#include "Plugins/Process/POSIX/ProcessPOSIXLog.h"
#endif

#if defined(_MSC_VER)
#include "Plugins/Process/Windows/Common/ProcessWindowsLog.h"
#include "lldb/Host/windows/windows.h"
#endif

#include "llvm/Support/TargetSelect.h"

#include <string>

using namespace lldb_private;

static void
fatal_error_handler(void *user_data, const std::string &reason, bool gen_crash_diag)
{
    Host::SetCrashDescription(reason.c_str());
    ::abort();
}

SystemInitializerCommon::SystemInitializerCommon()
{
}

SystemInitializerCommon::~SystemInitializerCommon()
{
}

void
SystemInitializerCommon::Initialize()
{
#if defined(_MSC_VER)
    const char *disable_crash_dialog_var = getenv("LLDB_DISABLE_CRASH_DIALOG");
    if (disable_crash_dialog_var && llvm::StringRef(disable_crash_dialog_var).equals_lower("true"))
    {
        // This will prevent Windows from displaying a dialog box requiring user interaction when
        // LLDB crashes.  This is mostly useful when automating LLDB, for example via the test
        // suite, so that a crash in LLDB does not prevent completion of the test suite.
        ::SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    }
#endif

    Log::Initialize();
    HostInfo::Initialize();
    Timer scoped_timer(__PRETTY_FUNCTION__, __PRETTY_FUNCTION__);

    llvm::install_fatal_error_handler(fatal_error_handler, 0);

    process_gdb_remote::ProcessGDBRemoteLog::Initialize();

    // Initialize plug-ins
    ObjectContainerBSDArchive::Initialize();
    ObjectFileELF::Initialize();

    EmulateInstructionARM::Initialize();
    EmulateInstructionMIPS::Initialize();
    EmulateInstructionMIPS64::Initialize();

    //----------------------------------------------------------------------
    // Apple/Darwin hosted plugins
    //----------------------------------------------------------------------

#if defined(__APPLE__)
    ObjectFileMachO::Initialize();
#endif
#if defined(__linux__)
    static ConstString g_linux_log_name("linux");
    ProcessPOSIXLog::Initialize(g_linux_log_name);
#endif
#if defined(_MSC_VER)
    ProcessWindowsLog::Initialize();
#endif
}

void
SystemInitializerCommon::Terminate()
{
    Timer scoped_timer(__PRETTY_FUNCTION__, __PRETTY_FUNCTION__);
    ObjectContainerBSDArchive::Terminate();
    ObjectFileELF::Terminate();

    EmulateInstructionARM::Terminate();
    EmulateInstructionMIPS::Terminate();
    EmulateInstructionMIPS64::Terminate();

    ObjectContainerUniversalMachO::Terminate();
#if defined(__APPLE__)
    ObjectFileMachO::Terminate();
#endif

#if defined(_MSC_VER)
    ProcessWindowsLog::Terminate();
#endif

    HostInfo::Terminate();
    Log::Terminate();
}
