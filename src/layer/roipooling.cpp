// Copyright 2017 Tencent
// SPDX-License-Identifier: BSD-3-Clause

#include "roipooling.h"

namespace ncnn {

ROIPooling::ROIPooling()
{
}

int ROIPooling::load_param(const ParamDict& pd)
{
    pooled_width = pd.get(0, 0);
    pooled_height = pd.get(1, 0);
    spatial_scale = pd.get(2, 1.f);

    return 0;
}

int ROIPooling::forward(const std::vector<Mat>& bottom_blobs, std::vector<Mat>& top_blobs, const Option& opt) const
{
    const Mat& bottom_blob = bottom_blobs[0];
    int w = bottom_blob.w;
    int h = bottom_blob.h;
    size_t elemsize = bottom_blob.elemsize;
    int channels = bottom_blob.c;

    const Mat& roi_blob = bottom_blobs[1];

    Mat& top_blob = top_blobs[0];
    top_blob.create(pooled_width, pooled_height, channels, elemsize, opt.blob_allocator);
    if (top_blob.empty())
        return -100;

    // For each ROI R = [x y w h]: max pool over R
    const float* roi_ptr = roi_blob;

    int roi_x1 = static_cast<int>(round(roi_ptr[0] * spatial_scale));
    int roi_y1 = static_cast<int>(round(roi_ptr[1] * spatial_scale));
    int roi_x2 = static_cast<int>(round(roi_ptr[2] * spatial_scale));
    int roi_y2 = static_cast<int>(round(roi_ptr[3] * spatial_scale));

    int roi_w = std::max(roi_x2 - roi_x1 + 1, 1);
    int roi_h = std::max(roi_y2 - roi_y1 + 1, 1);

    float bin_size_w = (float)roi_w / (float)pooled_width;
    float bin_size_h = (float)roi_h / (float)pooled_height;

    #pragma omp parallel for num_threads(opt.num_threads)
    for (int q = 0; q < channels; q++)
    {
        const float* ptr = bottom_blob.channel(q);
        float* outptr = top_blob.channel(q);

        for (int ph = 0; ph < pooled_height; ph++)
        {
            for (int pw = 0; pw < pooled_width; pw++)
            {
                // Compute pooling region for this output unit:
                //  start (included) = floor(ph * roi_height / pooled_height)
                //  end (excluded) = ceil((ph + 1) * roi_height / pooled_height)
                int hstart = static_cast<int>(roi_y1 + floorf(ph * bin_size_h));
                int wstart = static_cast<int>(roi_x1 + floorf(pw * bin_size_w));
                int hend = static_cast<int>(roi_y1 + ceilf((ph + 1) * bin_size_h));
                int wend = static_cast<int>(roi_x1 + ceilf((pw + 1) * bin_size_w));

                hstart = std::min(std::max(hstart, 0), h);
                wstart = std::min(std::max(wstart, 0), w);
                hend = std::min(std::max(hend, 0), h);
                wend = std::min(std::max(wend, 0), w);

                bool is_empty = (hend <= hstart) || (wend <= wstart);

                float max = is_empty ? 0.f : ptr[hstart * w + wstart];

                for (int y = hstart; y < hend; y++)
                {
                    for (int x = wstart; x < wend; x++)
                    {
                        int index = y * w + x;
                        max = std::max(max, ptr[index]);
                    }
                }

                outptr[pw] = max;
            }

            outptr += pooled_width;
        }
    }

    return 0;
}

} // namespace ncnn
