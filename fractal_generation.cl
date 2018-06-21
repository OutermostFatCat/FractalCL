__kernel void fractal_generation(write_only image2d_t cl_texture) {

    int2 coord = (int2)(get_global_id(0), get_global_id(1));

    /*         REAL AXIS: [-2, 1]   */
    /*  IMAGININARY AXIS: [-1, 1]   */

    float width = 3.0;
    float height = 2.0;

    float2 mandelbrot_scale = (float2) (-2.0 + (3.0 * get_global_id(0)) / 900.0, 1 - (2.0 * get_global_id(1)) / 600.0); 
    float2 mandelbrot_vec = (0.0, 0.0);
    float iteration = 0.0;

    while (length(mandelbrot_vec) < 2.0 && iteration < 1000.0) {
      mandelbrot_vec = (float2) (pown(mandelbrot_vec.x, 2) - pown(mandelbrot_vec.y, 2) + mandelbrot_scale.x, 
                    2 * mandelbrot_vec.x * mandelbrot_vec.y + mandelbrot_scale.y);
      iteration += 1.0;
    }

    float4 pixel;

    if (length(mandelbrot_vec) < 2.0) {
      pixel = (float4) (0.0, 0.0, 0.0, 0.0);
    } else {
      pixel = (float4) ((float2) (log10((iteration, iteration))) / 2, 0.8, 0.0);
    }

    write_imagef(cl_texture, coord, pixel);
}