#pragma once

#if defined(_WIN32)
#include <Windows.h>
#include <iostream>

namespace pd_measure {
inline void environmentJson(const char* label) {
    PROCESS_POWER_THROTTLING_STATE process{PROCESS_POWER_THROTTLING_CURRENT_VERSION, 0, 0};
    const auto processOk = GetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling,
                                                 &process, sizeof(process));
    const auto processError = processOk ? 0UL : GetLastError();
    THREAD_POWER_THROTTLING_STATE thread{THREAD_POWER_THROTTLING_CURRENT_VERSION, 0, 0};
    const auto threadOk =
        GetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &thread, sizeof(thread));
    const auto threadError = threadOk ? 0UL : GetLastError();
    SIZE_T processAffinity = 0, systemAffinity = 0;
    const auto affinityOk =
        GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity);
    const auto affinityError = affinityOk ? 0UL : GetLastError();
    GROUP_AFFINITY groupAffinity{};
    const auto groupOk = GetThreadGroupAffinity(GetCurrentThread(), &groupAffinity);
    const auto groupError = groupOk ? 0UL : GetLastError();
    std::cout << "PD_ENV {\"label\":\"" << label << "\",\"pid\":" << GetCurrentProcessId()
              << ",\"tid\":" << GetCurrentThreadId()
              << ",\"process_affinity_error\":" << affinityError
              << ",\"process_affinity\":" << processAffinity
              << ",\"system_affinity\":" << systemAffinity
              << ",\"processor_group_error\":" << groupError
              << ",\"processor_group\":" << groupAffinity.Group
              << ",\"processor_number\":" << GetCurrentProcessorNumber()
              << ",\"process_priority\":" << GetPriorityClass(GetCurrentProcess())
              << ",\"thread_priority\":" << GetThreadPriority(GetCurrentThread())
              << ",\"process_power_error\":" << processError
              << ",\"process_power_control\":" << process.ControlMask
              << ",\"process_power_state\":" << process.StateMask
              << ",\"thread_power_error\":" << threadError
              << ",\"thread_power_control\":" << thread.ControlMask
              << ",\"thread_power_state\":" << thread.StateMask << "}\n";
}
} // namespace pd_measure
#endif
