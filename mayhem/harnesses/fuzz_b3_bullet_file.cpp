// .bullet files through the Bullet3 copy of the reader (src/Bullet3Serialize/Bullet2FileLoader):
// b3BulletFile(buf, len).parse().
#include <stdint.h>
#include <vector>
#include "Bullet3Serialize/Bullet2FileLoader/b3BulletFile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 24)) return -1;  // the constructor takes an int length
	// A .bullet file starts with a 12-byte header ("BULLETf_v28x", SIZEOFBLENDERHEADER in b3File.h);
	// bFile::parseHeader copies those 12 bytes without checking the length. Shorter inputs are not
	// .bullet files and would only rediscover that one read past the end, so they are rejected.
	if (size < 12) return -1;
	std::vector<char> buf(data, data + size);
	bParse::b3BulletFile file(buf.data(), (int)buf.size());
	if (file.ok())
		file.parse(0);
	return 0;
}
