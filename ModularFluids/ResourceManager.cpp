#include "ResourceManager.h"

#include <windows.h>
#include <iostream>
#include <unordered_map>

#include "resource.h"



class Resource : public IResource {
public:
	struct Parameters {
		std::size_t size_bytes = 0;
		void* ptr = nullptr;
	};

private:
	HRSRC hResource = nullptr;
	HGLOBAL hMemory = nullptr;

	Parameters p;

public:
	Resource(HMODULE hModule, int resource_id, int resource_type) {
		hResource = FindResource(hModule, MAKEINTRESOURCE(resource_id), MAKEINTRESOURCE(resource_type));
		//std::cout << hResource << std::endl;

		hMemory = LoadResource(hModule, hResource);
		//std::cout << hMemory << std::endl;

		p.size_bytes = SizeofResource(hModule, hResource);
		p.ptr = LockResource(hMemory);
	}

	virtual std::size_t size() override {
		return p.size_bytes;
	}

	virtual void* data() override {
		return p.ptr;
	}

	virtual std::string_view toString() const override {
		std::string_view dst;
		if (p.ptr != nullptr)
			dst = std::string_view(reinterpret_cast<char*>(p.ptr), p.size_bytes);
		return dst;
	}
};


// ResourceManager internal variables
static HMODULE dllModule = NULL;
static std::unordered_map<int, IResource*> loadedResources;

namespace ResourceManager {
	void Init(std::size_t hModule) {
		dllModule = (HMODULE)hModule;
	}

	// Can replace with EnumResource functionality later.
	void LoadResources() {
		loadedResources.insert({ IDR_BEEMOVIE,				new Resource(dllModule, IDR_BEEMOVIE,				TEXTFILE) });
		loadedResources.insert({ IDR_CONFIG,				new Resource(dllModule, IDR_CONFIG,					TEXTFILE) });

		loadedResources.insert({ IDR_COMP_PARTICLE,			new Resource(dllModule, IDR_COMP_PARTICLE,			TEXTFILE) });
		loadedResources.insert({ IDR_COMP_HASHTABLE,		new Resource(dllModule, IDR_COMP_HASHTABLE,			TEXTFILE) });
		loadedResources.insert({ IDR_COMP_DENSITY,			new Resource(dllModule, IDR_COMP_DENSITY,			TEXTFILE) });
		loadedResources.insert({ IDR_COMP_PRESSURE,			new Resource(dllModule, IDR_COMP_PRESSURE,			TEXTFILE) });

		loadedResources.insert({ IDR_VERT_FULLSCREEN,		new Resource(dllModule, IDR_VERT_FULLSCREEN,		TEXTFILE) });
		loadedResources.insert({ IDR_VERT_FLUIDDEPTH,		new Resource(dllModule, IDR_VERT_FLUIDDEPTH,		TEXTFILE) });
		loadedResources.insert({ IDR_FRAG_FLUIDDEPTH,		new Resource(dllModule, IDR_FRAG_FLUIDDEPTH,		TEXTFILE) });
		loadedResources.insert({ IDR_FRAG_GAUSSBLUR,		new Resource(dllModule, IDR_FRAG_GAUSSBLUR,			TEXTFILE) });
		loadedResources.insert({ IDR_FRAG_RAYMARCH,			new Resource(dllModule, IDR_FRAG_RAYMARCH,			TEXTFILE) });
		loadedResources.insert({ IDR_FRAG_RAYMARCHBOUNDS,	new Resource(dllModule, IDR_FRAG_RAYMARCHBOUNDS,	TEXTFILE) });
	}


	IResource* GetResource(int resource_id) {
		return loadedResources[resource_id];
	}
}