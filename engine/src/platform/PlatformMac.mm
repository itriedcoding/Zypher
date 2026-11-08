#include "zypher/core/Platform.h"

#include "zypher/core/FileSystem.h"
#include "zypher/core/Log.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <dispatch/dispatch.h>
#include <mach-o/dyld.h>
#include <sys/sysctl.h>
#include <unistd.h>

namespace zy {

std::string macosExecutableDirectory() {
    @autoreleasepool {
        NSString* executable = [[NSBundle mainBundle] executablePath];
        if (executable == nil) {
            uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);
            std::string buffer(size, '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
                buffer.resize(size > 0 ? size - 1 : 0);
                NSString* path = [NSString stringWithUTF8String:buffer.c_str()];
                executable = path;
            }
        }
        if (executable == nil) {
            return FileSystem::currentWorkingDirectory();
        }
        return FileSystem::parentPath(executable.UTF8String);
    }
}

} // namespace zy

namespace zy::platform {

std::string_view name() { return "macOS"; }

std::string_view architecture() {
#if defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__)
    return "x86_64";
#else
    return "unknown";
#endif
}

std::string osVersion() {
    @autoreleasepool {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        return "macOS " + std::to_string(version.majorVersion) + "." + std::to_string(version.minorVersion) +
               "." + std::to_string(version.patchVersion);
    }
}

bool isDebuggerAttached() {
    // A process being traced reports a non-zero P_TRACED flag.
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
    kinfo_proc information{};
    size_t size = sizeof(information);
    if (sysctl(mib, 4, &information, &size, nullptr, 0) != 0) {
        return false;
    }
    return (information.kp_proc.p_flag & P_TRACED) != 0;
}

void debugBreak() { __builtin_trap(); }

void showErrorMessage(std::string_view title, std::string_view message) {
    @autoreleasepool {
        // NSAlert must be shown from the main thread; queue it when called from
        // a worker thread (the caller is about to abort anyway).
        const auto show = [title = std::string(title), message = std::string(message)] {
            NSAlert* alert = [[NSAlert alloc] init];
            alert.messageText = [NSString stringWithUTF8String:title.c_str()];
            alert.informativeText = [NSString stringWithUTF8String:message.c_str()];
            alert.alertStyle = NSAlertStyleCritical;
            [alert addButtonWithTitle:@"Quit"];
            [alert runModal];
        };
        if ([NSThread isMainThread]) {
            show();
        } else {
            dispatch_sync(dispatch_get_main_queue(), ^{
                show();
            });
        }
    }
}

} // namespace zy::platform
