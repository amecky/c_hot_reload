# Goal

The goal is to provide a simple solution to reload parts of the application dynamically at runtime.

# Note

I am currently working on this document. This is more of a draft rather than a full documentation.

# Reference

I have been following the inspiring blog ourmachinery.com. They have described a pretty neat
plugin system. Everything that you will find here is based on their ideas. You can find a very
detailed explanation [here](http://ourmachinery.com/post/little-machines-working-together-part-1/) and [here](http://ourmachinery.com/post/little-machines-working-together-part-2/).

# What we have here

- restricted to C only
- build a struct containing function pointers
- externalize data

# Plugins

So what exactly describes a plugin.

```
struct my_plugin {

    void (*add)(int a, int b);
};

void load_my_plugin(plugin_registry* registry);
```

```

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
during a reload. It the data would live inside the plugin it would be reset everytime. 
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
After that we can aquire an instance of the plugin. This instance will be kept alive across all
hot reloads. It is safe to keep this instance. The actual plugin can be accessed from the instance.
This pointer will basically change after every reload. So you cannot rely on this pointer. As you
see that plugin is always pulled from the istance inside the loop. Also inside the loop we will
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
The next problem is the PDB file. Visual studion will also keep a lock on this file.
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
The main drawback is that you get a new PDB file everytime you compile the DLL. So
make sure that you clean up the files at a certain period. Also the really big point
is that Visual Studio will not find a PDB file and therefore will issue a full compile
on the project. This might be quite heavy depending on your code.
Again there are other options. If you find a better one then please let me know.


# Advantages and disadvantages

# About the code

# Last words

First of all it is working. But as any piece of code in can be improved. 
This short page might also be too short. But I have to admit that writing documentation
is not my favourite thing to do. If you have any questions or your having difficulties
understaning it that drop me a line and I will try to make it better.
The code is released under the MIT license.
I am always open to feedback especially if you make the code a little better.
Send an email to amecky@gmail.com.

