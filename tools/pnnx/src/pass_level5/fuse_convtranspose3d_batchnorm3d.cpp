// Copyright 2023 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "fuse_convtranspose3d_batchnorm3d.h"

#include "pass_level2.h"

#include <math.h>
#include <string.h>

namespace pnnx {

class fuse_convtranspose3d_batchnorm3d_pass : public GraphRewriterPass
{
public:
    const char* match_pattern_graph() const
    {
        return R"PNNXIR(7767517
4 3
pnnx.Input              input       0 1 input
nn.ConvTranspose3d      op_0        1 1 input a in_channels=%in_channels out_channels=%out_channels kernel_size=%kernel_size stride=%stride output_padding=%output_padding padding=%padding dilation=%dilation groups=%groups bias=%bias @weight @bias
nn.BatchNorm3d          op_1        1 1 a out num_features=%num_features eps=%eps affine=%affine @running_mean @running_var @weight @bias
pnnx.Output             output      1 0 out
)PNNXIR";
    }

    const char* type_str() const
    {
        return "nn.ConvTranspose3d";
    }

    const char* name_str() const
    {
        return "convtransposebn3d";
    }

    void write(Operator* op, const std::map<std::string, Parameter>& captured_params, const std::map<std::string, Attribute>& captured_attrs) const
    {
        op->params["in_channels"] = captured_params.at("in_channels");
        op->params["out_channels"] = captured_params.at("out_channels");
        op->params["kernel_size"] = captured_params.at("kernel_size");
        op->params["stride"] = captured_params.at("stride");
        op->params["output_padding"] = captured_params.at("output_padding");
        op->params["padding"] = captured_params.at("padding");
        op->params["dilation"] = captured_params.at("dilation");
        op->params["groups"] = captured_params.at("groups");
        op->params["bias"] = true;

        // resolve merged convtranspose3d weight and bias
        int channels = captured_params.at("num_features").i;
        float bn_eps = captured_params.at("eps").f;
        bool has_bn_affine = captured_params.at("affine").b;
        bool has_convtranspose_bias = captured_params.at("bias").b;

        auto bn_running_mean = captured_attrs.at("op_1.running_mean").get_float32_data();
        auto bn_running_var = captured_attrs.at("op_1.running_var").get_float32_data();
        auto bn_weight = has_bn_affine ? captured_attrs.at("op_1.weight").get_float32_data() : std::vector<float>();
        auto bn_bias = has_bn_affine ? captured_attrs.at("op_1.bias").get_float32_data() : std::vector<float>();

        // a = bias - slope * mean / sqrt(var + eps)
        // b = slope / sqrt(var + eps)
        // value = value * b + a

        std::vector<float> a(channels);
        std::vector<float> b(channels);
        for (int i = 0; i < channels; i++)
        {
            double sqrt_var = sqrt(bn_running_var[i] + bn_eps);

            if (has_bn_affine)
            {
                a[i] = (float)(bn_bias[i] - bn_weight[i] * bn_running_mean[i] / sqrt_var);
                b[i] = (float)(bn_weight[i] / sqrt_var);
            }
            else
            {
                a[i] = (float)(-bn_running_mean[i] / sqrt_var);
                b[i] = (float)(1.f / sqrt_var);
            }
        }

        op->attrs["weight"] = captured_attrs.at("op_0.weight");

        if (has_convtranspose_bias)
        {
            op->attrs["bias"] = captured_attrs.at("op_0.bias");
        }
        else
        {
            // init bias as zero
            op->attrs["bias"] = Attribute();
            op->attrs["bias"].type = op->attrs["weight"].type;
            op->attrs["bias"].shape = {channels};
            op->attrs["bias"].set_float32_data(std::vector<float>(channels, 0.f));
        }

        auto conv_weight = op->attrs["weight"].get_float32_data();
        auto conv_bias = op->attrs["bias"].get_float32_data();

        // group-inch/group-outch/group-kh-kw
        const int inch = captured_params.at("in_channels").i;
        const int outch = captured_params.at("out_channels").i;
        const int groups = captured_params.at("groups").i;
        const int kd = captured_params.at("kernel_size").ai[0];
        const int kh = captured_params.at("kernel_size").ai[1];
        const int kw = captured_params.at("kernel_size").ai[2];

        const int outch_g = outch / groups;
        const int inch_g = inch / groups;
        const int maxk = kd * kh * kw;

        for (int g = 0; g < groups; g++)
        {
            float* wg = (float*)conv_weight.data() + g * inch_g * outch_g * maxk;
            for (int i = 0; i < inch_g; i++)
            {
                for (int j = 0; j < outch_g; j++)
                {
                    for (int k = 0; k < maxk; k++)
                    {
                        wg[(i * outch_g + j) * maxk + k] *= b[g * outch_g + j];
                    }
                }
            }
        }

        for (int i = 0; i < channels; i++)
        {
            conv_bias[i] = conv_bias[i] * b[i] + a[i];
        }

        op->attrs["weight"].set_float32_data(conv_weight);
        op->attrs["bias"].set_float32_data(conv_bias);
    }
};

void fuse_convtranspose3d_batchnorm3d(Graph& graph)
{
    fuse_convtranspose3d_batchnorm3d_pass a;
    int opindex = 0;

    pnnx_graph_rewrite(graph, &a, opindex);
}

} // namespace pnnx
