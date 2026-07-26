#pragma once

#include <cuexis/content/content_provider.hpp>

namespace cuexis::playback {

// Compatibility aliases for the pre-1E include path. New code should include
// cuexis/content/content_provider.hpp and use cuexis::content directly.
using ContentRequest = content::ContentRequest;
using ContentBlob = content::ContentBlob;
using IContentProvider = content::IContentProvider;
using FilesystemContentRoot = content::FilesystemContentRoot;
using FilesystemContentProvider = content::FilesystemContentProvider;
using MemoryContentEntry = content::MemoryContentEntry;
using MemoryContentProvider = content::MemoryContentProvider;
using HostContentCallback = content::HostContentCallback;
using HostContentProvider = content::HostContentProvider;

} // namespace cuexis::playback
