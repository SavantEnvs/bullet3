// COLLADA .dae meshes: LoadMeshFromCollada (geometry library, float/int array tokenising,
// visual scene transforms, index handling) over tinyxml2.
#include <stdint.h>
#include "fuzz_common.h"
#include "Importers/ImportColladaDemo/LoadMeshFromCollada.h"
#include "Utils/b3BulletDefaultFileIO.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	FuzzTempFile f(data, size, ".dae");
	if (!f.ok) return 0;
	b3BulletDefaultFileIO fileIO;
	btAlignedObjectArray<GLInstanceGraphicsShape> visualShapes;
	btAlignedObjectArray<ColladaGraphicsInstance> instances;
	btTransform upAxisTrans;
	upAxisTrans.setIdentity();
	float unitMeterScaling = 1.f;
	LoadMeshFromCollada(f.path.c_str(), visualShapes, instances, upAxisTrans, unitMeterScaling, 2, &fileIO);
	return 0;
}
