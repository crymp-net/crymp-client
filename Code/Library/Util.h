#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>

namespace Util
{
	bool LessNoCase(const std::string_view & stringA, const std::string_view & stringB);

	bool EqualNoCase(const std::string_view & stringA, const std::string_view & stringB);
	bool EqualNoCase(const std::wstring_view & stringA, const std::wstring_view & stringB);

	bool StartsWith(const std::string_view & text, const std::string_view & prefix);
	bool StartsWith(const std::wstring_view & text, const std::wstring_view & prefix);
	bool StartsWithNoCase(const std::string_view & text, const std::string_view & prefix);
	bool StartsWithNoCase(const std::wstring_view & text, const std::wstring_view & prefix);

	bool PathStartsWith(const std::filesystem::path & path, const std::filesystem::path & prefix);

	std::vector<std::string_view> Split(const std::string_view & text, const std::string_view & separators);
	std::vector<std::string_view> SplitLines(const std::string_view & text);
	std::vector<std::string_view> SplitWhitespace(const std::string_view & text);

	const char *CopyToBuffer(char *buffer, size_t bufferSize, const std::string_view & text);

	std::string RemoveColorCodes(const std::string_view& text);
	std::string MakeHumanReadableBytes(uint64_t bytes);

	std::string SecondsToString(uint64_t seconds);

	std::string sha256(const std::string_view & text);  // lowercase hex digits
	std::string SHA256(const std::string_view & text);  // uppercase hex digits

	std::string GetHWID(const std::string_view & salt);

	inline std::string_view RemovePrefix(std::string_view text, size_t length)
	{
		text.remove_prefix(std::min(length, text.length()));
		return text;
	}

	inline std::string_view RemoveSuffix(std::string_view text, size_t length)
	{
		text.remove_suffix(std::min(length, text.length()));
		return text;
	}

	inline std::string_view RemovePrefixAndSuffix(std::string_view text, size_t prefixLength, size_t suffixLength)
	{
		text.remove_prefix(std::min(prefixLength, text.length()));
		text.remove_suffix(std::min(suffixLength, text.length()));
		return text;
	}

	struct TransparentStringCompare
	{
		using is_transparent = void;

		bool operator()(const std::string_view & a, const std::string_view & b) const
		{
			return a < b;
		}
	};

	struct TransparentStringCompareNoCase
	{
		using is_transparent = void;

		bool operator()(const std::string_view & a, const std::string_view & b) const
		{
			return LessNoCase(a, b);
		}
	};
}
