#include "StdFile.h"

std::string StdFile::ReadAll()
{
	std::string content;
	char buffer[512];

	while (true)
	{
		std::size_t size = std::fread(buffer, 1, sizeof(buffer), m_file.get());
		content.append(buffer, size);

		if (size < sizeof(buffer))
		{
			break;
		}
	}

	return content;
}
