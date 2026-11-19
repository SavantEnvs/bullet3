// Shared helpers for the bullet3 fuzz harnesses.
#ifndef BULLET3_FUZZ_COMMON_H
#define BULLET3_FUZZ_COMMON_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

// Writes the fuzz input to a private scratch file under $TMPDIR (default /tmp) with the given
// suffix, for loaders whose public entry point takes a path. Removed on destruction.
struct FuzzTempFile
{
	std::string path;
	bool ok;
	FuzzTempFile(const uint8_t* data, size_t size, const char* suffix) : ok(false)
	{
		const char* dir = getenv("TMPDIR");
		if (!dir || !*dir) dir = "/tmp";
		path = std::string(dir) + "/bullet3fuzzXXXXXX" + suffix;
		std::string tmpl = path;
		int fd = mkstemps(&tmpl[0], (int)strlen(suffix));
		if (fd < 0) return;
		path = tmpl;
		size_t off = 0;
		while (off < size)
		{
			ssize_t n = write(fd, data + off, size - off);
			if (n <= 0) break;
			off += (size_t)n;
		}
		close(fd);
		ok = (off == size);
	}
	~FuzzTempFile() { if (!path.empty()) unlink(path.c_str()); }
};

// NUL-terminated copy of the input, for parsers that take a C string.
static inline std::string fuzzCString(const uint8_t* data, size_t size)
{
	return std::string((const char*)data, size);
}
#endif
