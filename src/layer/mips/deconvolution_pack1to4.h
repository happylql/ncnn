// Copyright 2021 Tencent
// SPDX-License-Identifier: BSD-3-Clause

static void deconvolution_pack1to4_msa(const Mat& bottom_blob, Mat& top_blob, const Mat& weight_data_pack1ton, const Mat& bias_data, int kernel_w, int kernel_h, int dilation_w, int dilation_h, int stride_w, int stride_h, int activation_type, const Mat& activation_params, const Option& opt)
{
    int w = bottom_blob.w;
    int h = bottom_blob.h;
    int channels = bottom_blob.c;

    int outw = top_blob.w;
    int outh = top_blob.h;
    int outch = top_blob.c;

    const int kernel_extent_w = dilation_w * (kernel_w - 1) + 1;
    const int kernel_extent_h = dilation_h * (kernel_h - 1) + 1;

    const int maxk = kernel_w * kernel_h;

    const float* bias_data_ptr = bias_data;

    // num_output
    #pragma omp parallel for num_threads(opt.num_threads)
    for (int p = 0; p < outch; p++)
    {
        float* outptr = top_blob.channel(p);

        for (int i = 0; i < outh; i++)
        {
            for (int j = 0; j < outw; j++)
            {
                v4f32 _sum = (v4f32)__msa_fill_w(0);

                if (bias_data_ptr)
                {
                    _sum = (v4f32)__msa_ld_w((const float*)bias_data_ptr + p * 4, 0);
                }

                const float* kptr = (const float*)weight_data_pack1ton + maxk * channels * p * 4;

                // channels
                for (int q = 0; q < channels; q++)
                {
                    const Mat m = bottom_blob.channel(q);

                    for (int y = 0; y < kernel_h; y++)
                    {
                        int sys = (i + y * dilation_h - (kernel_extent_h - 1));
                        if (sys < 0 || sys % stride_h != 0)
                            continue;

                        int sy = sys / stride_h;
                        if (sy >= h)
                            continue;

                        const float* sptr = m.row(sy);

                        for (int x = 0; x < kernel_w; x++)
                        {
                            int sxs = (j + x * dilation_w - (kernel_extent_w - 1));
                            if (sxs < 0 || sxs % stride_w != 0)
                                continue;

                            int sx = sxs / stride_w;
                            if (sx >= w)
                                continue;

                            float val = sptr[sx];

                            int k = y * kernel_w + x;

                            v4f32 _val = (v4f32)__msa_fill_w_f32(val);
                            v4f32 _w = (v4f32)__msa_ld_w(kptr + k * 4, 0);
                            _sum = __msa_fmadd_w(_sum, _val, _w);
                        }
                    }

                    kptr += maxk * 4;
                }

                _sum = activation_ps(_sum, activation_type, activation_params);

                __msa_st_w((v4i32)_sum, outptr + j * 4, 0);
            }

            outptr += outw * 4;
        }
    }
}
