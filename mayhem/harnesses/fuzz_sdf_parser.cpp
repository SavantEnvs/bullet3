// SDF world/model files: UrdfParser in SDF mode (setParseSDF(true), as BulletUrdfImporter does)
// then loadSDF.
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
	parser.setParseSDF(true);
	SilentLogger logger;
	parser.loadSDF(text.c_str(), &logger);
	return 0;
}
