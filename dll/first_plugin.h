#pragma once

#define FIRST_PLUGIN_NAME "first_plugin"

extern "C" {

	struct FirstPluginData {

		int value;
	};

	struct FirstPlugin {

		FirstPluginData* data;

		int(*add)(int a, int b);

		void(*set_value)(FirstPluginData* data, int v);

		int(*get_value)(FirstPluginData* data);
	};

	struct plugin_registry;

	__declspec(dllexport) void load_first_plugin(plugin_registry* registry);

}
