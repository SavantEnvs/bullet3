// MuJoCo MJCF XML: BulletMJCFImporter::parseMJCFString (defaults, bodies, geoms, joints,
// compiler settings, meshes).
#include <stdint.h>
#include "fuzz_common.h"
#include "Importers/ImportMJCFDemo/BulletMJCFImporter.h"
#include "Utils/b3BulletDefaultFileIO.h"

struct SilentLogger : public MJCFErrorLogger
{
	virtual void reportError(const char*) {}
	virtual void reportWarning(const char*) {}
	virtual void printMessage(const char*) {}
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::string text = fuzzCString(data, size);
	b3BulletDefaultFileIO fileIO;
	BulletMJCFImporter importer(0, 0, &fileIO, 0);
	SilentLogger logger;
	importer.parseMJCFString(text.c_str(), &logger);
	return 0;
}
