#pragma once

#define FIRST_PLUGIN_NAME "first_plugin"
extern "C" {

	struct FirstPlugin {

		int(*add)(int a, int b);
	};

	struct ds_api_registry;

	__declspec(dllexport) void load_first_plugin(ds_api_registry* registry);

}
