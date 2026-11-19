// VTK tetrahedral meshes: btSoftBodyHelpers::CreateFromVtkFile, then one soft-body step.
#include <stdint.h>
#include "fuzz_common.h"
#include "softbody_world.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	FuzzTempFile f(data, size, ".vtk");
	if (!f.ok) return 0;
	SoftWorld sw;
	sw.run(btSoftBodyHelpers::CreateFromVtkFile(sw.world.getWorldInfo(), f.path.c_str()));
	return 0;
}
