#pragma once
#include <stdint.h>

struct PluginInstance {
	void* data;
	uint32_t pluginHash;
};

struct plugin_registry {

	void (*add)(const char* name, void* interf, size_t size);

	bool (*contains)(const char* name);

	void*(*get_plugin_data)(const char* name);

	PluginInstance* (*get)(const char* name);

	void(*check_plugins)();

	bool(*load_plugin)(const char* path, const char* name);
};

plugin_registry create_registry();

void shutdown_registry();