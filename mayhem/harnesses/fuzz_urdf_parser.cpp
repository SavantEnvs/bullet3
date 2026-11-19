// URDF robot descriptions: UrdfParser::loadUrdf (links, joints, geometry, materials, sensors,
// deformables, tree building) over tinyxml2.
#include <stdint.h>
#include "fuzz_common.h"
#include "Importers/ImportURDFDemo/UrdfParser.h"
#include "Utils/b3BulletDefaultFileIO.h"

struct SilentLogger : public ErrorLogger
{
	virtual void reportError(const char*) {}
	virtual void reportWarning(const char*) {}
	virtual void printMessage(const char*) {}
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	std::string text = fuzzCString(data, size);
	b3BulletDefaultFileIO fileIO;
	UrdfParser parser(&fileIO);
	SilentLogger logger;
	parser.loadUrdf(text.c_str(), &logger, false, true);  // parseSensors on, as pybullet allows
	return 0;
}
