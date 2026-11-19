// .bullet binary scene files: btBulletWorldImporter::loadFileFromMemory (bFile/DNA parser,
// pointer fix-ups, shape/body/constraint reconstruction) on a fresh world, then one step.
#include <stdint.h>
#include <string.h>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "btBulletWorldImporter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 24)) return -1;  // the loader takes an int length
	// A .bullet file starts with a 12-byte header ("BULLETf_v28x", SIZEOFBLENDERHEADER in bFile.h);
	// bFile::parseHeader copies those 12 bytes without checking the length. Shorter inputs are not
	// .bullet files and would only rediscover that one read past the end, so they are rejected.
	if (size < 12) return -1;
	std::vector<char> buf(data, data + size);  // the loader rewrites the buffer in place
	btDefaultCollisionConfiguration config;
	btCollisionDispatcher dispatcher(&config);
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
	{
		btBulletWorldImporter importer(&world);
		importer.setVerboseMode(0);
		if (importer.loadFileFromMemory(buf.data(), (int)buf.size()))
			world.stepSimulation(1.f / 60.f, 1);
		for (int i = world.getNumConstraints() - 1; i >= 0; i--)
			world.removeConstraint(world.getConstraint(i));
		for (int i = world.getNumCollisionObjects() - 1; i >= 0; i--)
			world.removeCollisionObject(world.getCollisionObjectArray()[i]);
		importer.deleteAllData();
	}
	return 0;
}
