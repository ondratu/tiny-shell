#include <unistd.h>

#include <climits>

#include "wm/process.h"

namespace tiny::wm {

std::string local_host_name()
{
    char name[256] = {}; // HOST_NAME_MAX is not everywhere
    if (gethostname(name, sizeof(name) - 1) != 0) {
        return "";
    }
    return name;
}

pid_t killable_pid(unsigned long pid, const std::string& machine,
                   const std::string& host)
{
    if (pid <= 1 || pid > static_cast<unsigned long>(INT_MAX) ||
        static_cast<pid_t>(pid) == getpid()) {
        return 0;
    }
    return !machine.empty() && machine == host ? static_cast<pid_t>(pid) : 0;
}

} // namespace tiny::wm
