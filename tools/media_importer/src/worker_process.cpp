#include "worker_process.hpp"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace cuexis::media_importer {
namespace {

[[nodiscard]] auto workerError(std::string code, std::string message) -> core::Error {
    return core::Error{std::move(code), std::move(message)};
}

#if defined(_WIN32)

[[nodiscard]] auto quoteArgument(const std::string& argument) -> std::wstring {
    std::wstring wide;
    wide.reserve(argument.size() + 2);
    wide.push_back(L'"');
    for (const char character : argument) {
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(character)));
    }
    wide.push_back(L'"');
    return wide;
}

auto runBoundedWorkerWindows(const WorkerRequest& request) -> core::Result<WorkerOutcome> {
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job == nullptr) {
        return core::unexpected(
            workerError("media.worker.unavailable", "A worker job object could not be created"));
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    limits.ProcessMemoryLimit = static_cast<SIZE_T>(request.memoryLimitBytes);
    if (SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ==
        0) {
        CloseHandle(job);
        return core::unexpected(workerError("media.worker.unavailable",
                                            "The worker memory limit could not be applied"));
    }

    std::wstring commandLine = quoteArgument(request.executable.string());
    for (const auto& argument : request.arguments) {
        commandLine.push_back(L' ');
        commandLine += quoteArgument(argument);
    }

    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (CreatePipe(&readEnd, &writeEnd, &attributes, 0) == 0) {
        CloseHandle(job);
        return core::unexpected(
            workerError("media.worker.unavailable", "A worker output pipe could not be created"));
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');
    const BOOL created = CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, TRUE,
                                        CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    CloseHandle(writeEnd);
    if (created == 0) {
        CloseHandle(readEnd);
        CloseHandle(job);
        return core::unexpected(
            workerError("media.worker.unavailable", "The worker process could not be created"));
    }

    if (AssignProcessToJobObject(job, process.hProcess) == 0) {
        TerminateProcess(process.hProcess, 1);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(readEnd);
        CloseHandle(job);
        return core::unexpected(workerError("media.worker.unavailable",
                                            "The worker process could not be assigned to its job"));
    }
    ResumeThread(process.hThread);

    std::string output;
    char chunk[4096];
    DWORD read = 0;
    while (ReadFile(readEnd, chunk, sizeof(chunk), &read, nullptr) != 0 && read > 0) {
        output.append(chunk, read);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(readEnd);
    CloseHandle(job);

    WorkerOutcome outcome;
    outcome.started = true;
    outcome.exitCode = static_cast<int>(exitCode);
    outcome.output = std::move(output);
    return outcome;
}

#else

auto runBoundedWorkerPosix(const WorkerRequest& request) -> core::Result<WorkerOutcome> {
    int pipeEnds[2] = {-1, -1};
    if (::pipe(pipeEnds) != 0) {
        return core::unexpected(
            workerError("media.worker.unavailable", "A worker output pipe could not be created"));
    }
    const pid_t child = ::fork();
    if (child < 0) {
        ::close(pipeEnds[0]);
        ::close(pipeEnds[1]);
        return core::unexpected(
            workerError("media.worker.unavailable", "The worker process could not be created"));
    }
    if (child == 0) {
        ::close(pipeEnds[0]);
        ::dup2(pipeEnds[1], STDOUT_FILENO);
        ::dup2(pipeEnds[1], STDERR_FILENO);
        ::close(pipeEnds[1]);
        // RLIMIT_AS caps the whole address space, which a sanitizer runtime cannot work inside:
        // see workerAddressSpaceCapSupported. The sanitize presets therefore run without the cap
        // and the CLI gate only asserts cap enforcement where the cap is actually applied.
        if (workerAddressSpaceCapSupported) {
            rlimit limit{};
            limit.rlim_cur = static_cast<rlim_t>(request.memoryLimitBytes);
            limit.rlim_max = static_cast<rlim_t>(request.memoryLimitBytes);
            if (::setrlimit(RLIMIT_AS, &limit) != 0) {
                ::_exit(120);
            }
        }
        std::vector<char*> arguments;
        arguments.reserve(request.arguments.size() + 2);
        arguments.push_back(const_cast<char*>(request.executable.c_str()));
        for (const auto& argument : request.arguments) {
            arguments.push_back(const_cast<char*>(argument.c_str()));
        }
        arguments.push_back(nullptr);
        ::execv(request.executable.c_str(), arguments.data());
        ::_exit(121);
    }

    ::close(pipeEnds[1]);
    std::string output;
    char chunk[4096];
    ssize_t read = 0;
    while ((read = ::read(pipeEnds[0], chunk, sizeof(chunk))) > 0) {
        output.append(chunk, static_cast<std::size_t>(read));
    }
    ::close(pipeEnds[0]);

    int status = 0;
    if (::waitpid(child, &status, 0) < 0) {
        return core::unexpected(
            workerError("media.worker.unavailable", "The worker process could not be reaped"));
    }
    WorkerOutcome outcome;
    outcome.started = true;
    outcome.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -WTERMSIG(status);
    outcome.output = std::move(output);
    return outcome;
}

#endif

} // namespace

auto runBoundedWorker(const WorkerRequest& request) -> core::Result<WorkerOutcome> {
    if (request.memoryLimitBytes == 0) {
        return core::unexpected(
            workerError("media.worker.unavailable", "A worker memory limit is required"));
    }
#if defined(_WIN32)
    return runBoundedWorkerWindows(request);
#else
    return runBoundedWorkerPosix(request);
#endif
}

} // namespace cuexis::media_importer
