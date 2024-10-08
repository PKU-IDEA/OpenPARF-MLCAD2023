import torch

from openparf import configure
from . import shape_alignment_cpp

if configure.compile_configurations["CUDA_FOUND"] == "TRUE":
    from . import shape_alignment_cuda


class ShapeAlignment(object):
    def __init__(self, params, placedb, data_cls):
        super(ShapeAlignment, self).__init__()
        self.shape_inst_map = data_cls.shape_inst_map
        self.inst_sizes_max = data_cls.inst_sizes_max
        self.num_threads = params.num_threads
        self.device = self.inst_sizes_max.device
        self.inst_shape_offset = shape_alignment_cpp.buildInstShapeOffset(
            self.shape_inst_map.bs.cpu(),
            self.shape_inst_map.b_starts.cpu(),
            self.inst_sizes_max.cpu(),
        ).to(self.device)

    def forward(self, pos):
        foo = (
            shape_alignment_cuda.forward if pos.is_cuda else shape_alignment_cpp.forward
        )
        with torch.no_grad():
            foo(
                pos,
                self.shape_inst_map.bs,
                self.shape_inst_map.b_starts,
                self.inst_shape_offset,
                self.num_threads,
            )

    def __call__(self, pos):
        return self.forward(pos)
