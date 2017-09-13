#pragma once

#define FIRST_PLUGIN_NAME "first_plugin"

#ifdef DEBUG
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#ifdef DEBUG
extern "C" {
#endif
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

	DLL_EXPORT void load_first_plugin(plugin_registry* registry);
#ifdef DEBUG
}
#endif
