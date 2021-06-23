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
    std::vector<banyan_go::via> vias;
    std::vector<banyan_go::layer> layers;
    std::vector<banyan_go::viadef> viadefs;
    std::vector<banyan_go::track> tracks;

    nets.push_back(banyan_go::net{"NET_1", 0, 2});
    pins.push_back(banyan_go::pin{"PIN_1", 0, 10, 0, 5});
    pins.push_back(banyan_go::pin{"PIN_2", 10, 20, 5, 10});

    for (int i = 0; i < 20; ++i)
        boxes.push_back(banyan_go::box{0, i, i*i, i*i*i, i*i*i*i});
    for (int i = 0; i < 10; ++i)
        vias.push_back(banyan_go::via{i%2, i, i*i});

    layers.push_back(banyan_go::layer{100, 5, 0, 1, 0, 0});
    layers.push_back(banyan_go::layer{100, 5, 1, 2, 0, 1});
    layers.push_back(banyan_go::layer{100, 5, 2, 3, 1, 2});
    viadefs.push_back(banyan_go::viadef{"VIA_1",
        {1, 0, 0, 100, 100}, {0, 0, 0, 200, 200}});
    viadefs.push_back(banyan_go::viadef{"VIA_2",
        {2, 0, 0, 300, 300}, {1, 0, 0, 400, 400}});
    tracks.push_back(banyan_go::track{0, 10, 0, 10000});
    tracks.push_back(banyan_go::track{1, 100, 0, 10000});
    tracks.push_back(banyan_go::track{2, 1000, 0, 10000});

    banyan_go::entry_ret ret;

    printf("banyan returns %lld\n", banyan(
        banyan_go::to_slice(nets),
        banyan_go::to_slice(pins),
        banyan_go::to_slice(boxes),
        banyan_go::to_slice(vias),
        banyan_go::to_slice(layers),
        banyan_go::to_slice(viadefs),
        banyan_go::to_slice(tracks),
        &ret));

    for (size_t i = 0; i < nets.size(); ++i)
    {
        if ((nets[i].rbox_begin == nets[i].rbox_end)
            && (nets[i].rvia_begin == nets[i].rvia_end)) {
            continue;
        }
        printf("net[%zu]=rbox[%zu,%zu), rvia[%zu,%zu)\n",
            i, nets[i].rbox_begin, nets[i].rbox_end,
            nets[i].rvia_begin, nets[i].rvia_end);
    }

    for (size_t i = 0; i < ret.rboxes_size; ++i)
    {
        printf("rbox[%zu]=(%d,%d,%d,%d,%d)\n",
            i, ret.rboxes[i].la,
            ret.rboxes[i].x, ret.rboxes[i].y,
            ret.rboxes[i].w, ret.rboxes[i].h);
    }

    for (size_t i = 0; i < ret.rvias_size; ++i)
    {
        printf("rvia[%zu]=(%d,%d,%d)\n",
            i, ret.rvias[i].viadef_index,
            ret.rvias[i].x, ret.rvias[i].y);
    }    

    if (ret.rboxes_size != 0)
        free(ret.rboxes);
    if (ret.rvias_size != 0)
        free(ret.rvias);

    return 0;
}
