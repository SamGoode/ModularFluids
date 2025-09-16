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

	virtual std::string_view getString() const override {
		std::string_view dst;
		if (p.ptr != nullptr)
			dst = std::string_view(reinterpret_cast<char*>(p.ptr), p.size_bytes);
		return dst;
	}

	virtual std::size_t getSize() override {
		return p.size_bytes;
	}

	virtual void* getData() override {
		return p.ptr;
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
		loadedResources.insert({ IDR_BEEMOVIE, new Resource(dllModule, IDR_BEEMOVIE, TEXTFILE) });
		loadedResources.insert({ IDR_TEXTFILE2, new Resource(dllModule, IDR_TEXTFILE2, TEXTFILE) });
		loadedResources.insert({ IDR_TEXTFILE3, new Resource(dllModule, IDR_TEXTFILE3, TEXTFILE) });
		loadedResources.insert({ IDR_TEXTFILE4, new Resource(dllModule, IDR_TEXTFILE4, TEXTFILE) });
		loadedResources.insert({ IDR_TEXTFILE5, new Resource(dllModule, IDR_TEXTFILE5, TEXTFILE) });
		loadedResources.insert({ IDR_CONFIG, new Resource(dllModule, IDR_CONFIG, TEXTFILE) });
	}


	IResource* GetResource(int resource_id) {
		return loadedResources[resource_id];
	}
}