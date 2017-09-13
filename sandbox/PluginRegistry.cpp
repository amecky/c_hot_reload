#include "PluginRegistry.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <vector>
#include <strsafe.h>
#include "logger.h"

const uint32_t FNV_Prime = 0x01000193; //   16777619
const uint32_t FNV_Seed = 0x811C9DC5; // 2166136261

// ---------------------------------------------------
// simple hashing
// ---------------------------------------------------
inline uint32_t fnv1a(const char* text, uint32_t hash = FNV_Seed) {
	const unsigned char* ptr = (const unsigned char*)text;
	while (*ptr) {
		hash = (*ptr++ ^ hash) * FNV_Prime;
	}
	return hash;
}

// ---------------------------------------------------
// Plugin
// ---------------------------------------------------
struct Plugin {
	const char* name;
	const char* path;
	void* data;
	uint32_t hash;
	FILETIME fileTime;
	HINSTANCE instance;
};


// ---------------------------------------------------
// registry context
// ---------------------------------------------------
struct RegistryContext {
	// FIXME: use std::unordered_map
	std::vector<Plugin> plugins;
	std::vector<PluginInstance> instances;

	char error_message[512];
};

static RegistryContext _ctx;

typedef void(*LoadFunc)(plugin_registry*);

void report_error(const char* message, DWORD lastError = 0) {
	if (lastError != 0) {
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			_ctx.error_message,
			0,
			NULL
		);		
	}
	else {
		sprintf_s(_ctx.error_message,"%s",message);
	}
}

