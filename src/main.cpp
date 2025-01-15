#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <fstream>
#include <chrono>

// MPI header
#include <mpi.h>

// 3rd party library for loading PNGs
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// 3rd party library for writing PNGs
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "util.hpp"
#include "operations.hpp"


std::vector<OperationType> parse_pipeline(std::string const& pipeline) {
    std::vector<OperationType> ops;

    std::ifstream file(pipeline);

    if(!file.is_open()) {
        std::cerr << "Failed to open file: " << pipeline << std::endl;
        std::terminate();
    }

    std::string line;
    while(std::getline(file, line)) {
        size_t i = 0;
        for(; i < OperationType::COUNT; i++) {
            if(line[0] == '#') {
                break;
            }

            if(line == operations[i].name) {
                ops.push_back(static_cast<OperationType>(i));
                break;
            }
        }

        if(i == OperationType::COUNT) {
            std::cerr << "Invalid operation: " << line << std::endl;
            std::terminate();
        }
    }

    return ops;
}

void run_operations_main(Color* image, int full_width, int full_height, std::vector<OperationType> ops, int this_worker, int worker_count) {
    Color* output = new Color[full_width * full_height];
    Defer free_output([output](){ delete[] output; });

    for(auto op : ops) {
        operations[op].main_function(image, full_width, full_height, output, this_worker, worker_count);
        std::swap(image, output);
    }

    std::copy(image, image + full_width * full_height, output);
}

void run_operations_worker(int full_width, int full_height, std::vector<OperationType> ops, int this_worker, int worker_count) {
    for(auto op : ops) {
        operations[op].worker_function(full_width, full_height, this_worker, worker_count);
    }
}



void main_worker(int rank, int size, std::string const& image_path, std::string const& pipeline_path) {
    // std::cout << "Image path: " << image_path << std::endl;
    // std::cout << "Pipeline: " << pipeline_path << std::endl;

    std::vector<OperationType> ops = parse_pipeline(pipeline_path);
    
    int full_width, full_height, channels;
    Color* img = (Color*)stbi_load(image_path.c_str(), &full_width, &full_height, &channels, 4);
    if(img == nullptr) {
        std::cerr << "Failed to load image: " << image_path << std::endl;
        std::terminate();
    }
    Defer free_img([img](){ stbi_image_free(img); });


    // seens fair to also measure the time to send metadata
    auto start = std::chrono::high_resolution_clock::now();

    
    // Send pipeline data
    int ops_size = ops.size();
    MPI_Bcast(&ops_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(ops.data(), ops_size, MPI_INT, 0, MPI_COMM_WORLD);

    // Send full image dimensions
    MPI_Bcast(&full_width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&full_height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Run operations
    run_operations_main(img, full_width, full_height, ops, rank, size);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto time_in_s = std::chrono::duration<double>(end - start).count();

    printf("Image: %dx%d, Pipeline: %s, PipelineSize: %d, Time: %f\n", full_width, full_height, pipeline_path.c_str(), ops_size, time_in_s);

    stbi_write_png("output.png", full_width, full_height, 4, img, 0);
}

void worker(int rank, int size) {
    // Receive pipeline data
    int ops_size;
    MPI_Bcast(&ops_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    std::vector<OperationType> ops(ops_size);
    MPI_Bcast(ops.data(), ops_size, MPI_INT, 0, MPI_COMM_WORLD);

    // Receive image size
    int full_width, full_height;
    MPI_Bcast(&full_width, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&full_height, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Run operations
    run_operations_worker(full_width, full_height, ops, rank, size);
}


int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    Defer mpi_finalize([](){ MPI_Finalize(); });

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(rank == 0) {
        if(argc < 3) {
            std::cerr << "Usage: " << argv[0] << " <image> <pipeline>" << std::endl;
            return 1;
        }

        std::string image_path = argv[1];
        std::string pipeline_path = argv[2];

        main_worker(rank, size, image_path, pipeline_path);
    } else {
        worker(rank, size);
    }

    return 0;
}