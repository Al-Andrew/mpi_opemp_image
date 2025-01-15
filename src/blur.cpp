#include <mpi.h>
#include "stb_image_write.h"

#include "operations.hpp"

static constexpr int32_t kernel_size = 3;
static constexpr int32_t kernel_radius = kernel_size / 2;


// Simple box blur
void blur_main(Color* image, int width, int height, Color* output, int this_worker, int worker_count) {
    (void)this_worker;
    int32_t rows_per_worker = height / worker_count;
    int32_t remaining_rows = height % worker_count;


    // Send the image data to all workers with overlap
    for (int32_t i = 1; i < worker_count; ++i) {
        int32_t start_row = i * rows_per_worker - kernel_size;
        int32_t num_rows = rows_per_worker + kernel_radius * 2;
        
        if (i == worker_count - 1) {
            num_rows += remaining_rows;
        }

        if (start_row + num_rows > height) {
            num_rows = height - start_row;
        }

        MPI_Send(image + start_row * width, num_rows * width * sizeof(Color), MPI_BYTE, i, 0, MPI_COMM_WORLD);
    }

    // Blur the colors in the main worker's portion
    for (int32_t y = 0; y < rows_per_worker; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            int32_t rsum = 0;
            int32_t gsum = 0;
            int32_t bsum = 0;
            int32_t count = 0;
            

            // consider hand unrolling the kernel loop
            for (int32_t ky = -kernel_radius; ky <= kernel_radius; ++ky) {
                for (int32_t kx = -kernel_radius; kx <= kernel_radius; ++kx) {
                    int32_t ny = y + ky;
                    int32_t nx = x + kx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        Color pixel = image[ny * width + nx];
                        rsum += pixel.r;
                        gsum += pixel.g;
                        bsum += pixel.b;
                        ++count;
                    }
                }
            }
            output[y * width + x] = {
                static_cast<uint8_t>(static_cast<float>(rsum) / count),
                static_cast<uint8_t>(static_cast<float>(gsum) / count),
                static_cast<uint8_t>(static_cast<float>(bsum) / count),
                255
            };
        }
    }

    // Receive the blurred parts from all workers
    for (int32_t i = 1; i < worker_count; ++i) {
        int32_t start_row = i * rows_per_worker;
        int32_t num_rows = rows_per_worker;
        
        if (i == worker_count - 1) {
            num_rows += remaining_rows;
        }

        if (start_row + num_rows > height) {
            num_rows = height - start_row;
        }

        MPI_Recv(output + start_row * width, num_rows * width * sizeof(Color), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

void blur_worker(int width, int height, int this_worker, int worker_count) {
    int rows_per_worker = height / worker_count;
    int remaining_rows = height % worker_count;

    int32_t start_row = this_worker * rows_per_worker - kernel_size;
    int32_t num_rows = rows_per_worker + kernel_radius * 2;
    
    if (this_worker == worker_count - 1) {
        num_rows += remaining_rows;
    }

    if (start_row + num_rows > height) {
        num_rows = height - start_row;
    }

    int32_t effective_rows = num_rows - 2 * kernel_radius;

    Color* local_image = new Color[num_rows * width];
    Defer free_local_image([local_image](){ delete[] local_image; });
    Color* local_output = new Color[effective_rows * width]; // Exclude the overlap rows
    Defer free_local_output([local_output](){ delete[] local_output; });

    // Receive the image data from the main worker
    MPI_Recv(local_image, num_rows * width * sizeof(Color), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Blur the colors in the worker's portion
    for (int32_t y = 1; y < num_rows - 1; ++y) { // Exclude the overlap rows
        for (int32_t x = 0; x < width; ++x) {
            int32_t rsum = 0;
            int32_t gsum = 0;
            int32_t bsum = 0;
            int32_t count = 0;
            
            // consider hand unrolling the kernel loop
            for (int32_t ky = -kernel_radius; ky <= kernel_radius; ++ky) {
                for (int32_t kx = -kernel_radius; kx <= kernel_radius; ++kx) {
                    int32_t ny = y + ky;
                    int32_t nx = x + kx;
                    if (ny >= 0 && ny < height && nx >= 0 && nx < width) {
                        Color pixel = local_image[ny * width + nx];
                        rsum += pixel.r;
                        gsum += pixel.g;
                        bsum += pixel.b;
                        ++count;
                    }
                }
            }
            local_output[(y - 1) * width + x] = {
                static_cast<uint8_t>(static_cast<float>(rsum) / count),
                static_cast<uint8_t>(static_cast<float>(gsum) / count),
                static_cast<uint8_t>(static_cast<float>(bsum) / count),
                255
            };
        }
    }

    // Send the blurred parts back to the main worker
    MPI_Send(local_output, effective_rows * width * sizeof(Color), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
}
