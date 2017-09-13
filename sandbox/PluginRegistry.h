#pragma once
#include <stdint.h>

struct PluginInstance {
	void* data;
	uint32_t pluginHash;
};

struct plugin_registry {

	enum ResultCode {
		RC_OK,
		RC_ERROR,
		RC_NO_METHOD
	};

	void (*add)(const char* name, void* interf);

	bool (*contains)(const char* name);

	void*(*get_plugin_data)(const char* name);

	PluginInstance* (*get)(const char* name);

	void(*check_plugins)();

	bool(*load_plugin)(const char* path, const char* name);

	const char*(*get_last_error)();
};

plugin_registry* get_registry();

void shutdown_registry();