// End-to-end asset loading through the physics server (PhysicsDirect, the pybullet in-process
// path): the input is written to a scratch file and loaded with loadURDF / loadSDF / loadMJCF
// (first byte picks), which runs BulletUrdfImporter / BulletMJCFImporter, URDF2Bullet,
// MyMultiBodyCreator and btMultiBody setup; then a few simulation steps.
// The server is created once; every input ends with resetSimulation, which removes all bodies,
// so no input sees another's state.
#include <stdint.h>
#include "fuzz_common.h"
#include "SharedMemory/PhysicsClientC_API.h"
#include "SharedMemory/PhysicsDirectC_API.h"
#include "SharedMemory/SharedMemoryPublic.h"

static b3PhysicsClientHandle client;

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	client = b3ConnectPhysicsDirect();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	if (size < 1) return -1;
	if (!client) client = b3ConnectPhysicsDirect();
	int kind = data[0] % 3;
	static const char* suffixes[3] = {".urdf", ".sdf", ".xml"};
	FuzzTempFile f(data + 1, size - 1, suffixes[kind]);
	if (!f.ok) return 0;

	b3SharedMemoryCommandHandle cmd = b3InitPhysicsParamCommand(client);
	b3PhysicsParamSetGravity(cmd, 0, 0, -10);
	b3SubmitClientCommandAndWaitStatus(client, cmd);

	if (kind == 0)
		cmd = b3LoadUrdfCommandInit(client, f.path.c_str());
	else if (kind == 1)
		cmd = b3LoadSdfCommandInit(client, f.path.c_str());
	else
		cmd = b3LoadMJCFCommandInit(client, f.path.c_str());
	b3SharedMemoryStatusHandle status = b3SubmitClientCommandAndWaitStatus(client, cmd);
	int type = b3GetStatusType(status);
	if (type == CMD_URDF_LOADING_COMPLETED || type == CMD_SDF_LOADING_COMPLETED || type == CMD_MJCF_LOADING_COMPLETED)
	{
		for (int i = 0; i < 3; i++)
			b3SubmitClientCommandAndWaitStatus(client, b3InitStepSimulationCommand(client));
	}
	b3SubmitClientCommandAndWaitStatus(client, b3InitResetSimulationCommand(client));
	return 0;
}
