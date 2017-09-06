#pragma once
#include <stdint.h>

struct PluginInstance {
	void* data;
	uint32_t pluginHash;
};

struct ds_api_registry {

	void (*add)(const char* name, void* interf);

	PluginInstance* (*get)(const char* name);

	void(*check_plugins)();

	bool(*load_plugin)(const char* path, const char* name);
};

ds_api_registry create_registry();

void shutdown_registry();