#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "ApiRegistry.h"
#include <stdio.h>
#include "..\dll\first_plugin.h"

int main() {
	ds_api_registry registry = create_registry();
	registry.load_plugin("..\\dll\\Debug", "first_plugin");
	PluginInstance* inst = registry.get(FIRST_PLUGIN_NAME);
	while (true) {
		if (inst) {
			FirstPlugin* fp = (FirstPlugin*)inst->data;
			int ret = fp->add(100, 200);
			printf("ret: %d\n", ret);
		}
		else {
			printf("ERROR: Canot find plugin\n");
		}
		Sleep(500);
		registry.check_plugins();
	}
	shutdown_registry();
    return 0;
}

