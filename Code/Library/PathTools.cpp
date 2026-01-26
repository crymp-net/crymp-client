#include "PathTools.h"

static bool IsAlpha(char ch)
{
	return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

static char ToLower(char ch)
{
	return ch |= (ch >= 'A' && ch <= 'Z') << 5;
}

static char FixSlash(char ch)
{
	return (ch == '\\') ? '/' : ch;
}

bool PathTools::IsAbsolutePath(std::string_view path)
{
	if (path.length() >= 1 && IsSlash(path[0]))
	{
		// POSIX or UNC path
		return true;
	}

	if (path.length() >= 3 && IsAlpha(path[0]) && path[1] == ':' && IsSlash(path[2]))
	{
		// Windows path
		return true;
	}

	return false;
}

bool PathTools::StartsWith(std::string_view path, std::string_view prefix)
{
	auto length = prefix.length();

	if (length > 0)
	{
		if (IsSlash(prefix.back()))
		{
			length--;
		}

		if (path.length() < length || (path.length() > length && !IsSlash(path[length])))
		{
			return false;
		}
	}

	for (std::string_view::size_type i = 0; i < length; i++)
	{
		const char chA = FixSlash(ToLower(prefix[i]));
		const char chB = FixSlash(ToLower(path[i]));

		if (chA != chB)
		{
			return false;
		}
	}

	return true;
}

std::string_view PathTools::RemoveLeadingSlashes(std::string_view path)
{
	while (!path.empty() && IsSlash(path.front()))
	{
		path.remove_prefix(1);
	}

	return path;
}

std::string_view PathTools::RemoveTrailingSlashes(std::string_view path)
{
	while (!path.empty() && IsSlash(path.back()))
	{
		path.remove_suffix(1);
	}

	return path;
}

std::string_view PathTools::DirPath(std::string_view path)
{
	path = RemoveTrailingSlashes(path);

	while (!path.empty() && !IsSlash(path.back()))
	{
		path.remove_suffix(1);
	}

	return RemoveTrailingSlashes(path);
}

std::string_view PathTools::FileName(std::string_view path)
{
	path = RemoveTrailingSlashes(path);

	const auto pos = path.find_last_of("/\\");
	if (pos != std::string_view::npos)
	{
		path.remove_prefix(pos + 1);
	}

	return path;
}

std::string_view PathTools::PathStem(std::string_view path)
{
	path = FileName(path);

	const auto pos = path.rfind('.');
	if (pos != std::string_view::npos && pos != 0)
	{
		path.remove_suffix(path.length() - pos);
	}

	return path;
}

std::string_view PathTools::PathExtension(std::string_view path)
{
	path = FileName(path);

	const auto pos = path.rfind('.');
	if (pos != std::string_view::npos && pos != 0)
	{
		path.remove_prefix(pos);
	}
	else
	{
		path.remove_prefix(path.length());
	}

	return path;
}

std::tuple<std::string_view, std::string_view> PathTools::SplitFirstPathComponent(std::string_view path)
{
	path = RemoveLeadingSlashes(path);

	std::string_view remaining;

	const auto pos = path.find_first_of("/\\");
	if (pos != std::string_view::npos)
	{
		remaining = path;
		remaining.remove_prefix(pos + 1);
		path.remove_suffix(path.length() - pos);
	}

	return {path, remaining};
}

std::tuple<std::string_view, std::string_view> PathTools::SplitPathIntoDirAndFile(std::string_view path)
{
	std::string_view dirPath = RemoveTrailingSlashes(path);
	std::string_view fileName = dirPath;

	while (!dirPath.empty() && !IsSlash(dirPath.back()))
	{
		dirPath.remove_suffix(1);
	}

	fileName.remove_prefix(dirPath.length());

	dirPath = RemoveTrailingSlashes(dirPath);

	return {dirPath, fileName};
}

std::tuple<std::string_view, std::string_view> PathTools::SplitNameIntoStemAndExtension(std::string_view name)
{
	std::string_view stem = name;
	std::string_view extension;

	const auto pos = stem.rfind('.');
	if (pos != std::string_view::npos && pos != 0)
	{
		extension = stem;
		extension.remove_prefix(pos + 1);
		stem.remove_suffix(stem.length() - pos);
	}

	return {stem, extension};
}

std::tuple<std::string_view, std::string_view> PathTools::SplitPathIntoStemAndExtension(std::string_view path)
{
	return SplitNameIntoStemAndExtension(FileName(path));
}

std::string PathTools::Join(std::string_view pathA, std::string_view pathB)
{
	std::string result;

	if (pathA.empty() || IsSlash(pathA.back()) || pathB.empty() || IsSlash(pathB.front()))
	{
		result.reserve(pathA.length() + pathB.length());

		result += pathA;
		result += pathB;
	}
	else
	{
		result.reserve(pathA.length() + 1 + pathB.length());

		result += pathA;
		result += '/';
		result += pathB;
	}

	return result;
}

void PathTools::AddTrailingSlash(std::string& path)
{
	if (!path.empty() && !IsSlash(path.back()))
	{
		path += '/';
	}
}

void PathTools::Normalize(std::string& path)
{
	std::string::size_type prefixLength = 0;

	char targetSlash = '/';

	if (path.length() >= 1 && IsSlash(path[0]))
	{
		if (path.length() >= 2 && path[0] == '\\' && path[1] == '\\')
		{
			// UNC path
			// keep backslashes in UNC paths
			targetSlash = '\\';
			prefixLength = 2;
		}
		else
		{
			// POSIX absolute path
			path[0] = targetSlash;
			prefixLength = 1;
		}
	}
	else if (path.length() >= 3 && IsAlpha(path[0]) && path[1] == ':' && IsSlash(path[2]))
	{
		// Windows absolute path
		path[2] = targetSlash;
		prefixLength = 3;
	}

	auto goodLength = prefixLength;

	while (goodLength < path.length())
	{
		const auto nextSlashPos = path.find_first_of("/\\", goodLength);
		const std::string_view element(path.data() + goodLength, ((nextSlashPos != std::string::npos) ?
			nextSlashPos : path.length()) - goodLength);

		if (element.empty())
		{
			path.erase(goodLength, 1);
			continue;
		}

		if (element == ".")
		{
			path.erase(goodLength, 2);
			continue;
		}

		if (element == "..")
		{
			auto erasePos = prefixLength;

			if (goodLength > 2)
			{
				const auto prevSlashPos = path.rfind(targetSlash, goodLength - 2);
				if (prevSlashPos != std::string::npos && prevSlashPos > prefixLength)
				{
					erasePos = prevSlashPos + 1;
				}
			}

			path.erase(erasePos, ((nextSlashPos != std::string::npos) ?
				(nextSlashPos + 1) : path.length()) - erasePos);

			goodLength = erasePos;

			continue;
		}

		goodLength += element.length();

		if (nextSlashPos != std::string::npos)
		{
			path[nextSlashPos] = targetSlash;
			goodLength++;
		}
	}

	// remove trailing slash
	// but keep the first slash
	if ((path.length() > 1 && path.back() == '/') || (path.length() > 2 && path.back() == '\\'))
	{
		path.pop_back();
	}
}
