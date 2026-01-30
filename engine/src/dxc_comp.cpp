#include "dxc_comp.h"

#include <iostream>

// 1. Define NOMINMAX to stop Windows from breaking std::min/std::max
#define NOMINMAX 

// 2. Windows and COM headers
#ifdef _WIN32
#include <windows.h>
#endif

// 3. DXC Headers
#include <dxc/dxcapi.h>

// 4. WRL for smart pointers (ComPtr)
#include <wrl/client.h> 

using namespace Microsoft::WRL;

// --- The Hidden Implementation Struct ---
struct ShaderCompilerImpl
{
	ComPtr<IDxcUtils> _utils;
	ComPtr<IDxcCompiler3> _compiler;
};

// --- Constructor / Destructor ---

ShaderCompiler::ShaderCompiler() : _impl(std::make_unique<ShaderCompilerImpl>())
{
}

// The destructor must be here where ShaderCompilerImpl is fully defined
ShaderCompiler::~ShaderCompiler() = default;

ShaderCompiler::ShaderCompiler(ShaderCompiler&&) noexcept = default;
ShaderCompiler& ShaderCompiler::operator=(ShaderCompiler&&) noexcept = default;


// --- Internal Helper ---
namespace
{
	std::wstring GetTargetProfile(const std::filesystem::path& aPath)
	{
		const auto ext = aPath.extension().string();
		if (ext == ".vert") return L"vs_6_0";
		if (ext == ".frag") return L"ps_6_0";
		if (ext == ".comp") return L"cs_6_0";
		return L"vs_6_0"; // Default or error
	}
}

// --- Public Methods ---

bool ShaderCompiler::Init() const
{
	// Initialize DXC Utils
	HRESULT hRes = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&_impl->_utils));
	if (FAILED(hRes))
	{
		std::cerr << "Failed to initialize DXC Utils" << '\n';
		return false;
	}

	// Initialize DXC Compiler
	hRes = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&_impl->_compiler));
	if (FAILED(hRes))
	{
		std::cerr << "Failed to initialize DXC Compiler" << '\n';
		return false;
	}

	return true;
}

std::vector<uint32_t> ShaderCompiler::Compile(const std::filesystem::path& path, bool optimize) const
{
	// Safety check
	if (!_impl->_compiler || !_impl->_utils)
	{
		std::cerr << "ShaderCompiler not initialized!" << '\n';
		return {};
	}

	const std::wstring profile = GetTargetProfile(path);

	// Convert path to wide string for Windows API
	// (Note: std::filesystem handles this, but DXC specifically wants LPCWSTR)
	const std::wstring widePath = path.c_str();

	// 1. Load File
	uint32_t codePage = DXC_CP_ACP;
	ComPtr<IDxcBlobEncoding> sourceBlob;

	HRESULT hres = _impl->_utils->LoadFile(widePath.c_str(), &codePage, &sourceBlob);
	if (FAILED(hres))
	{
		std::cerr << "Could not load shader file: " << path.string() << '\n';
		return {};
	}

	// 2. Build Arguments
	std::vector<LPCWSTR> arguments;
	arguments.push_back(L"-E");
	arguments.push_back(L"main");
	arguments.push_back(L"-T");
	arguments.push_back(profile.c_str());
	arguments.push_back(L"-spirv");
	arguments.push_back(L"-fspv-target-env=vulkan1.2");

	if (optimize)
	{
		arguments.push_back(L"-O3");
	}
	else
	{
		arguments.push_back(L"-Od");
		arguments.push_back(L"-Zi");
	}

	// 3. Compile
	DxcBuffer buffer = {};
	buffer.Encoding = DXC_CP_ACP;
	buffer.Ptr = sourceBlob->GetBufferPointer();
	buffer.Size = sourceBlob->GetBufferSize();

	ComPtr<IDxcResult> result;
	hres = _impl->_compiler->Compile(
		&buffer,
		arguments.data(),
		static_cast<uint32_t>(arguments.size()),
		nullptr,
		IID_PPV_ARGS(&result)
	);

	if (FAILED(hres)) return {};

	// 4. Check Errors
	ComPtr<IDxcBlobUtf8> errors;
	result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0)
	{
		std::cerr << "Shader Error " << path.string() << ":\n" << errors->GetStringPointer() << '\n';
	}

	// 5. Get Binary
	ComPtr<IDxcBlob> blob;
	result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
	if (!blob) return {};

	std::vector<uint32_t> spirv(blob->GetBufferSize() / sizeof(uint32_t));
	memcpy(spirv.data(), blob->GetBufferPointer(), blob->GetBufferSize());

	return spirv;
}
