// Binary STL meshes: LoadMeshFromSTL (header, triangle count vs size, vertex building).
#include <stdint.h>
#include "fuzz_common.h"
#include "Utils/b3BulletDefaultFileIO.h"
#include "Importers/ImportSTLDemo/LoadMeshFromSTL.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	// A binary STL file is an 80-byte header plus a 4-byte triangle count; LoadMeshFromSTL reads the
	// count at offset 80 before any size check, so shorter inputs (not STL files) are rejected rather
	// than rediscovering that one read past the end. Everything after is checked by the loader.
	if (size < 84) return -1;
	FuzzTempFile f(data, size, ".stl");
	if (!f.ok) return 0;
	b3BulletDefaultFileIO fileIO;
	GLInstanceGraphicsShape* shape = LoadMeshFromSTL(f.path.c_str(), &fileIO);
	delete shape;  // frees m_vertices and m_indices too
	return 0;
}
