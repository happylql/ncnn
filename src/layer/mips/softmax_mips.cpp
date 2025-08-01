// Copyright 2020 Leo <leo@nullptr.com.cn>
// SPDX-License-Identifier: BSD-3-Clause

#include "softmax_mips.h"

#include <float.h>

#if __mips_msa
#include <msa.h>
#include "msa_mathfun.h"
#endif // __mips_msa

namespace ncnn {

int Softmax_mips::forward_inplace(Mat& bottom_top_blob, const Option& opt) const
{
    int dims = bottom_top_blob.dims;
    size_t elemsize = bottom_top_blob.elemsize;
    int positive_axis = axis < 0 ? dims + axis : axis;

    if (dims != 3 || positive_axis != 0)
        return Softmax::forward_inplace(bottom_top_blob, opt);

    // value = exp( value - global max value )
    // sum all value
    // value = value / sum

    int w = bottom_top_blob.w;
    int h = bottom_top_blob.h;
    int channels = bottom_top_blob.c;
    int size = w * h;

    Mat max;
    max.create(w, h, elemsize, opt.workspace_allocator);
    if (max.empty())
        return -100;
    max.fill(-FLT_MAX);
    for (int q = 0; q < channels; q++)
    {
        float* ptr = bottom_top_blob.channel(q);
        float* maxptr = max;

        for (int i = 0; i < size; i++)
        {
            maxptr[i] = std::max(maxptr[i], ptr[i]);
        }
    }

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        float* ptr = bottom_top_blob.channel(q);
        float* maxptr = max;

#if __mips_msa
        int nn = size >> 2;
        int remain = size - (nn << 2);
#else
        int remain = size;
#endif // __mips_msa

#if __mips_msa
        for (; nn > 0; nn--)
        {
            v4f32 _p = (v4f32)__msa_ld_w(ptr, 0);
            v4f32 _max = (v4f32)__msa_ld_w(maxptr, 0);

            _p = exp_ps(__msa_fsub_w(_p, _max));

            __msa_st_w((v4i32)_p, ptr, 0);

            ptr += 4;
            maxptr += 4;
        }
#endif // __mips_msa

        for (; remain > 0; remain--)
        {
            *ptr = expf(*ptr - *maxptr);

            ptr++;
            maxptr++;
        }
    }

    Mat sum;
    sum.create(w, h, elemsize, opt.workspace_allocator);
    if (sum.empty())
        return -100;
    sum.fill(0.f);
    for (int q = 0; q < channels; q++)
    {
        float* ptr = bottom_top_blob.channel(q);
        float* sumptr = sum;

#if __mips_msa
        int nn = size >> 2;
        int remain = size - (nn << 2);
#else
        int remain = size;
#endif // __mips_msa

#if __mips_msa
        for (; nn > 0; nn--)
        {
            v4f32 _p = (v4f32)__msa_ld_w(ptr, 0);
            v4f32 _sum = (v4f32)__msa_ld_w(sumptr, 0);
            _sum = __msa_fadd_w(_sum, _p);
            __msa_st_w((v4i32)_sum, sumptr, 0);

            ptr += 4;
            sumptr += 4;
        }
#endif // __mips_msa

        for (; remain > 0; remain--)
        {
            *sumptr += *ptr;

            ptr++;
            sumptr++;
        }
    }

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        float* ptr = bottom_top_blob.channel(q);
        float* sumptr = sum;

#if __mips_msa
        int nn = size >> 2;
        int remain = size - (nn << 2);
#else
        int remain = size;
#endif // __mips_msa

#if __mips_msa
        for (; nn > 0; nn--)
        {
            v4f32 _p = (v4f32)__msa_ld_w(ptr, 0);
            v4f32 _sum = (v4f32)__msa_ld_w(sumptr, 0);
            _p = __msa_fdiv_w(_p, _sum);
            __msa_st_w((v4i32)_p, ptr, 0);

            ptr += 4;
            sumptr += 4;
        }
#endif // __mips_msa

        for (; remain > 0; remain--)
        {
            *ptr /= *sumptr;

            ptr++;
            sumptr++;
        }
    }

    return 0;
}

} // namespace ncnn
