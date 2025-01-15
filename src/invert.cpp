#include <mpi.h>

#include "operations.hpp"

// Color invert
void invert_main(Color* image, int width, int height, Color* output, int this_worker, int worker_count) {
    (void)this_worker;
    int32_t rows_per_worker = height / worker_count;
    int32_t remaining_rows = height % worker_count;

    // Send the image data to all workers
    for (int32_t i = 1; i < worker_count; ++i) {
        int32_t start_row = i * rows_per_worker;
        int32_t num_rows = (i == worker_count - 1) ? rows_per_worker + remaining_rows : rows_per_worker;
        MPI_Send(image + start_row * width, num_rows * width * sizeof(Color), MPI_BYTE, i, 0, MPI_COMM_WORLD);
    }

    // Invert the colors in the main worker's portion
    #pragma omp parallel for schedule(runtime)
    for (int32_t y = 0; y < rows_per_worker; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            output[y * width + x] = {
                static_cast<uint8_t>(255 - image[y * width + x].r), 
                static_cast<uint8_t>(255 - image[y * width + x].g),
                static_cast<uint8_t>(255 - image[y * width + x].b),
                image[y * width + x].a,
            };
        }
    }

    // Receive the inverted parts from all workers
    for (int32_t i = 1; i < worker_count; ++i) {
        int32_t start_row = i * rows_per_worker;
        int32_t num_rows = (i == worker_count - 1) ? rows_per_worker + remaining_rows : rows_per_worker;
        MPI_Recv(output + start_row * width, num_rows * width * sizeof(Color), MPI_BYTE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

void invert_worker(int width, int height, int this_worker, int worker_count) {
    int32_t rows_per_worker = height / worker_count;
    int32_t remaining_rows = height % worker_count;
    int32_t num_rows = (this_worker == worker_count - 1) ? rows_per_worker + remaining_rows : rows_per_worker;

    Color* local_image = new Color[num_rows * width];
    Defer free_local_image([local_image](){ delete[] local_image; });
    Color* local_output = new Color[num_rows * width];
    Defer free_local_output([local_output](){ delete[] local_output; });

    // Receive the image data from the main worker
    MPI_Recv(local_image, num_rows * width * sizeof(Color), MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Invert the colors in the worker's portion
    #pragma omp parallel for schedule(runtime)
    for (int32_t y = 0; y < num_rows; ++y) {
        for (int32_t x = 0; x < width; ++x) {
            local_output[y * width + x] = {
                static_cast<uint8_t>(255 - local_image[y * width + x].r), 
                static_cast<uint8_t>(255 - local_image[y * width + x].g),
                static_cast<uint8_t>(255 - local_image[y * width + x].b),
                local_image[y * width + x].a,
            };
        }
    }
    // Send the inverted parts back to the main worker
    MPI_Send(local_output, num_rows * width * sizeof(Color), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
}
