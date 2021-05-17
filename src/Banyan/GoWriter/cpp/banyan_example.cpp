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

    std::vector<banyan_go::net> nets;
    std::vector<banyan_go::pin> pins;
    std::vector<banyan_go::box> boxes;
    std::vector<banyan_go::layer> layers;
    std::vector<banyan_go::track> tracks;

    nets.push_back(banyan_go::net{"NET_1", 0, 2});
    pins.push_back(banyan_go::pin{"PIN_1", 0, 10});
    pins.push_back(banyan_go::pin{"PIN_2", 10, 20});

    for (int i = 0; i < 20; ++i)
        boxes.push_back(banyan_go::box{0, i, i*i, i*i*i, i*i*i*i});

    layers.push_back(banyan_go::layer{100, 5, 0, 1});
    tracks.push_back(banyan_go::track{0, 10, 0, 10000});

    banyan_go::entry_ret ret;

    printf("%lld\n", banyan(
        banyan_go::to_slice(nets),
        banyan_go::to_slice(pins),
        banyan_go::to_slice(boxes),
        banyan_go::to_slice(layers),
        banyan_go::to_slice(tracks),
        &ret));

    for (size_t i = 0; i < nets.size(); ++i)
    {
        printf("net[%zu]=rbox[%lld,%lld)\n",
            i, nets[i].rbox_begin, nets[i].rbox_end);
    }

    for (size_t i = 0; i < ret.rboxes_size; ++i)
    {
        printf("rbox[%zu]=(%d,%d,%d,%d,%d)\n",
            i, ret.rboxes[i].la,
            ret.rboxes[i].x, ret.rboxes[i].y,
            ret.rboxes[i].w, ret.rboxes[i].h);
    }

    free(ret.rboxes);

    return 0;
}
