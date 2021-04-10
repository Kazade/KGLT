#include <SDL.h>

#include <unistd.h>

#include "platform.h"
#include "../../application.h"
#include "../../window.h"

namespace smlt {

Resolution LinuxPlatform::native_resolution() const {
    SDL_DisplayMode mode;

    Resolution native;
    if(SDL_GetDesktopDisplayMode(0, &mode) == -1) {
        S_WARN("Unable to get the current desktop display mode!!");
        S_WARN("{0}", SDL_GetError());
        S_WARN("Falling back to 1080p");
        native.width = 1920;
        native.height = 1080;
        native.refresh_rate = 60;
    } else {
        native.width = mode.w;
        native.height = mode.h;
        native.refresh_rate = mode.refresh_rate;
    }
    return native;
}

uint64_t LinuxPlatform::available_ram_in_bytes() const {
    auto lines = get_app()->window->vfs->read_file_lines("/proc/meminfo");
    for(auto& line: lines) {
        if(line.find("MemFree:") != std::string::npos) {
            auto prefix = line.substr(0, line.find_last_of(" "));
            auto suffix = prefix.substr(prefix.find_last_of(" ") + 1);
            return smlt::stol(suffix) * 1024;
        }
    }

    return MEMORY_VALUE_UNAVAILABLE;
}

uint64_t LinuxPlatform::total_ram_in_bytes() const {
    auto lines = get_app()->window->vfs->read_file_lines("/proc/meminfo");
    for(auto& line: lines) {
        if(line.find("MemTotal:") != std::string::npos) {
            auto prefix = line.substr(0, line.find_last_of(" "));
            auto suffix = prefix.substr(prefix.find_last_of(" ") + 1);
            uint64_t kb = std::stol(suffix);
            return kb * 1024;
        }
    }

    return MEMORY_VALUE_UNAVAILABLE;
}

uint64_t LinuxPlatform::process_ram_usage_in_bytes(ProcessID process_id) const {
    _S_UNUSED(process_id);

    int tSize = 0, resident = 0, share = 0;
    std::ifstream buffer("/proc/self/statm");
    buffer >> tSize >> resident >> share;
    buffer.close();

    long page_size = sysconf(_SC_PAGESIZE); // in case x86-64 is configured to use 2MB pages
    return resident * page_size;
}

}
