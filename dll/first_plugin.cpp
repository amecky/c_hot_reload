// first_plugin.cpp : Defines the exported functions for the DLL application.
//

#include <ApiRegistry.h>
#include "first_plugin.h"

extern "C" {

	int my_add(int a, int b) {
		return (a + b) * 18;
	}

	static FirstPlugin INSTANCE = { my_add };

	__declspec(dllexport) void load_first_plugin(ds_api_registry* registry) {
		registry->add(FIRST_PLUGIN_NAME, &INSTANCE);
	}
}
