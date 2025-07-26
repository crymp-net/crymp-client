#include <ctype.h>
#include <wctype.h>
#include <string.h>
#include <array>
#include <algorithm>

#include <picosha2.h>

#include "Util.h"
#include "StringTools.h"
#include "WinAPI.h"

bool Util::LessNoCase(const std::string_view & stringA, const std::string_view & stringB)
{
	auto [itA, itB] = std::mismatch(
		stringA.begin(),
		stringA.end(),
		stringB.begin(),
		stringB.end(),
		[](char a, char b) -> bool
		{
			return tolower(a) == tolower(b);
		}
	);

	if (itB == stringB.end())
	{
		// B is a prefix of A or both strings are equal
		return false;
	}
	else if (itA == stringA.end())
	{
		// A is a prefix of B
		return true;
	}
	else
	{
		return *itA < *itB;
	}
}

bool Util::EqualNoCase(const std::string_view & stringA, const std::string_view & stringB)
{
	return std::equal(stringA.begin(), stringA.end(), stringB.begin(), stringB.end(), [](char a, char b) -> bool
	{
		return tolower(a) == tolower(b);
	});
}

bool Util::EqualNoCase(const std::wstring_view & stringA, const std::wstring_view & stringB)
{
	return std::equal(stringA.begin(), stringA.end(), stringB.begin(), stringB.end(), [](wchar_t a, wchar_t b) -> bool
	{
		return towlower(a) == towlower(b);
	});
}

bool Util::StartsWith(const std::string_view & text, const std::string_view & prefix)
{
	if (prefix.length() <= text.length())
		return prefix == std::string_view(text.data(), prefix.length());
	else
		return false;
}

bool Util::StartsWith(const std::wstring_view & text, const std::wstring_view & prefix)
{
	if (prefix.length() <= text.length())
		return prefix == std::wstring_view(text.data(), prefix.length());
	else
		return false;
}

bool Util::StartsWithNoCase(const std::string_view & text, const std::string_view & prefix)
{
	if (prefix.length() <= text.length())
		return EqualNoCase(prefix, std::string_view(text.data(), prefix.length()));
	else
		return false;
}

bool Util::StartsWithNoCase(const std::wstring_view & text, const std::wstring_view & prefix)
{
	if (prefix.length() <= text.length())
		return EqualNoCase(prefix, std::wstring_view(text.data(), prefix.length()));
	else
		return false;
}

bool Util::PathStartsWith(const std::filesystem::path & path, const std::filesystem::path & prefix)
{
	auto [prefixIt, pathIt] = std::mismatch(
		prefix.begin(),
		prefix.end(),
		path.begin(),
		path.end(),
		[](const std::filesystem::path & elementA, const std::filesystem::path & elementB) -> bool
		{
			return EqualNoCase(elementA.native(), elementB.native());
		}
	);

	return prefixIt == prefix.end();
}

std::vector<std::string_view> Util::Split(const std::string_view & text, const std::string_view & separators)
{
	std::vector<std::string_view> result;

	std::string_view remainingText = text;
	size_t pos = remainingText.find_first_of(separators);

	while (pos != std::string_view::npos)
	{
		std::string_view token(remainingText.data(), pos);

		if (!token.empty())
			result.push_back(token);

		remainingText.remove_prefix(pos + 1);
		pos = remainingText.find_first_of(separators);
	}

	if (!remainingText.empty())
		result.push_back(remainingText);

	return result;
}

std::vector<std::string_view> Util::SplitLines(const std::string_view & text)
{
	return Split(text, "\r\n");
}

std::vector<std::string_view> Util::SplitWhitespace(const std::string_view & text)
{
	return Split(text, "\t\n\v\f\r ");
}

const char *Util::CopyToBuffer(char *buffer, size_t bufferSize, const std::string_view & text)
{
	if (buffer && bufferSize > 0)
	{
		const size_t length = std::min(text.length(), bufferSize - 1);

		memcpy(buffer, text.data(), length);
		buffer[length] = '\0';
	}

	return buffer;
}

std::string Util::RemoveColorCodes(const std::string_view& text)
{
	std::string result;
	result.reserve(text.length());

	for (size_t i = 0; i < text.length(); i++)
	{
		const char ch = text[i];

		if (ch == '$')
		{
			// drop color codes
			i++;
		}
		else
		{
			result += ch;
		}
	}

	return result;
}

std::string Util::MakeHumanReadableBytes(uint64_t bytes)
{
	constexpr std::array<char, 7> PREFIXES = { 'k', 'M', 'G', 'T', 'P', 'E', 'Z' };

	if (bytes == 0)
	{
		return "0 B";
	}

	if (bytes < 2000)
	{
		return std::to_string(bytes) + " B";
	}

	double value = static_cast<double>(bytes);

	// last prefix
	char prefix = PREFIXES.back();

	for (auto it = PREFIXES.begin(); it != PREFIXES.end(); ++it)
	{
		value /= 1000;

		if (value < 2000)
		{
			prefix = *it;
			break;
		}
	}

	return StringTools::Format("%.1f %cB", value, prefix);
}

std::string Util::SecondsToString(uint64_t seconds)
{
	constexpr unsigned int SECONDS_PER_DAY = 60 * 60 * 24;

	std::string result;

	if (seconds >= SECONDS_PER_DAY)
	{
		const uint64_t days = seconds / SECONDS_PER_DAY;

		StringTools::FormatTo(result, "%u %s ", days, (days == 1) ? "day" : "days");

		seconds %= SECONDS_PER_DAY;
	}

	const unsigned int hours = static_cast<unsigned int>(seconds / 3600);
	seconds %= 3600;

	const unsigned int minutes = static_cast<unsigned int>(seconds / 60);
	seconds %= 60;

	StringTools::FormatTo(result, "%02u:%02u:%02u", hours, minutes, static_cast<unsigned int>(seconds));

	return result;
}

std::string Util::sha256(const std::string_view & text)
{
	// lowercase hex digits
	return picosha2::hash256_hex_string(text);
}

std::string Util::SHA256(const std::string_view & text)
{
	std::string result = sha256(text);

	// uppercase hex digits
	std::transform(result.begin(), result.end(), result.begin(), toupper);

	return result;
}

std::string Util::GetHWID(const std::string_view & salt)
{
	std::string hwid = Util::SHA256(WinAPI::GetMachineGUID());

	if (!hwid.empty())
		hwid += ':' + Util::SHA256(hwid + std::string(salt));

	return hwid;
}