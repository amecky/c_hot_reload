#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include "PluginRegistry.h"
#include <stdio.h>
#include "..\dll\first_plugin.h"
#include "logger.h"

int main() {
	plugin_registry registry = create_registry();
	registry.load_plugin("..\\dll\\Debug", "first_plugin");
	PluginInstance* inst = registry.get(FIRST_PLUGIN_NAME);
	FirstPlugin* tmp = (FirstPlugin*)inst->data;
	FirstPluginData data;
	tmp->set_value(&data,100);
	while (true) {
		if (inst) {
			FirstPlugin* fp = (FirstPlugin*)inst->data;
			int ret = fp->add(100, 200);
			log("ret: %d value: %d", ret,fp->get_value(&data));
		}
		else {
			log("ERROR: Canot find plugin");
		}
		Sleep(500);
		registry.check_plugins();
	}
	shutdown_registry();
    return 0;
}

