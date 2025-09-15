#pragma once

#include <iostream>

//#include <windows.h>
//#include <cstdio>
//#include "resource.h"


class IResource {
public:
	virtual ~IResource() = 0 {}

	virtual void load(int resource_id, int resource_type) = 0;
	virtual	std::string_view getString() const = 0;
	virtual std::size_t getSize() = 0;
	virtual void* getData() = 0;
};


namespace ResourceManager {
	void passDllModuleRef(std::size_t hModule);

	IResource* genResource();
	IResource* genResource(int resource_id, int resource_type);
	//IResource* loadResource(int resource_id, int resource_type);
	//void loadResource(int resource_id, int resource_type);
	//void loadResources();
	//void getResourceStr();
}