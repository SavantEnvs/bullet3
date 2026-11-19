// A soft-body world info for the soft-body file loaders.
#include "btBulletDynamicsCommon.h"
#include "BulletSoftBody/btSoftBody.h"
#include "BulletSoftBody/btSoftBodyHelpers.h"
#include "BulletSoftBody/btSoftRigidDynamicsWorld.h"
#include "BulletSoftBody/btSoftBodyRigidBodyCollisionConfiguration.h"

struct SoftWorld
{
	btSoftBodyRigidBodyCollisionConfiguration config;
	btCollisionDispatcher dispatcher;
	btDbvtBroadphase broadphase;
	btSequentialImpulseConstraintSolver solver;
	btSoftRigidDynamicsWorld world;
	SoftWorld() : dispatcher(&config), world(&dispatcher, &broadphase, &solver, &config)
	{
		btSoftBodyWorldInfo& info = world.getWorldInfo();
		info.m_dispatcher = &dispatcher;
		info.m_broadphase = &broadphase;
		info.m_gravity.setValue(0, -10, 0);
		info.m_sparsesdf.Initialize();
	}
	// Adds the body, steps once, removes and frees it.
	void run(btSoftBody* body)
	{
		if (!body) return;
		world.addSoftBody(body);
		world.stepSimulation(1.f / 60.f, 1);
		world.removeSoftBody(body);
		delete body;
	}
};
