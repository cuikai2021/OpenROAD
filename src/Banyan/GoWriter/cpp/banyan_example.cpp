#include <stdio.h>
#include <dlfcn.h>

#include "../../src/banyan_go.h"

int main()
{
    void *so = dlopen("./banyan.so", RTLD_NOW);
    if (so == nullptr)
    {
        printf("cannot load banyan.so\n");
        return -1;
    }

    auto banyan = (banyan_go::entry_func)dlsym(so, "banyan");
    if (banyan == nullptr)
    {
        printf("cannot find banyan\n");
        return -1;
    }

    std::vector<banyan_go::box> boxes;
    for (int i = 0; i < 100; ++i)
        boxes.push_back(banyan_go::box{i, i*i, i*i*i, i*i*i*i});

    printf("%lld\n", banyan(banyan_go::to_slice(boxes)));

    return 0;
}
