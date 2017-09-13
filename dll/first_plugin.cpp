// first_plugin.cpp : Defines the exported functions for the DLL application.
//

#include <PluginRegistry.h>
#include "first_plugin.h"
#include <string.h>
#include <stdio.h>



#ifdef DEBUG
extern "C" {
#endif
	int my_add(int a, int b);

	void my_set_value(FirstPluginData* data, int v);

	int my_get_value(FirstPluginData* data);

	static FirstPlugin INSTANCE = { 0, my_add, my_set_value, my_get_value };

	int my_add(int a, int b) {
		return (a + b) * 6;
	}

	void my_set_value(FirstPluginData* data,int v) {
		data->value = v;
	}

	int my_get_value(FirstPluginData* data) {
		return data->value;
	}

	

	DLL_EXPORT void load_first_plugin(plugin_registry* registry) {
		registry->add(FIRST_PLUGIN_NAME, &INSTANCE, sizeof(FirstPlugin));
		
	}
#ifdef DEBUG
}
#endif
