// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "Patcher.h"
#include "DebugDeviceEnum.h"
#include "RSAggregatorDeviceEnum.h"
#include "Configurator.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(hModule);

		rslog::InitLog();
		rslog::info_ts() << " - Wrapper DLL loaded" << std::endl;
		PatchOriginalCode();
		break;
	case DLL_PROCESS_DETACH:
		rslog::info_ts() << " - Wrapper DLL unloaded" << std::endl;
		rslog::CleanupLog();
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
		break;
    }
    return TRUE;
}

HRESULT STDAPICALLTYPE Patched_CoCreateInstance(REFCLSID rclsid, IUnknown *pUnkOuter, DWORD dwClsContext, REFIID riid, void **ppOut)
{
	rslog::info_ts() << "Patched_CoCreateInstance called: " << riid << std::endl;

	if (!ppOut)
		return E_POINTER;

	if (riid == __uuidof(IMMDeviceEnumerator))
	{
		RSAggregatorDeviceEnum* aggregatorEnum = new RSAggregatorDeviceEnum();
		SetupDeviceEnumerator(*aggregatorEnum);

		DebugDeviceEnum* newEnum = new DebugDeviceEnum(aggregatorEnum);
		aggregatorEnum->Release();

		*ppOut = newEnum;
		return S_OK;
	}

	return CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppOut);
}

static const size_t offsetPortAudio_CaptureClientParent = 0x1f0;
static const size_t offsetPortAudio_CaptureClientStream = 0x1f4;
static const size_t offsetPortAudio_CaptureClient = 0x1f8;
static const size_t offsetPortAudio_in_ClientStream = 0x10c;
static const size_t offsetPortAudio_in_ClientParent = 0x108;
static const size_t offsetPortAudio_in_ClientProc = 0x110;

static const size_t offsetPortAudio_RenderClientParent = 0x2e8;
static const size_t offsetPortAudio_RenderClientStream = 0x2ec;
static const size_t offsetPortAudio_RenderClient = 0x2f0;
static const size_t offsetPortAudio_out_ClientStream = 0x204;
static const size_t offsetPortAudio_out_ClientParent = 0x200;
static const size_t offsetPortAudio_out_ClientProc = 0x208;


HRESULT Patched_PortAudio_MarshalStreamComPointers(void* stream)
{
	BYTE* streamBytes = (BYTE*)stream;
	IUnknown*& captureClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_CaptureClientParent]);
	IUnknown*& captureClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_CaptureClientStream]);
	IUnknown*& in_ClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_in_ClientParent]);
	IUnknown*& in_ClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_in_ClientStream]);

	IUnknown*& renderClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_RenderClientParent]);
	IUnknown*& renderClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_RenderClientStream]);
	IUnknown*& out_ClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_out_ClientParent]);
	IUnknown*& out_ClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_out_ClientStream]);

	rslog::info_ts() << __FUNCTION__ << std::endl;
	captureClientStream = nullptr;
	in_ClientStream = nullptr;
	renderClientStream = nullptr;
	out_ClientStream = nullptr;
	
	if (in_ClientParent != nullptr)
	{
		rslog::info_ts() << "  marshalling input device" << std::endl;

		// (IID_IAudioClient) marshal stream->in->clientParent into stream->in->clientStream
		in_ClientStream = in_ClientParent;
		if (in_ClientParent) in_ClientParent->AddRef();

		// (IID_IAudioCaptureClient) marshal stream->captureClientParent onto stream->captureClientStream
		captureClientStream = captureClientParent;
		if (captureClientParent) captureClientParent->AddRef();
	}

	if (out_ClientParent != nullptr)
	{
		rslog::info_ts() << "  marshalling output device" << std::endl;

		// (IID_IAudioClient) marshal stream->out->clientParent into stream->out->clientStream
		out_ClientStream = out_ClientParent;
		if (out_ClientParent) out_ClientParent->AddRef();

		// (IID_IAudioRenderClient) marshal stream->renderClientParent onto stream->renderClientStream
		renderClientStream = renderClientParent;
		if (renderClientParent) renderClientParent->AddRef();
	}

	return S_OK;
}

HRESULT Patched_PortAudio_UnmarshalStreamComPointers(void* stream)
{
	BYTE* streamBytes = (BYTE*)stream;
	IUnknown*& captureClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_CaptureClientStream]);
	IUnknown*& captureClient = *(IUnknown**)&(streamBytes[offsetPortAudio_CaptureClient]);
	IUnknown*& in_ClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_in_ClientStream]);
	IUnknown*& in_ClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_in_ClientParent]);
	IUnknown*& in_ClientProc = *(IUnknown**)&(streamBytes[offsetPortAudio_in_ClientProc]);

	IUnknown*& renderClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_RenderClientStream]);
	IUnknown*& renderClient = *(IUnknown**)&(streamBytes[offsetPortAudio_RenderClient]);
	IUnknown*& out_ClientStream = *(IUnknown**)&(streamBytes[offsetPortAudio_out_ClientParent]);
	IUnknown*& out_ClientParent = *(IUnknown**)&(streamBytes[offsetPortAudio_out_ClientParent]);
	IUnknown*& out_ClientProc = *(IUnknown**)&(streamBytes[offsetPortAudio_out_ClientProc]);

	captureClient = nullptr;
	renderClient = nullptr;
	in_ClientProc = nullptr;
	out_ClientProc = nullptr;

	rslog::info_ts() << __FUNCTION__ << std::endl;

	if (in_ClientParent != nullptr)
	{
		rslog::info_ts() << "  unmarshalling input device" << std::endl;

		// (IID_IAudioClient) unmarshal from stream->in->clientStream into stream->in->clientProc
		in_ClientProc = in_ClientStream;
		in_ClientStream = nullptr;

		// (IID_IAudioCaptureClient) unmarshal from stream->captureClientStream onto stream->captureClient
		captureClient = captureClientStream;
		captureClientStream = nullptr;
	}

	if (out_ClientParent != nullptr)
	{
		rslog::info_ts() << "  unmarshalling output device" << std::endl;

		// (IID_IAudioClient) unmarshal from stream->out->clientStream into stream->out->clientProc
		out_ClientProc = out_ClientStream;
		out_ClientStream = nullptr;

		// (IID_IAudioRenderClient) unmarshal from stream->renderClientStream onto stream->renderClient
		renderClient = renderClientStream;
		renderClientStream = nullptr;
	}

	return S_OK;
}
