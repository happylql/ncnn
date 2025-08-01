# Copyright 2023 Tencent
# SPDX-License-Identifier: BSD-3-Clause

import torch
import torch.nn as nn
import torch.nn.functional as F

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

    def forward(self, a0, a1):
        a = torch.mm(a0, a1)
        return a

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    a0 = torch.rand(23, 14)
    a1 = torch.rand(14, 35)

    a = net(a0, a1)

    # export torchscript
    mod = torch.jit.trace(net, (a0, a1))
    mod.save("test_torch_mm.pt")

    # torchscript to pnnx
    import os
    os.system("../../src/pnnx test_torch_mm.pt inputshape=[23,14],[14,35]")

    # ncnn inference
    import test_torch_mm_ncnn
    b = test_torch_mm_ncnn.test_inference()

    return torch.allclose(a, b, 1e-4, 1e-4)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
