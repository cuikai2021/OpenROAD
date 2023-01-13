#include "testa.hpp"
#include "prettyprint.hpp"
#include <string>
#include <typeinfo>

using namespace std;

namespace {

void easy_test(const string&)
{
    string trial = pp::prettyPrint("easy_test");
    TESTA_ASSERT(trial == "easy_test")
        (trial)
        .issue();
}

} // namespace
TESTA_DEF_JUNIT_LIKE1(easy_test);
