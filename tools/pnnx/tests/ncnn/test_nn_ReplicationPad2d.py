# Copyright 2021 Tencent
# SPDX-License-Identifier: BSD-3-Clause

import torch
import torch.nn as nn
import torch.nn.functional as F

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.pad_0 = nn.ReplicationPad2d(2)
        self.pad_1 = nn.ReplicationPad2d(padding=(3,4,5,6))
        self.pad_2 = nn.ReplicationPad2d(padding=(1,0,2,0))

    def forward(self, x):
        x = self.pad_0(x)
        x = self.pad_1(x)
        x = self.pad_2(x)
        return x

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 12, 13, 13)

    a = net(x)

    # export torchscript
    mod = torch.jit.trace(net, x)
    mod.save("test_nn_ReplicationPad2d.pt")

    # torchscript to pnnx
    import os
    os.system("../../src/pnnx test_nn_ReplicationPad2d.pt inputshape=[1,12,13,13]")

    # ncnn inference
    import test_nn_ReplicationPad2d_ncnn
    b = test_nn_ReplicationPad2d_ncnn.test_inference()

    return torch.allclose(a, b, 1e-4, 1e-4)

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
