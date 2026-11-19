// Bullet XML world files: btBulletXmlWorldImporter::loadFile (XML -> serialized structs ->
// pointer fix-ups -> btWorldImporter conversion) on a fresh world.
#include <stdint.h>
#include "fuzz_common.h"
#include "btBulletDynamicsCommon.h"
#include "btBulletXmlWorldImporter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	FuzzTempFile f(data, size, ".xml");
	if (!f.ok) return 0;
	btDefaultCollisionConfiguration config;
	btCollisionDispatcher dispatcher(&config);
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
	{
		btBulletXmlWorldImporter importer(&world);
		importer.setVerboseMode(0);
		if (importer.loadFile(f.path.c_str()))
			world.stepSimulation(1.f / 60.f, 1);
		for (int i = world.getNumConstraints() - 1; i >= 0; i--)
			world.removeConstraint(world.getConstraint(i));
		for (int i = world.getNumCollisionObjects() - 1; i >= 0; i--)
			world.removeCollisionObject(world.getCollisionObjectArray()[i]);
		importer.deleteAllData();
	}
	return 0;
}
