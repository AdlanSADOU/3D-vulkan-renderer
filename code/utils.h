#pragma once

// Callee allocates dst
static errno_t GetPathFolder(char **dst, const char *src, uint32_t srcLength)
{
    assert(src);
    assert(srcLength > 0);

    size_t size = std::string(src).find_last_of('/');
    *dst         = (char*)malloc(sizeof(char) * size+1);
    return strncpy_s(*(char**)dst, size+2, src, size+1);
}