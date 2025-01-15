#pragma once

#include "util.hpp" 

enum OperationType{
    HFLIP = 0, 
    INVERT,
    BLUR,
    COUNT
};


struct Operation {
    using main_fn_t = void(*)(Color*, int, int, Color*, int, int);
    using worker_fn_t = void(*)(int, int, int, int);

    OperationType type;
    main_fn_t main_function;
    worker_fn_t worker_function;
    const char* name;
};


void hflip_main(Color* image, int width, int height, Color* output, int this_worker, int worker_count);
void hflip_worker(int width, int height, int this_worker, int worker_count);

void invert_main(Color* image, int width, int height, Color* output, int this_worker, int worker_count);
void invert_worker(int width, int height, int this_worker, int worker_count);

void blur_main(Color* image, int width, int height, Color* output, int this_worker, int worker_count);
void blur_worker(int width, int height, int this_worker, int worker_count);

constexpr static inline Operation operations[] = {
    {OperationType::HFLIP, hflip_main, hflip_worker, "hflip"},
    {OperationType::INVERT, invert_main, invert_worker, "invert"},
    {OperationType::BLUR,   blur_main, blur_worker, "blur"}
};