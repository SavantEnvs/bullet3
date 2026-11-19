// .bullet files carrying btMultiBody data: btMultiBodyWorldImporter::loadFileFromMemory on a
// btMultiBodyDynamicsWorld, then one step.
#include <stdint.h>
#include <vector>
#include "btBulletDynamicsCommon.h"
#include "BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBody.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraint.h"
#include "btMultiBodyWorldImporter.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size > (1u << 24)) return -1;  // the loader takes an int length
	// A .bullet file starts with a 12-byte header ("BULLETf_v28x", SIZEOFBLENDERHEADER in bFile.h);
	// bFile::parseHeader copies those 12 bytes without checking the length. Shorter inputs are not
	// .bullet files and would only rediscover that one read past the end, so they are rejected.
	if (size < 12) return -1;
	std::vector<char> buf(data, data + size);
	btDefaultCollisionConfiguration config;
	btCollisionDispatcher dispatcher(&config);
	btDbvtBroadphase broadphase;
	btMultiBodyConstraintSolver solver;
	btMultiBodyDynamicsWorld world(&dispatcher, &broadphase, &solver, &config);
	{
		btMultiBodyWorldImporter importer(&world);
		importer.setVerboseMode(0);
		if (importer.loadFileFromMemory(buf.data(), (int)buf.size()))
			world.stepSimulation(1.f / 60.f, 1);
		for (int i = world.getNumMultiBodyConstraints() - 1; i >= 0; i--)
			world.removeMultiBodyConstraint(world.getMultiBodyConstraint(i));
		for (int i = world.getNumMultibodies() - 1; i >= 0; i--)
			world.removeMultiBody(world.getMultiBody(i));
		for (int i = world.getNumConstraints() - 1; i >= 0; i--)
			world.removeConstraint(world.getConstraint(i));
		for (int i = world.getNumCollisionObjects() - 1; i >= 0; i--)
			world.removeCollisionObject(world.getCollisionObjectArray()[i]);
		importer.deleteAllData();
	}
	return 0;
}
