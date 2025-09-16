// dllmain.cpp : Defines the entry point for the DLL application.

#include "framework.h"
#include "ResourceManager.h"


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        ResourceManager::Init((std::size_t)hModule);
        ResourceManager::LoadResources();
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

