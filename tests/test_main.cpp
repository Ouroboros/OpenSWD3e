#include <cstring>
#include <fstream>
#include <set>
#include <string>

#include "test.h"

namespace swd3etest
{
namespace
{

int failures = 0;

// The cases copied from swd3-rebuild that still check swd32 data (golden results, saves, swd32.exe bytes): listed one
// name per line in tests/pending_swd3e.txt, they are skipped until their module is rebuilt for swd3e.exe.
std::set<std::string> PendingCases()
{
    std::set<std::string> names;
    std::ifstream stream(std::string(SWD3_SOURCE_DIR) + "/tests/pending_swd3e.txt");
    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (!line.empty() && line.front() != '#')
        {
            names.insert(line);
        }
    }

    return names;
}

}  // namespace

std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

void fail(std::string_view file, int line, const std::string& message)
{
    ++failures;
    std::fprintf(stderr, "%.*s:%d: %s\n", static_cast<int>(file.size()), file.data(), line, message.c_str());
}

}  // namespace swd3etest

int main(int argc, char** argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int run = 0;
    int pending = 0;
    const std::set<std::string> pendingNames = swd3etest::PendingCases();

    for (const auto& c : swd3etest::registry())
    {
        if (argc > 1 && c.name.find(argv[1]) == std::string_view::npos)
        {
            continue;
        }

        if (pendingNames.contains(std::string(c.name)))
        {
            ++pending;
            std::printf("[PEND] %.*s\n", static_cast<int>(c.name.size()), c.name.data());
            continue;
        }

        int before = swd3etest::failures;
        c.body();
        ++run;
        std::printf(
            "%s %.*s\n",
            swd3etest::failures == before ? "[ OK ]" : "[FAIL]",
            static_cast<int>(c.name.size()),
            c.name.data()
        );
    }

    std::printf("%d test cases, %d failures, %d pending swd3e\n", run, swd3etest::failures, pending);
    return swd3etest::failures == 0 ? 0 : 1;
}
