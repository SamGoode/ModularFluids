#pragma once

#include <iostream>


class IResource {
public:
	virtual ~IResource() = 0 {}

	virtual std::size_t size() = 0;
	virtual void* data() = 0;

	virtual	std::string_view toString() const = 0;
};


namespace ResourceManager {
	void Init(std::size_t hModule);
	void LoadResources();

	IResource* GetResource(int resource_id);
}