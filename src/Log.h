#pragma once
namespace Log {
void initialize(void* module);
void write(const char* format, ...);
}
