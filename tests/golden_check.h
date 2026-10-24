#pragma once

#include <functional>
#include <map>
#include <string>

#include "core/types.h"

namespace swd3etest
{

// The golden hashes of a results file: a scenario name (which may hold spaces), a space and its hash on each line.
std::map<std::string, std::string> LoadGolden(const std::string& path);

// Compares the hashes of the scenarios run with the golden ones (a report callback of the golden runners), printing the
// first mismatches.
class GoldenCheck
{
public:
    explicit GoldenCheck(const std::map<std::string, std::string>& golden);
    // A check that leaves out the scenarios `excluded` accepts (counted in `excluded`).
    GoldenCheck(const std::map<std::string, std::string>& golden, std::function<bool(const std::string&)> excludes);

    void operator()(const std::string& name, swd3e::u64 hash);

    int compared = 0;
    int mismatches = 0;
    int excluded = 0;

private:
    const std::map<std::string, std::string>& golden;
    std::function<bool(const std::string&)> excludes;
};

}  // namespace swd3etest
