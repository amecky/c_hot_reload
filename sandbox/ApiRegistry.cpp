#include "ApiRegistry.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <vector>
#include <strsafe.h>

void ErrorExit(const char* lpszFunction) {
	// Retrieve the system error message for the last-error code

	LPVOID lpMsgBuf;
	LPVOID lpDisplayBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf,
		0, NULL);

	// Display the error message and exit the process

	lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT,
		(lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR));
	StringCchPrintf((LPTSTR)lpDisplayBuf,
		LocalSize(lpDisplayBuf) / sizeof(TCHAR),
		TEXT("%s failed with error %d: %s"),
		lpszFunction, dw, lpMsgBuf);
	MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK);

	LocalFree(lpMsgBuf);
	LocalFree(lpDisplayBuf);
	ExitProcess(dw);
}


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

	std::vector<Plugin> plugins;
	std::vector<PluginInstance> instances;
};

static RegistryContext* _ctx = 0;

typedef void(*LoadFunc)(ds_api_registry*);

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
	sprintf_s(buffer, "%s\\%s.dll", descriptor.path, descriptor.name);
	printf("checking %s\n", buffer);
	get_file_time(buffer, now);
	int t = CompareFileTime(&descriptor.fileTime, &now);
	if (t == -1) {
		printf("=> requires reload");
		return true;
	}
	return false;
}

// ---------------------------------------------------
// find index of plugin by name
// ---------------------------------------------------
int find_plugin(const char* name) {
	uint32_t h = fnv1a(name);
	for (size_t i = 0; i < _ctx->plugins.size(); ++i) {
		if (_ctx->plugins[i].hash == h) {
			return i;
		}
	}
	return -1;
}

// ---------------------------------------------------
// add new plugin 
// ---------------------------------------------------
void api_registry_add(const char* name, void* interf) {
	int idx = find_plugin(name);
	if (idx != -1) {
		printf("updating existing plugin\n");
		Plugin& p = _ctx->plugins[idx];
		p.data = interf;
		uint32_t h = fnv1a(name);
		for (size_t i = 0; i < _ctx->instances.size(); ++i) {
			if (_ctx->instances[i].pluginHash == h) {
				printf("patching %d\n", i);
				_ctx->instances[i].data = p.data;
			}
		}
	}
	else {
		printf("ERROR: plugin not found\n");
		//Plugin np = { name, interf, fnv1a(name) };
		//_ctx->plugins.push_back(np);
	}
};

// ---------------------------------------------------
// get plugin
// ---------------------------------------------------
PluginInstance* api_registry_get(const char* name) {
	uint32_t h = fnv1a(name);
	for (size_t i = 0; i < _ctx->plugins.size(); ++i) {
		if (_ctx->plugins[i].hash == h) {
			PluginInstance inst = { _ctx->plugins[i].data, h };
			printf("creating new instance\n");
			_ctx->instances.push_back(inst);
			return &_ctx->instances[_ctx->instances.size() - 1];
		}
	}
	return 0;
}

bool api_registry_load_plugin(const char* path, const char* name);

bool api_registry_load_plugin(Plugin* plugin);

// ---------------------------------------------------
// check plugins
// ---------------------------------------------------
void api_registry_check_plugins() {
	for (size_t i = 0; i < _ctx->plugins.size(); ++i) {
		Plugin& p = _ctx->plugins[i];
		if (requires_reload(p)) {
			api_registry_load_plugin(&p);
		}
	}
}

static ds_api_registry REGISTRY = { api_registry_add, api_registry_get,api_registry_check_plugins,api_registry_load_plugin };

// ---------------------------------------------------
// load plugin
// ---------------------------------------------------
bool api_registry_load_plugin(const char* path, const char* name) {
	Plugin descriptor;
	descriptor.name = name;
	descriptor.path = path;
	descriptor.instance = 0;
	descriptor.data = 0;
	descriptor.hash = fnv1a(name);
	_ctx->plugins.push_back(descriptor);
	return api_registry_load_plugin(&_ctx->plugins[_ctx->plugins.size() - 1]);
}

// ---------------------------------------------------
// load plugin
// ---------------------------------------------------
static bool api_registry_load_plugin(Plugin* plugin) {
	char buffer[256];
	char new_buffer[256];
	sprintf_s(buffer, "%s\\%s.dll", plugin->path, plugin->name);
	if (plugin->instance) {
		printf("freeing old instance");
		if (!FreeLibrary(plugin->instance)) {
			printf("ERROR: cannot free library\n");
		}
	}
	sprintf_s(new_buffer, "%s\\%s_____.dll", plugin->path, plugin->name);
	printf("copy %s to %s\n", buffer, new_buffer);
	get_file_time(buffer, plugin->fileTime);
	CopyFile(buffer, new_buffer, false);	
	plugin->instance = LoadLibrary(new_buffer);
	if (plugin->instance) {
		printf("we have got a handle\n");
		sprintf_s(buffer, "load_%s", plugin->name);
		LoadFunc lf = (LoadFunc)GetProcAddress(plugin->instance, buffer);
		if (lf) {			
			(lf)(&REGISTRY);
			printf("LOADED\n");
			return true;
		}
		else {
			printf("ERROR: Could not find method\n");
		}
	}
	else {
		ErrorExit("ERROR: No instance\n");
	}
	return false;
}

// ---------------------------------------------------
// create registry
// ---------------------------------------------------
ds_api_registry create_registry() {
	_ctx = new RegistryContext;
	return REGISTRY;
}

// ---------------------------------------------------
// shutdown registry
// ---------------------------------------------------
void shutdown_registry() {
	if (_ctx != 0) {
		delete _ctx;
		// FIXME: call shutdown on all registered plugins???
	}
}

