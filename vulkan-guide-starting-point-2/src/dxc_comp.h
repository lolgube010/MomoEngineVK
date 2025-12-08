#pragma once

#include <vector>
#include <filesystem>
#include <memory> // For std::unique_ptr
#include <cstdint>

// Forward declaration of the implementation struct
struct ShaderCompilerImpl;

class ShaderCompiler
{
public:
	ShaderCompiler();
	~ShaderCompiler(); // Destructor must be defined in .cpp for Pimpl

	// No copy allowed (because we own a unique pointer)
	ShaderCompiler(const ShaderCompiler&) = delete;
	ShaderCompiler& operator=(const ShaderCompiler&) = delete;

	// Move is okay
	ShaderCompiler(ShaderCompiler&&) noexcept;
	ShaderCompiler& operator=(ShaderCompiler&&) noexcept;

	[[nodiscard]] bool Init() const;

	[[nodiscard]] std::vector<uint32_t> Compile(const std::filesystem::path& path, bool optimize = false) const;

private:
	// This pointer hides all the ugly Windows/COM details
	std::unique_ptr<ShaderCompilerImpl> _impl;
};
