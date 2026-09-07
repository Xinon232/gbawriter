#pragma once
#include <cstddef>
#include <cstdint>
namespace writer {
// Deliberately small, allocation-free subset: %s, %c, %d, %u, %lx and zero widths.
int format(char* out, std::size_t capacity, const char* pattern, ...);
bool parse_manifest(const char* text, std::size_t length, unsigned& size, uint32_t& hash);
}
