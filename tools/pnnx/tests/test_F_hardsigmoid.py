# Copyright 2021 Tencent
# SPDX-License-Identifier: BSD-3-Clause

import torch
import torch.nn as nn
import torch.nn.functional as F

def hardsigmoid_forward_0(x):
    return F.relu6(x + 3., True) / 6.

def hardsigmoid_forward_1(x):
    return x.add_(3.).clamp_(0., 6.).div_(6.)

class h_sigmoid(nn.Module):
    def __init__(self, inplace=True):
        super(h_sigmoid, self).__init__()
        self.relu = nn.ReLU6(inplace=inplace)

    def forward(self, x):
        return self.relu(x + 3) / 6

class Model(nn.Module):
    def __init__(self):
        super(Model, self).__init__()

        self.h_sigmoid = h_sigmoid();

    def forward(self, x, y, z, w):
        x = x * 2 - 1
        y = y * 2 - 1
        z = z * 2 - 1
        w = w * 2 - 1
        x = F.hardsigmoid(x)
        y = F.hardsigmoid(y)
        z = self.h_sigmoid(z)
        w = hardsigmoid_forward_0(w)
        w = hardsigmoid_forward_1(w)
        return x, y, z, w

def test():
    net = Model()
    net.eval()

    torch.manual_seed(0)
    x = torch.rand(1, 16)
    y = torch.rand(12, 2, 16)
    z = torch.rand(1, 3, 12, 16)
    w = torch.rand(1, 5, 7, 9, 11)

    a = net(x, y, z, w)

    # export torchscript
    mod = torch.jit.trace(net, (x, y, z, w))
    mod.save("test_F_hardsigmoid.pt")

    # torchscript to pnnx
    import os
    os.system("../src/pnnx test_F_hardsigmoid.pt inputshape=[1,16],[12,2,16],[1,3,12,16],[1,5,7,9,11]")

    # pnnx inference
    import test_F_hardsigmoid_pnnx
    b = test_F_hardsigmoid_pnnx.test_inference()

    for a0, b0 in zip(a, b):
        if not torch.allclose(a0, b0, 1e-4, 1e-4):
            return False
    return True

if __name__ == "__main__":
    if test():
        exit(0)
    else:
        exit(1)
