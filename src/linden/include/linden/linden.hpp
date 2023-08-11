#pragma once
#include "ord/OpenRoad.hh"
#include <deque>
#include <string>

namespace ord {
class OpenRoad;
}

namespace linden {

void initLindenDp(::ord::OpenRoad*);
void deleteLindenDp(::ord::OpenRoad*);

class DetailedPlacer
{
public:
    explicit DetailedPlacer(::ord::OpenRoad& ord);
    ~DetailedPlacer() =default;

    int go(std::deque<std::string>&);

private:
    ::ord::OpenRoad& openroad_;
};

class IncrLegalizer
{
public:
    explicit IncrLegalizer(::ord::OpenRoad& ord);
    ~IncrLegalizer() =default;

    int go(std::deque<std::string>&);

private:
    ::ord::OpenRoad& openroad_;
};

} // namespace linden
