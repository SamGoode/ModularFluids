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
		//std::cout << hResource << std::endl;
		hResource = FindResource(hModule, MAKEINTRESOURCE(resource_id), MAKEINTRESOURCE(resource_type));
		//std::cout << hResource << std::endl;

		//std::cout << hMemory << std::endl;
		hMemory = LoadResource(hModule, hResource);
		//std::cout << hMemory << std::endl;

		p.size_bytes = SizeofResource(hModule, hResource);
		p.ptr = LockResource(hMemory);
	}

	//virtual void load(int resource_id, int resource_type) override {
	//	//std::cout << hResource << std::endl;
	//	hResource = FindResource(dllModule, MAKEINTRESOURCE(resource_id), MAKEINTRESOURCE(resource_type));
	//	//std::cout << hResource << std::endl;

	//	//std::cout << hMemory << std::endl;
	//	hMemory = LoadResource(dllModule, hResource);
	//	//std::cout << hMemory << std::endl;

	//	p.size_bytes = SizeofResource(dllModule, hResource);
	//	p.ptr = LockResource(hMemory);
	//}

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
void ResourceManager::passDllModuleRef(std::size_t hModule) {
	dllModule = (HMODULE)hModule;
}


static std::unordered_map<int, IResource*> loadedResources;
void ResourceManager::loadResources() {
	loadedResources.insert({ IDR_TEXTFILE1, new Resource(dllModule, IDR_TEXTFILE1, TEXTFILE) });
	loadedResources.insert({ IDR_TEXTFILE2, new Resource(dllModule, IDR_TEXTFILE2, TEXTFILE) });
	loadedResources.insert({ IDR_TEXTFILE3, new Resource(dllModule, IDR_TEXTFILE3, TEXTFILE) });
	loadedResources.insert({ IDR_TEXTFILE4, new Resource(dllModule, IDR_TEXTFILE4, TEXTFILE) });
	loadedResources.insert({ IDR_TEXTFILE5, new Resource(dllModule, IDR_TEXTFILE5, TEXTFILE) });
}


IResource* ResourceManager::getResource(int resource_id) {
	return loadedResources[resource_id];
}


//IResource* ResourceManager::genResource() {
//	return new Resource();
//}

//IResource* ResourceManager::genResource(int resource_id, int resource_type) {
//	IResource* res = new Resource(resource_id, resource_type);
//	//res->load(resource_id, resource_type);
//	return res;
//}