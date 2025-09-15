#include "ResourceManager.h"


#include <iostream>

#include <windows.h>
#include <cstdio>
#include "resource.h"

//#include "ModularFluids.h"

static HMODULE dllModule = NULL;

void ResourceManager::passDllModuleRef(std::size_t hModule) {
	//setDllModule((size_t)hModule);
	dllModule = (HMODULE)hModule;
}


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

	virtual void load(int resource_id, int resource_type) override {
		//std::cout << hResource << std::endl;
		hResource = FindResource(dllModule, MAKEINTRESOURCE(resource_id), MAKEINTRESOURCE(resource_type));
		//std::cout << hResource << std::endl;

		//std::cout << hMemory << std::endl;
		hMemory = LoadResource(dllModule, hResource);
		//std::cout << hMemory << std::endl;

		p.size_bytes = SizeofResource(dllModule, hResource);
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


IResource* ResourceManager::genResource() {
	return new Resource();
}

IResource* ResourceManager::genResource(int resource_id, int resource_type) {
	IResource* res = new Resource();
	res->load(resource_id, resource_type);
	return res;
}