// ---------------------------------------------------
// get filetime
// ---------------------------------------------------
void get_file_time(const char* fileName, FILETIME& time) {
	WORD ret = -1;
	// no file sharing mode
	HANDLE hFile = CreateFile(fileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {
		// Retrieve the file times for the file.
		GetFileTime(hFile, NULL, NULL, &time);
		CloseHandle(hFile);
	}
}

// ---------------------------------------------------
// checks if a file needs to be reloaded
// ---------------------------------------------------
bool requires_reload(const Plugin& descriptor) {
	FILETIME now;
	char buffer[256];
	char fqn[256];
	sprintf_s(fqn, "%s\\%s.dll", descriptor.path, descriptor.name);
	DWORD retval = GetFullPathName(fqn, 256, buffer, 0);
	log("checking %s", buffer);
	get_file_time(buffer, now);
	int t = CompareFileTime(&descriptor.fileTime, &now);
	if (t == -1) {
		log("=> requires reload");
		return true;
	}
	return false;
}

// ---------------------------------------------------
// find index of plugin by name
// ---------------------------------------------------
static int find_plugin(const char* name) {
	uint32_t h = fnv1a(name);
	for (size_t i = 0; i < _ctx.plugins.size(); ++i) {
		if (_ctx.plugins[i].hash == h) {
			return i;
		}
	}
	return -1;
}

// ---------------------------------------------------
// contains
// ---------------------------------------------------
bool plugin_registry_contains(const char* name) {
	int idx = find_plugin(name);
	if (idx != -1) {
		// we still need to check if we have an instance
		return _ctx.plugins[idx].instance != 0;
	}
	return false;
}

void* plugin_registry_get_plugin_data(const char* name) {
	int idx = find_plugin(name);
	log("get_plugin_data - idx: %d", idx);
	if (idx != -1) {
		return _ctx.plugins[idx].data;
	}
	return 0;
}
// ---------------------------------------------------
// add new plugin 
// ---------------------------------------------------
void plugin_registry_add(const char* name, void* interf) {
	int idx = find_plugin(name);
	if (idx != -1) {
		Plugin& p = _ctx.plugins[idx];
		p.data = interf;
		uint32_t h = fnv1a(name);
		for (size_t i = 0; i < _ctx.instances.size(); ++i) {
			if (_ctx.instances[i].pluginHash == h) {
				_ctx.instances[i].data = p.data;
			}
		}
	}
	else {
		Plugin p;
		p.data = interf;
		p.instance = 0;
		p.name = name;
		p.path = 0;
		p.hash = fnv1a(name);
		_ctx.plugins.push_back(p);
	}
};

// ---------------------------------------------------
// get plugin
// ---------------------------------------------------
PluginInstance* plugin_registry_get(const char* name) {
	uint32_t h = fnv1a(name);
	for (size_t i = 0; i < _ctx.plugins.size(); ++i) {
		if (_ctx.plugins[i].hash == h) {
			PluginInstance inst = { _ctx.plugins[i].data, h };
			log("creating new instance");
			_ctx.instances.push_back(inst);
			return &_ctx.instances[_ctx.instances.size() - 1];
		}
	}
	return 0;
}

bool plugin_registry_load_plugin(const char* path, const char* name);

bool plugin_registry_load_plugin(Plugin* plugin);

// ---------------------------------------------------
// check plugins
// ---------------------------------------------------
void plugin_registry_check_plugins() {
#ifdef DEBUG
	for (size_t i = 0; i < _ctx.plugins.size(); ++i) {
		Plugin& p = _ctx.plugins[i];
		if (requires_reload(p)) {
			plugin_registry_load_plugin(&p);
		}
	}
#endif
}

const char* plugin_registry_get_last_error() {
	return _ctx.error_message;
}

static plugin_registry REGISTRY = { 
	plugin_registry_add, 
	plugin_registry_contains, 
	plugin_registry_get_plugin_data,
	plugin_registry_get,
	plugin_registry_check_plugins,
	plugin_registry_load_plugin,
	plugin_registry_get_last_error
};

// ---------------------------------------------------
// load plugin
// ---------------------------------------------------
bool plugin_registry_load_plugin(const char* path, const char* name) {
	int l = strlen(path) + strlen(name);
	if (l >= 255) {
		report_error("The path and name of DLL is too long - only 256 chars supported");
		return false;
	}
	Plugin descriptor;
	descriptor.name = name;
	descriptor.path = path;
	descriptor.instance = 0;
	descriptor.data = 0;
	descriptor.hash = fnv1a(name);
	_ctx.plugins.push_back(descriptor);
	return plugin_registry_load_plugin(&_ctx.plugins[_ctx.plugins.size() - 1]);
}

// ---------------------------------------------------
// load plugin
// ---------------------------------------------------
static bool plugin_registry_load_plugin(Plugin* plugin) {
	char fqn[256];
	char new_buffer[256];
	char buffer[256];
	sprintf_s(fqn, "%s\\%s.dll", plugin->path, plugin->name);
	// we have to do it like this because LoadLibrary does not like relative paths
	DWORD retval = GetFullPathName(fqn, 256, buffer, 0);
	if (plugin->instance) {
		//log("freeing old instance");
		if (!FreeLibrary(plugin->instance)) {
			//log("ERROR: cannot free library");
			report_error("ERROR: Cannot free library", GetLastError());
			return false;
		}
	}
	// we need a absolute path 
	GetCurrentDirectory(256, fqn);
	//log("current dir: %s", fqn);
	// we are copying the file to the current directory to prevent locking
	sprintf_s(new_buffer, "%s\\%s.dll", fqn, plugin->name);
	//log("copy %s to %s\n", buffer, new_buffer);
	get_file_time(buffer, plugin->fileTime);
	CopyFile(buffer, new_buffer, false);	
	plugin->instance = LoadLibrary(new_buffer);
	if (plugin->instance) {
		sprintf_s(buffer, "load_%s", plugin->name);
		LoadFunc lf = (LoadFunc)GetProcAddress(plugin->instance, buffer);
		if (lf) {			
			(lf)(&REGISTRY);
			return true;
		}
		else {
			sprintf_s(_ctx.error_message, "Cannot find method: '%s' in DLL", buffer);
			return false;
		}
	}
	else {
		report_error("ERROR: No instance",GetLastError());
		return false;
	}
	return true;
}

// ---------------------------------------------------
// create registry
// ---------------------------------------------------
plugin_registry* get_registry() {
	return &REGISTRY;
}

// ---------------------------------------------------
// shutdown registry
// ---------------------------------------------------
void shutdown_registry() {
	for (size_t i = 0; i < _ctx.plugins.size(); ++i) {
		if (_ctx.plugins[i].instance) {
			FreeLibrary(_ctx.plugins[i].instance);
		}
	}
}

