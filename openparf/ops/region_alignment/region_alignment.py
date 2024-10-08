import torch


class RegionAlignment(object):
    def __init__(self, params, placedb, data_cls):
        super(RegionAlignment, self).__init__()
        self.data_cls = data_cls
        with torch.no_grad():
            local_region_boxes = data_cls.region_boxes.clone()
            local_inst2region = data_cls.region_inst_map.b2as.clone()

        # trick: create a pesudo region, which is the whole die area for the instance that does not belong to any region
        local_region_boxes = torch.cat(
            (
                local_region_boxes,
                torch.tensor(data_cls.diearea)
                .reshape(1, 4)
                .to(local_region_boxes.device),
            ),
            dim=0,
        )

        local_inst2region[local_inst2region < 0] = len(local_region_boxes) - 1

        self.inst_boxes = torch.gather(
            local_region_boxes,
            dim=0,
            index=local_inst2region.unsqueeze(1)
            .expand(-1, local_region_boxes.size(1))
            .long(),
        )

    def forward(self, pos):
        movable_range = self.data_cls.movable_range
        with torch.no_grad():
            for i in range(4):
                j = i % 2
                foo = torch.max if i < 2 else torch.min
                pos[movable_range[0] : movable_range[1], j] = foo(
                    pos[movable_range[0] : movable_range[1], j],
                    self.inst_boxes[movable_range[0] : movable_range[1], i],
                )

    def __call__(self, pos):
        return self.forward(pos)
