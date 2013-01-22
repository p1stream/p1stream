const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_LINEAR;

kernel void RGBAtoYUV420(read_only image2d_t input, global write_only uchar* output)
{
    size_t wUV = get_global_size(0);
    size_t hUV = get_global_size(1);
    size_t xUV = get_global_id(0);
    size_t yUV = get_global_id(1);

    size_t wY = wUV * 2;
    size_t hY = hUV * 2;
    size_t xY = xUV * 2;
    size_t yY = yUV * 2;

    float2 xyImg = (float2)(xY, yY);
    size_t lenUV = wUV * hUV;
    size_t lenY = wY * hY;

    float4 s;
    size_t base;
    float value;

    // Write 2x2 block of Y values.
    base = yY * wY + xY;
    for (size_t dx = 0; dx < 2; dx++) {
        for (size_t dy = 0; dy < 2; dy++) {
            s = read_imagef(input, sampler, xyImg + (float2)(dx, dy) + 0.5f);
            value = 16 + 65.481f*s.r + 128.553f*s.g + 24.966f*s.b;
            output[base + dy * wY + dx] = value;
        }
    }

    // Write UV values.
    s = read_imagef(input, sampler, xyImg + 1.0);
    base = yUV * wUV + xUV;
    value = 128 - 37.797f*s.r - 74.203f*s.g + 112.0f*s.b;
    output[lenY + base] = value;
    value = 128 + 112.0f*s.r - 93.786f*s.g - 18.214f*s.b;
    output[lenY + lenUV + base] = value;
}