#pragma once

#include <iostream>


class IResource {
public:
	virtual ~IResource() = 0 {}

	virtual	std::string_view getString() const = 0;
	virtual std::size_t getSize() = 0;
	virtual void* getData() = 0;
};


namespace ResourceManager {
	void Init(std::size_t hModule);
	void LoadResources();

	IResource* GetResource(int resource_id);
}