#pragma once

#include <cstdint>
#include <functional>

struct Color {
    uint8_t r, g, b, a;
};

class Defer{
public:
    [[nodiscard]]
    inline Defer(std::function<void()> func) : func(func) {}
    inline ~Defer() { func(); }
private:
    std::function<void()> func;
};

