# Goal

The goal is to provide a simple solution to reload parts of the application dynamically at runtime.

# Note

I am currently working on this document. This is more of a draft rather than a full documentation.

# Reference

I have been following the inspiring blog ourmachinery.com. They have described a pretty neat
plugin system. Everything that you will find here is based on their ideas. You can find a very
detailed explanation [here](http://ourmachinery.com/post/little-machines-working-together-part-1/) and [here](http://ourmachinery.com/post/little-machines-working-together-part-2/).

# The basic idea

The basic is idea to separate the code into plugins that contain only small parts of your application. These plugins will be provided
as DLLs and the application should reload them if they have changed. There is a registry which is used to load the plugins and also
handles the reload. Plugins can also use other plugins by using the registry.

Another idea is to split such a plugin into data and code. The data will live inside the main application and should not be 
changed during a hot reload. We want to keep the data but just change the implementation of the plugin.

Therefore this solution uses C and not C++. Trying to achieve the same with C++ is much harder. It also contradicts the
idea of separating code and data. 

# What we have here

- restricted to C only
- build a struct containing function pointers
- externalize data

# Plugins

So what exactly describes a plugin. Since we are using C only I have adapted the idea of using a struct containing
all methods as function pointers. There is also a load method which will be called by the plugin_registry. There
is an important naming convention here. The load method is called load_XXX where XXX is the name of the plugin.
By using this approach we only need to keep track of one pointer and need to repatch that pointer when we reload
the plugin.
Here is an example:
```
struct my_plugin {

    void (*add)(int a, int b);
};

void load_my_plugin(plugin_registry* registry);
```

# Difference between stateless and statefull data

The method exposed as API from our plugin might also need some kind of data. 
This data is statefull and therefore cannot be kept inside the plugin. 
Here is an example demonstrating this:
```
struct FirstPluginData {

	int value;
};

struct FirstPlugin {

	FirstPluginData* data;

	int(*add)(int a, int b);

	void(*set_value)(FirstPluginData* data, int v);

	int(*get_value)(FirstPluginData* data);

};
```
First we define a struct describing the data we need. In order to keep the data outside we need to pass
it to every function that will work on the data. This is mainly because the data should be kept alive
during a reload. It the data would live inside the plugin it would be reset every time. 
Here is a short example (the code will be explained further down):
```
PluginInstance* inst = registry.get(FIRST_PLUGIN_NAME);
FirstPlugin* tmp = (FirstPlugin*)inst->data;
FirstPluginData data;
tmp->set_value(&data,100);
```
Now the FirstPluginData is kept outside and the value will be 100 after every reload.
This is one of the key concepts to get the full power of hot reloading.


# Registry

```
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
```

# The PluginInstance

This is quite important to note. When you load a plugin you can ask the plugin_registry to get a PluginInstance.
This pointer will be kept alive during the entire lifetime of your application. It is save to keep this pointer.
In order to finally get to the real instance you need to do it like this:
```
PluginInstance* inst = registry.get(FIRST_PLUGIN_NAME);
FirstPlugin* tmp = (FirstPlugin*)inst->data;
```
The "data" pointer inside PluginInstance will change every time the plugin is loaded. So you need to obtain
this one every time before you actually use this.

# Loading plugins

The following code shows the main.cpp from this project. It basically shows all
steps to load plugin and use it.

```
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
```
First of all we need to create the registry. Then we can actually load the plugin (AKA the DLL).
After that we can acquire an instance of the plugin. This instance will be kept alive across all
hot reloads. It is safe to keep this instance. The actual plugin can be accessed from the instance.
This pointer will basically change after every reload. So you cannot rely on this pointer. As you
see that plugin is always pulled from the instance inside the loop. Also inside the loop we will
check if a DLL has changed in the filesystem and the reload it.

# Windows problems

If it would be all that simple everyone would use hot reloading. But unfortunately Windows
or better to Visual Studio is no nice guy. There are actually two major problems that
are described here.

## File lock

When you load a DLL using LoadLibrary then Visual Studio will keep a file handle open
to your file. Therefore you cannot overwrite by another process. The easy way here
is to copy the DLL to another file and the load this one. Once you detect that the
original file has changed you can use FreeLibrary to release the handle and then start over
again. 

## PDB file
The next problem is the PDB file. Visual Studio will also keep a lock on this file.
There are a number of ways to tackle this one. I have decided to use an easy way and
just make sure that the PDB file will be generated with an unique name. You can
achieve this in the settings:
```
Properties -> Linker -> Debugging -> Generate Program Database File
```
You need to put in the following.
```
$(OutDir)$(TargetName)-$([System.DateTime]::Now.ToString("HH_mm_ss_fff")).pdb
```
The main drawback is that you get a new PDB file every time you compile the DLL. So
make sure that you clean up the files at a certain period. Also the really big point
is that Visual Studio will not find a PDB file and therefore will issue a full compile
on the project. This might be quite heavy depending on your code.
Again there are other options. If you find a better one then please let me know.


# Advantages and disadvantages

# One real example

Sometimes it is difficult to understand a topic by just looking at some artificial example.
Therefore I will present a real example taken from a game. It is a simple particlesystem.

## The data

The data structure for the particlesystem is straight forward. This data will live outside of the plugin.
I want to keep the data of the active particles even if the plugin reload. So I can see the changes
immediately.

```C
struct ParticleData {

	ds::vec2* positions;
	ds::vec2* scales;
	float* rotations;
	ds::Color* colors;
	float* timers;
	float* ttls;
	ds::vec4* additionals;
	char* buffer;
	uint16_t capacity;
	uint16_t index;
	
};
```
There is nothing special here.

## The plugin

The API of the plugin is also very simple. Note that there is a special method for this one
particular particle effect. All values like scale and color and so on will be hardcoded
inside these methods. Actually that is what this is all about because this is exactly the
place that I can change easily.

```C
extern "C" {

	struct ParticleData {
		....		
	};

	struct particle_manager {

		ParticleData* data;

		void(*allocate)(ParticleData* data, uint16_t max_particles);

		void(*reallocate)(ParticleData* data, uint16_t max_particles);

		void(*tick)(ParticleData* data, float dt);

		void(*emitt_explosion)(ParticleData* data, const ds::vec2& pos);

		void(*update_explosion)(ParticleData* data, float dt);

	};

	struct plugin_registry;

	__declspec(dllexport) void load_particle_manager(plugin_registry* registry);

}
```
I have added an allocate method as convenient method. Here is the code just in case:
```C
static void pm_allocate(ParticleData* data, uint16_t max_particles) {
	uint32_t sz = max_particles * (sizeof(ds::vec2) + sizeof(ds::vec2) + sizeof(float) + sizeof(ds::Color) + sizeof(float) + sizeof(float) + sizeof(ds::vec4));
	data->buffer = new char[sz];
	data->capacity = max_particles;
	data->positions = (ds::vec2*)data->buffer;
	data->scales = (ds::vec2*)(data->positions + max_particles);
	data->rotations = (float*)(data->scales + max_particles);
	data->colors = (ds::Color*)(data->rotations + max_particles);
	data->timers = (float*)(data->colors + max_particles);
	data->ttls = (float*)(data->timers + max_particles);
	data->additionals = (ds::vec4*)(data->ttls + max_particles);
	data->index = 0;
}
```
This is a neat little trick I have learned from bitsquid. The additional char* buffer. Doing it this way all our data will be contiguous in memory
and afterwards we only need to delete this pointer.

The interesting part are the emitt and update methods:
```C
void pm_emitt_explosion(ParticleData* data, const ds::vec2& pos) {
	uint16_t start = 0;
	uint16_t count = 0;
	int num = 256;
	float numf = static_cast<float>(num);
	if (pm_wake_up(data, num, &start, &count)) {
		for (uint16_t i = 0; i < count; ++i) {
			float rx = pos.x + cos(i / numf * TWO_PI) * random(60.0f, 80.0f);
			float ry = pos.y + sin(i / numf * TWO_PI) * random(60.0f, 80.0f);
			data->positions[start + i] = ds::vec2(rx, ry);
			data->additionals[start + i].x = random(0.4f, 0.8f);
			data->additionals[start + i].y = random(-1.0f, 1.0f);
			data->scales[start + i] = ds::vec2(data->additionals[start + i].x);
			data->rotations[start + i] = i / numf * TWO_PI;
			data->colors[start + i] = ds::Color(255,255,183,255);
			data->timers[start + i] = 0.0f;
			data->ttls[start + i] = random(0.8f,1.4f);
		}
	}
}

void pm_update_explosion(ParticleData* data, float dt) {
	uint16_t i = 0;
	while (i < data->index) {
		float v = 500.0f * (1.0f - data->timers[i] / data->ttls[i]);
		data->positions[i] += get_radial(data->rotations[i],v) * dt;
		data->colors[i].b = (1.0f - data->timers[i] / data->ttls[i] * 0.8f);
		data->colors[i].a = (1.0f - data->timers[i] / data->ttls[i]);
		float s = data->additionals[i].x + static_cast<float>(sin(data->timers[i] / data->ttls[i] * TWO_PI)) * 0.3f;
		data->rotations[i] += data->additionals[i].y * 10.0f * dt;
		data->scales[i] = { s,s };
		++i;
	}
}
```
As you can see everything is hardcoded. I can change any of the values or even modify more properties of a particle.
All I need is to compile the DLL and the changes will appear in the game. 
For every particle effect I have some methods basically to emitt and to update the particles.
If you compare this to a regular approach to a particle system then it is quite obvious. You would externalize these
values somehow and then find a way to tweak this. You might even need a GUI to work on it. 

# About the code

The code is divided into two parts.

- dll - This contains a VS2015 solution containing the DLL.
- Sandbox - Contains the demo application along with the PluginRegistry

You need to run the Sandbox main.cpp and then you can actually change the code in the DLL and press F7 and there we go.

# Last words

First of all it is working. But as any piece of code it can be improved. 
This documentation might also be too short. But I have to admit that writing documentation
is not my favorite thing to do. If you have any questions or your having difficulties
understanding it than drop me a line and I will try to make it better.
The code is released under the MIT license.
I am always open to feedback especially if you make the code a little better.
Send an email to amecky@gmail.com.

