#include "rsz/LPResizer.hh"

namespace rsz {

void LPResolver::makeVar(Sdc* sdc, Vertex* vertex)
{
    
};
void genPIConstraints();
void genPOConstraints();
void genWireConstraints();
const char[MAX_BUF_LEN] getVarName(Vertex* vertex, const char* fmt, ...);
__attribute__((format(printf, 3, 4)));

const char[MAX_BUF_LEN] getWireName(Vertex* pred,
                                    Vertex* succ,
                                    const char* fmt,
                                    ...);
__attribute__((format(printf, 4, 5)));

LPResizer::LPResizer() : Resizer()
{
}

}  // namespace rsz
