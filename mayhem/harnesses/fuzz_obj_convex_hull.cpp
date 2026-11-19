// Wavefront OBJ meshes: LoadMeshFromObj (tiny_obj_loader + Wavefront2GLInstanceGraphicsShape);
// the vertices then go through HullLibrary::CreateConvexHull, btConvexHullComputer and
// btConvexHullShape, as the URDF/SDF importers do for convex mesh colliders.
#include <stdint.h>
#include "fuzz_common.h"
#include "btBulletDynamicsCommon.h"
#include "LinearMath/btConvexHull.h"
#include "LinearMath/btConvexHullComputer.h"
#include "Importers/ImportObjDemo/LoadMeshFromObj.h"
#include "OpenGLWindow/GLInstanceGraphicsShape.h"
#include "Utils/b3BulletDefaultFileIO.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	FuzzTempFile f(data, size, ".obj");
	if (!f.ok) return 0;
	b3BulletDefaultFileIO fileIO;
	GLInstanceGraphicsShape* shape = LoadMeshFromObj(f.path.c_str(), "", &fileIO);
	if (!shape) return 0;
	btAlignedObjectArray<btVector3> pts;
	if (shape->m_vertices)
		for (int i = 0; i < shape->m_vertices->size(); i++)
		{
			const GLInstanceVertex& v = shape->m_vertices->at(i);
			pts.push_back(btVector3(v.xyzw[0], v.xyzw[1], v.xyzw[2]));
		}
	if (pts.size() > 0 && pts.size() < 20000)
	{
		HullDesc desc(QF_TRIANGLES, pts.size(), &pts[0]);
		HullLibrary lib;
		HullResult result;
		if (lib.CreateConvexHull(desc, result) == QE_OK)
			lib.ReleaseResult(result);
		btConvexHullComputer computer;
		computer.compute(&pts[0].getX(), sizeof(btVector3), pts.size(), 0.f, 0.f);
		btConvexHullShape hull(&pts[0].getX(), pts.size(), sizeof(btVector3));
		hull.optimizeConvexHull();
		hull.initializePolyhedralFeatures();
	}
	delete shape;  // frees m_vertices and m_indices too
	return 0;
}
