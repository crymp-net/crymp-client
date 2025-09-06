#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <type_traits>

#include "CryCommon/CryInput/IInput.h"
#include "Library/StringTools.h"
#include "Library/WinAPI.h"

#include "LocalizationManager.h"

LocalizationManager LocalizationManager::s_globalInstance;

enum class ColumnID
{
	UNKNOWN,

	LABEL_NAME,
	ENGLISH_TEXT,
	ENGLISH_SUBTITLE,
	ENGLISH_CHARACTER_NAME,
	LOCALIZED_TEXT,
	LOCALIZED_SUBTITLE,
	LOCALIZED_CHARACTER_NAME,
	USE_SUBTITLE,
	SOUND_EVENT,
	SOUND_VOLUME,
	SOUND_DUCKING,
	SOUND_RADIO_RATIO,
	SOUND_RADIO_BACKGROUND,
	SOUND_RADIO_SQUELCH,
};

static constexpr struct { std::string_view name; ColumnID id; } COLUMN_ID_TABLE[] = {
	{ "key",                       ColumnID::LABEL_NAME               },
	{ "audio_filename",            ColumnID::LABEL_NAME               },
	{ "english dialogue",          ColumnID::ENGLISH_TEXT             },
	{ "english subtitle",          ColumnID::ENGLISH_SUBTITLE         },
	{ "translation",               ColumnID::LOCALIZED_TEXT           },
	{ "translated subtitle",       ColumnID::LOCALIZED_SUBTITLE       },
	{ "your translation",          ColumnID::LOCALIZED_TEXT           },
	{ "character name",            ColumnID::ENGLISH_CHARACTER_NAME   },
	{ "translated character name", ColumnID::LOCALIZED_CHARACTER_NAME },
	{ "use subtitle",              ColumnID::USE_SUBTITLE             },
	{ "prototype event",           ColumnID::SOUND_EVENT              },
	{ "volume",                    ColumnID::SOUND_VOLUME             },
	{ "ducking",                   ColumnID::SOUND_DUCKING            },
	{ "radio ratio",               ColumnID::SOUND_RADIO_RATIO        },
	{ "radio background",          ColumnID::SOUND_RADIO_BACKGROUND   },
	{ "radio squelch",             ColumnID::SOUND_RADIO_SQUELCH      },
};

static constexpr struct { std::string_view name; unsigned int id; } LANGUAGE_ID_TABLE[] = {
	{ "chinese",   0x404 },
	{ "czech",     0x405 },
	{ "english",   0x409 },
	{ "french",    0x40C },
	{ "german",    0x407 },
	{ "hungarian", 0x40E },
	{ "italian",   0x410 },
	{ "japanese",  0x411 },
	{ "korean",    0x412 },
	{ "polish",    0x415 },
	{ "russian",   0x419 },
	{ "spanish",   0x40A },
	{ "thai",      0x41E },
	{ "turkish",   0x41F },
};

static constexpr ColumnID ColumnNameToID(const std::string_view& name)
{
	for (const auto& column : COLUMN_ID_TABLE)
	{
		if (column.name == name)
		{
			return column.id;
		}
	}

	return ColumnID::UNKNOWN;
}

static constexpr unsigned int LanguageNameToID(const std::string_view& name)
{
	for (const auto& language : LANGUAGE_ID_TABLE)
	{
		if (language.name == name)
		{
			return language.id;
		}
	}

	return 0;
}

static bool ParseYesNo(const std::string_view& text, bool defaultValue = false)
{
	const std::string loweredText = StringTools::ToLower(text);

	if (loweredText == "yes"
	 || loweredText == "enable"
	 || loweredText == "true"
	 || loweredText == "1")
	{
		return true;
	}

	if (loweredText == "no"
	 || loweredText == "disable"
	 || loweredText == "false"
	 || loweredText == "0")
	{
		return false;
	}

	return defaultValue;
}

static std::vector<ColumnID> ParseHeaderRow(XmlNodeRef& rowNode)
{
	std::vector<ColumnID> columns;
	columns.reserve(32);

	int columnIndex = 0;

	const int cellCount = rowNode->getChildCount();

	for (int cell = 0; cell < cellCount; cell++)
	{
		XmlNodeRef cellNode = rowNode->getChild(cell);

		if (cellNode->isTag("Cell"))
		{
			int newIndex = 0;
			if (cellNode->getAttr("ss:Index", newIndex) && newIndex > 0)
			{
				columnIndex = newIndex - 1;
			}

			XmlNodeRef dataNode = cellNode->findChild("Data");

			if (dataNode)
			{
				const std::string columnName = StringTools::ToLower(dataNode->getContent());

				if (columnIndex >= static_cast<int>(columns.size()))
				{
					columns.resize(columnIndex + 1, ColumnID::UNKNOWN);
				}

				columns[columnIndex] = ColumnNameToID(columnName);
			}

			columnIndex++;
		}
	}

	return columns;
}

static LocalizationManager::Label ParseDataRow(XmlNodeRef& rowNode, const std::vector<ColumnID>& columns)
{
	LocalizationManager::Label result;

	int columnIndex = 0;
	bool hasLocalizedCharacterName = false;

	const auto parseDataCellContent = [&](ColumnID column, const std::string_view& content)
	{
		switch (column)
		{
			case ColumnID::UNKNOWN:
			{
				break;
			}
			case ColumnID::LABEL_NAME:
			{
				result.name = content;
				break;
			}
			case ColumnID::ENGLISH_TEXT:
			{
				result.englishText = content;
				break;
			}
			case ColumnID::ENGLISH_SUBTITLE:
			{
				result.englishSubtitle = content;
				break;
			}
			case ColumnID::ENGLISH_CHARACTER_NAME:
			{
				if (!hasLocalizedCharacterName)
				{
					StringTools::AssignTo(result.characterName, content);
				}
				break;
			}
			case ColumnID::LOCALIZED_TEXT:
			{
				StringTools::AssignTo(result.localizedText, content);
				break;
			}
			case ColumnID::LOCALIZED_SUBTITLE:
			{
				StringTools::AssignTo(result.localizedSubtitle, content);
				break;
			}
			case ColumnID::LOCALIZED_CHARACTER_NAME:
			{
				StringTools::AssignTo(result.characterName, content);
				hasLocalizedCharacterName = true;
				break;
			}
			case ColumnID::USE_SUBTITLE:
			{
				const bool defaultUseSubtitle = true;
				result.useSubtitle = ParseYesNo(content, defaultUseSubtitle);
				break;
			}
			case ColumnID::SOUND_EVENT:
			{
				result.soundEvent = content;
				break;
			}
			case ColumnID::SOUND_VOLUME:
			{
				result.soundVolume = static_cast<float>(std::atof(content.data()));
				break;
			}
			case ColumnID::SOUND_DUCKING:
			{
				result.soundDucking = static_cast<float>(std::atof(content.data()));
				break;
			}
			case ColumnID::SOUND_RADIO_RATIO:
			{
				result.soundRadioRatio = static_cast<float>(std::atof(content.data()));
				break;
			}
			case ColumnID::SOUND_RADIO_BACKGROUND:
			{
				result.soundRadioBackground = static_cast<float>(std::atof(content.data()));
				break;
			}
			case ColumnID::SOUND_RADIO_SQUELCH:
			{
				result.soundRadioSquelch = static_cast<float>(std::atof(content.data()));
				break;
			}
		}
	};

	const int cellCount = rowNode->getChildCount();

	for (int cell = 0; cell < cellCount; cell++)
	{
		XmlNodeRef cellNode = rowNode->getChild(cell);

		if (cellNode->isTag("Cell"))
		{
			int newIndex = 0;
			if (cellNode->getAttr("ss:Index", newIndex) && newIndex > 0)
			{
				columnIndex = newIndex - 1;
			}

			XmlNodeRef dataNode = cellNode->findChild("Data");

			if (dataNode)
			{
				ColumnID column = ColumnID::UNKNOWN;

				if (columnIndex < static_cast<int>(columns.size()))
				{
					column = columns[columnIndex];
				}

				const std::string_view content = dataNode->getContent();

				parseDataCellContent(column, content);
			}
		}

		columnIndex++;
	}

	return result;
}

template<typename String>
static void ExpandNewlineInPlace(String& text)
{
	auto srcIt = text.begin();
	auto dstIt = text.begin();

	while (srcIt != text.end())
	{
		if (*srcIt == '\\' && (srcIt + 1) != text.end() && *(srcIt + 1) == 'n')
		{
			*dstIt = '\n';

			++srcIt;
		}
		else
		{
			*dstIt = *srcIt;
		}

		++srcIt;
		++dstIt;
	}

	text.resize(text.length() - (srcIt - dstIt));
}

LocalizationManager::LocalizationManager()
{
}

LocalizationManager::~LocalizationManager()
{
}

bool LocalizationManager::Add(Label&& label, bool keepExisting)
{
	if (label.name.length() > 0 && label.name[0] == '@')
	{
		label.name.erase(0, 1);
	}

	if (label.name.empty())
	{
		return false;
	}

	if (label.englishText.empty() && label.localizedText.empty())
	{
		CryLogWarning("[LocalizationManager::Add] Dropping empty label %s", label.name.c_str());

		return false;
	}

	StringTools::ToLowerInPlace(label.name);
	StringTools::ToLowerInPlace(label.soundEvent);

	ExpandNewlineInPlace(label.englishText);
	ExpandNewlineInPlace(label.englishSubtitle);
	ExpandNewlineInPlace(label.localizedText);
	ExpandNewlineInPlace(label.localizedSubtitle);

	// binary search
	const auto it = std::lower_bound(m_language.labels.begin(), m_language.labels.end(), label.name,
		[](const Label& label, const std::string& name)
		{
			return label.name < name;
		}
	);

	if (it == m_language.labels.end() || it->name != label.name)
	{
		m_language.labels.insert(it, std::move(label));

		return true;
	}
	else if (!keepExisting)
	{
		*it = std::move(label);

		return true;
	}
	else
	{
		return false;
	}
}

const LocalizationManager::Label* LocalizationManager::FindLabel(std::string_view name) const
{
	std::string loweredName;
	StringTools::AppendTo(loweredName, name);
	StringTools::ToLowerInPlace(loweredName);

	return this->FindLabelImpl(loweredName);
}

const LocalizationManager::Label* LocalizationManager::FindLabel(std::wstring_view name) const
{
	std::string loweredName;
	StringTools::AppendTo(loweredName, name);
	StringTools::ToLowerInPlace(loweredName);

	return this->FindLabelImpl(loweredName);
}

std::string LocalizationManager::Localize(std::string_view text) const
{
	const bool english = false;

	std::string result;
	LocalizeStringImpl(text, result, english);

	return result;
}

std::string LocalizationManager::LocalizeEnglish(std::string_view text) const
{
	const bool english = true;

	std::string result;
	LocalizeStringImpl(text, result, english);

	return result;
}

void LocalizationManager::LogInfo()
{
	CryLogAlways("----------------------------- LocalizationManager ------------------------------");

	CryLogAlways("Files:");
	for (const std::string& filename : m_filenames)
	{
		CryLogAlways("- $8\"%s\"$o", filename.c_str());
	}
	CryLogAlways("Total files: $5%zu$o", m_filenames.size());

	CryLogAlways("Labels:");
	for (const Label& label : m_language.labels)
	{
		CryLogAlways(
			"- $8\"@%s\"$o"
			" englishText=$8\"%s\"$o"
			" englishSubtitle=$8\"%s\"$o"
			" localizedText=$8\"%s\"$o"
			" localizedSubtitle=$8\"%s\"$o"
			" characterName=$8\"%s\"$o"
			" soundEvent=$8\"%s\"$o"
			" soundVolume=$5%g$o"
			" soundDucking=$5%g$o"
			" soundRadioRatio=$5%g$o"
			" soundRadioBackground=$5%g$o"
			" soundRadioSquelch=$5%g$o"
			" useSubtitle=$5%s$o",
			label.name.c_str(),
			label.englishText.c_str(),
			label.englishSubtitle.c_str(),
			StringTools::ToUtf8(label.localizedText).c_str(),
			StringTools::ToUtf8(label.localizedSubtitle).c_str(),
			StringTools::ToUtf8(label.characterName).c_str(),
			label.soundEvent.c_str(),
			label.soundVolume,
			label.soundDucking,
			label.soundRadioRatio,
			label.soundRadioBackground,
			label.soundRadioSquelch,
			label.useSubtitle ? "true" : "false"
		);
	}
	CryLogAlways("Total labels: $5%zu$o", m_language.labels.size());

	CryLogAlways("Language: $8\"%s\"$o prettyName=$8\"%s\"$o id=$5%u$o", m_language.name.c_str(),
		m_language.prettyName.c_str(), m_language.id);

	CryLogAlways("--------------------------------------------------------------------------------");
}

std::string_view LocalizationManager::GetLanguageFromSystem()
{
	static constexpr struct { char code[2 + 1]; const char* name; } LANGUAGES[] = {
		{ "en", "English" },
		{ "de", "German" },
		{ "cs", "Czech" },
		{ "sk", "Czech" },  // Slovak -> Czech
		{ "pl", "Polish" },
		{ "es", "Spanish" },
		{ "fr", "French" },
		{ "it", "Italian" },
		{ "hu", "Hungarian" },
		{ "ru", "Russian" },
		{ "be", "Russian" },  // Belarusian -> Russian
	//	{ "uk", "Russian" },  // Ukrainian -> Russian, disabled for political reasons
		{ "tr", "Turkish" },
		{ "ja", "Japanese" },
		{ "zn", "Chinese" },
		{ "th", "Thai" },
	};

	char code[8] = {};
	WinAPI::GetSystemLanguageCode(code, sizeof(code));

	for (const auto& language : LANGUAGES)
	{
		if (std::memcmp(language.code, code, 2 + 1) == 0)
		{
			return language.name;
		}
	}

	return {};
}

bool LocalizationManager::SetLanguage(const char* name)
{
	const std::string loweredName = StringTools::ToLower(name);

	m_language.id = LanguageNameToID(loweredName);
	m_language.name = loweredName;
	m_language.prettyName = name;

	return true;
}

const char* LocalizationManager::GetLanguage()
{
	return m_language.prettyName.c_str();
}

bool LocalizationManager::LoadExcelXmlSpreadsheet(const char* filename, bool reload)
{
	const std::string loweredFilename = StringTools::ToLower(filename);

	const bool wasLoaded = std::find(m_filenames.begin(), m_filenames.end(), loweredFilename) != m_filenames.end();

	if (wasLoaded && !reload)
	{
		CryLogWarning("Skipped already loaded localization file %s", filename);

		return true;
	}

	XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFile(filename);
	XmlNodeRef worksheetNode;
	XmlNodeRef tableNode;

	if (!rootNode
	 || !(worksheetNode = rootNode->findChild("Worksheet"))
	 || !(tableNode = worksheetNode->findChild("Table")))
	{
		CryLogError("Failed to load localization file %s", filename);

		return false;
	}

	int row = 0;
	const int rowCount = tableNode->getChildCount();

	std::vector<ColumnID> columns;

	for (; row < rowCount; row++)
	{
		XmlNodeRef rowNode = tableNode->getChild(row);

		if (rowNode->isTag("Row"))
		{
			columns = ParseHeaderRow(rowNode);
			row++;
			break;
		}
	}

	if (!wasLoaded)
	{
		m_language.labels.reserve(m_language.labels.size() + (rowCount - row));
	}

	for (; row < rowCount; row++)
	{
		XmlNodeRef rowNode = tableNode->getChild(row);

		if (rowNode->isTag("Row"))
		{
			const bool keepExisting = true;

			Add(ParseDataRow(rowNode, columns), keepExisting);
		}
	}

	if (!wasLoaded)
	{
		m_filenames.emplace_back(loweredFilename);
	}

	CryLog("Loaded localization file %s", filename);

	return true;
}

void LocalizationManager::FreeData()
{
	m_language = {};
	m_filenames.clear();
}

bool LocalizationManager::LocalizeString(const char* text, wstring& result, bool english)
{
	result.assign(L"");

	return LocalizeStringImpl(text ? std::string_view(text) : std::string_view(), result, english);
}

bool LocalizationManager::LocalizeString(const string& text, wstring& result, bool english)
{
	result.assign(L"");

	return LocalizeStringImpl(std::string_view(text.c_str(), text.length()), result, english);
}

bool LocalizationManager::LocalizeLabel(const char* name, wstring& result, bool english)
{
	result.assign(L"");

	return LocalizeLabelImpl(name ? std::string_view(name) : std::string_view(), result, english);
}

bool LocalizationManager::GetLocalizedInfo(const char* name, SLocalizedInfo& info)
{
	const Label* label = FindLabel(name ? std::string_view(name) : std::string_view());

	if (label)
	{
		AssignLocalizedInfo(info, *label);

		return true;
	}
	else
	{
		CryLogError("[LocalizationManager::GetLocalizedInfo] Cannot find %s", name);

		return false;
	}
}

int LocalizationManager::GetLocalizedStringCount()
{
	return static_cast<int>(m_language.labels.size());
}

bool LocalizationManager::GetLocalizedInfoByIndex(int index, SLocalizedInfo& info)
{
	if (index >= 0 && index < GetLocalizedStringCount())
	{
		AssignLocalizedInfo(info, m_language.labels[index]);

		return true;
	}
	else
	{
		return false;
	}
}

bool LocalizationManager::GetLocalizedSoundInfo(const char* name, SLocalizedSoundInfo& soundInfo)
{
	const Label* label = FindLabel(name ? std::string_view(name) : std::string_view());

	if (label)
	{
		AssignLocalizedSoundInfo(soundInfo, *label);

		return true;
	}
	else
	{
		CryLogError("[LocalizationManager::GetLocalizedSoundInfo] Cannot find %s", name);

		return false;
	}
}

bool LocalizationManager::GetEnglishString(const char* name, string& result)
{
	const Label* label = FindLabel(name ? std::string_view(name) : std::string_view());

	if (label)
	{
		result = label->englishText.c_str();

		return true;
	}
	else
	{
		CryLogError("[LocalizationManager::GetEnglishString] Cannot find %s", name);

		result = name;

		return false;
	}
}

bool LocalizationManager::GetSubtitle(const char* name, wstring& result, bool forceSubtitle)
{
	result.assign(L"");

	const Label* label = FindLabel(name ? std::string_view(name) : std::string_view());

	if (label && (label->useSubtitle || forceSubtitle))
	{
		if (!label->localizedSubtitle.empty())
		{
			StringTools::AppendTo(result, label->localizedSubtitle);
		}
		else if (!label->localizedText.empty())
		{
			StringTools::AppendTo(result, label->localizedText);
		}
		else if (!label->englishSubtitle.empty())
		{
			StringTools::AppendTo(result, label->englishSubtitle);
		}
		else if (!label->englishText.empty())
		{
			StringTools::AppendTo(result, label->englishText);
		}

		return true;
	}
	else
	{
		if (!label)
		{
			CryLogError("[LocalizationManager::GetSubtitle] Cannot find %s", name);
		}

		return false;
	}
}

void LocalizationManager::FormatStringMessage(string& result, const string& format, const char** args, int count)
{
	FormatStringMessageImpl(result, std::string_view(format.c_str(), format.length()), args, count);
}

void LocalizationManager::FormatStringMessage(string& result, const string& format, const char* arg1,
                                                                                    const char* arg2,
                                                                                    const char* arg3,
                                                                                    const char* arg4)
{
	const char* args[4] = { arg1, arg2, arg3, arg4 };

	FormatStringMessageImpl(result, std::string_view(format.c_str(), format.length()), args, 4);
}

void LocalizationManager::FormatStringMessage(wstring& result, const wstring& format, const wchar_t** args, int count)
{
	FormatStringMessageImpl(result, std::wstring_view(format.c_str(), format.length()), args, count);
}
void LocalizationManager::FormatStringMessage(wstring& result, const wstring& format, const wchar_t* arg1,
                                                                                      const wchar_t* arg2,
                                                                                      const wchar_t* arg3,
                                                                                      const wchar_t* arg4)
{
	const wchar_t* args[4] = { arg1, arg2, arg3, arg4 };

	FormatStringMessageImpl(result, std::wstring_view(format.c_str(), format.length()), args, 4);
}

wchar_t LocalizationManager::ToUpperCase(wchar_t ch)
{
	return WinAPI::WideCharToUpper(ch, m_language.id);
}

wchar_t LocalizationManager::ToLowerCase(wchar_t ch)
{
	return WinAPI::WideCharToLower(ch, m_language.id);
}

void LocalizationManager::LocalizeTime(time_t time, bool isLocalTime, bool showSeconds, wstring& result)
{
	// always use ISO 8601
	const wchar_t* format = showSeconds ? L"%H:%M:%S" : L"%H:%M";

	const std::tm dateTime = isLocalTime ? *std::localtime(&time) : *std::gmtime(&time);

	constexpr std::size_t BUFFER_SIZE = 64;
	wchar_t buffer[BUFFER_SIZE];
	const std::size_t length = std::wcsftime(buffer, BUFFER_SIZE, format, &dateTime);

	result.assign(buffer, length);
}

void LocalizationManager::LocalizeDate(time_t time, bool isLocalTime, bool useShort, bool showWeekday, wstring& result)
{
	// always use ISO 8601
	const wchar_t* format = L"%Y-%m-%d";

	const std::tm dateTime = isLocalTime ? *std::localtime(&time) : *std::gmtime(&time);

	constexpr std::size_t BUFFER_SIZE = 64;
	wchar_t buffer[BUFFER_SIZE];
	const std::size_t length = std::wcsftime(buffer, BUFFER_SIZE, format, &dateTime);

	result.assign(buffer, length);
}

void LocalizationManager::LocalizeDuration(int seconds, wstring& result)
{
	if (seconds < 0)
	{
		seconds = 0;
	}

	const int days    = seconds / (60 * 60 * 24);
	const int hours   = seconds / (60 * 60);
	const int minutes = seconds / (60);

	seconds -= days    * (60 * 60 * 24);
	seconds -= hours   * (60 * 60);
	seconds -= minutes * (60);

	string buffer;

	if (days > 1)
	{
		buffer.Format("%d @ui_days %02d:%02d:%02d", days, hours, minutes, seconds);
	}
	else if (days > 0)
	{
		buffer.Format("%d @ui_day %02d:%02d:%02d", days, hours, minutes, seconds);
	}
	else if (hours > 0)
	{
		buffer.Format("%02d:%02d:%02d", hours, minutes, seconds);
	}
	else
	{
		buffer.Format("%02d:%02d", minutes, seconds);
	}

	LocalizeString(buffer, result);
}

const LocalizationManager::Label* LocalizationManager::FindLabelImpl(std::string_view loweredName) const
{
	if (loweredName.length() > 0 && loweredName[0] == '@')
	{
		loweredName.remove_prefix(1);
	}

	// binary search
	const auto it = std::lower_bound(m_language.labels.begin(), m_language.labels.end(), loweredName,
		[](const Label& label, const std::string_view& loweredName)
		{
			return label.name < loweredName;
		}
	);

	if (it != m_language.labels.end() && it->name == loweredName)
	{
		return &(*it);
	}
	else
	{
		return nullptr;
	}
}

template<typename NameStringView, typename ResultString>
bool LocalizationManager::LocalizeControlCode(NameStringView name, ResultString& result) const
{
	if (name.length() <= 4)
	{
		return false;
	}

	const bool isPrefixValid = (name[0] == '@')
	                        && (name[1] == 'c' || name[1] == 'C')
	                        && (name[2] == 'c' || name[2] == 'C')
	                        && (name[3] == '_');

	if (!isPrefixValid)
	{
		return false;
	}

	name.remove_prefix(4);

	if (!gEnv || !gEnv->pInput)
	{
		return false;
	}

	IInput* pInput = gEnv->pInput;

	std::string keyName;
	StringTools::AppendTo(keyName, name);

	SInputEvent event;
	event.deviceId = eDI_Keyboard;
	event.keyName = keyName.c_str();

	const char* localizedKeyName = pInput->GetKeyName(event, true);

	if (!localizedKeyName || !*localizedKeyName)
	{
		const wchar_t* localizedKeyNameWide = pInput->GetOSKeyName(event);

		if (localizedKeyNameWide && *localizedKeyNameWide)
		{
			StringTools::AppendTo(result, localizedKeyNameWide);

			return true;
		}
		else
		{
			event.deviceId = eDI_Unknown;

			localizedKeyName = pInput->GetKeyName(event, true);
		}
	}

	if (!localizedKeyName || !*localizedKeyName)
	{
		return false;
	}

	constexpr std::size_t BUFFER_SIZE = 32;
	wchar_t buffer[BUFFER_SIZE] = {};
	WinAPI::RawANSIToWide(localizedKeyName, StringTools::Length(localizedKeyName), buffer, BUFFER_SIZE - 1);
	buffer[0] = WinAPI::WideCharToUpper(buffer[0], m_language.id);

	StringTools::AppendTo(result, buffer);

	return true;
}

template<typename NameStringView, typename ResultString>
bool LocalizationManager::LocalizeLabelImpl(NameStringView name, ResultString& result, bool english) const
{
	if (name.empty() || name[0] != '@')
	{
		return false;
	}

	if (name.length() > 1 && name[1] == '{')
	{
		if (name.back() == '}')
		{
			name.remove_prefix(2);
			name.remove_suffix(1);
		}
		else
		{
			return false;
		}
	}

	const Label* label = FindLabel(name);

	if (label)
	{
		if (english || label->localizedText.empty())
		{
			StringTools::AppendTo(result, label->englishText);
		}
		else
		{
			StringTools::AppendTo(result, label->localizedText);
		}

		return true;
	}
	else if (LocalizeControlCode(name, result))
	{
		return true;
	}
	else
	{
		StringTools::AppendTo(result, name);

		return false;
	}
}

template<typename TextStringView, typename ResultString>
bool LocalizationManager::LocalizeStringImpl(TextStringView text, ResultString& result, bool english) const
{
	bool success = true;

	while (!text.empty())
	{
		bool specifier = true;
		auto pos = text.find('@');
		if (pos == TextStringView::npos)
		{
			pos = text.length();
			specifier = false;
		}

		StringTools::AppendTo(result, TextStringView(text.data(), pos));
		text.remove_prefix(pos);

		if (specifier)
		{
			if (text.length() > 1 && text[1] == '{')
			{
				pos = text.find('}');

				if (pos != TextStringView::npos)
				{
					pos++;
				}
			}
			else
			{
				pos = text.find(' ');
			}

			if (pos == TextStringView::npos)
			{
				pos = text.length();
			}

			const TextStringView label(text.data(), pos);

			if (!LocalizeLabelImpl(label, result, english))
			{
				StringTools::AppendTo(result, label);
				success = false;
			}

			text.remove_prefix(pos);
		}
	}

	return success;
}

template<typename ResultString, typename T>
void LocalizationManager::FormatStringMessageImpl(ResultString& result,
                                                  std::basic_string_view<T> format, const T** args, int count) const
{
	// set to false if you want the same behavior as in the original implementation
	constexpr bool localize = true;

	while (!format.empty())
	{
		bool specifier = true;
		auto pos = format.find('%');
		if (pos == std::basic_string_view<T>::npos)
		{
			pos = format.length();
			specifier = false;
		}

		if (localize)
		{
			LocalizeStringImpl(std::basic_string_view<T>(format.data(), pos), result);
		}
		else
		{
			StringTools::AppendTo(result, std::basic_string_view<T>(format.data(), pos));
		}

		format.remove_prefix(pos);

		if (specifier)
		{
			if (format.length() < 2)
			{
				StringTools::AppendTo(result, std::basic_string_view<T>(format.data(), 1));

				format.remove_prefix(1);
			}
			else
			{
				const int index = format[1] - '1';

				if (index >= 0 && index < count && args[index])
				{
					if (localize)
					{
						LocalizeStringImpl(std::basic_string_view<T>(args[index]), result);
					}
					else
					{
						StringTools::AppendTo(result, std::basic_string_view<T>(args[index]));
					}
				}
				else
				{
					StringTools::AppendTo(result, std::basic_string_view<T>(format.data(), 2));
				}

				format.remove_prefix(2);
			}
		}
	}
}

void LocalizationManager::AssignLocalizedInfo(SLocalizedInfo& info, const Label& label)
{
	info.sKey = label.name.c_str();
	info.sEnglish = label.englishText.c_str();
	info.sLocalizedString = label.localizedText.c_str();
	info.sEnglishSubtitle = label.englishSubtitle.c_str();
	info.sLocalizedSubtitle = label.localizedSubtitle.c_str();
	info.sWho = label.characterName.c_str();
	info.bUseSubtitle = label.useSubtitle;
}

void LocalizationManager::AssignLocalizedSoundInfo(SLocalizedSoundInfo& soundInfo, const Label& label)
{
	soundInfo.sEnglish = label.englishText.c_str();
	soundInfo.sLocalizedString = label.localizedText.c_str();
	soundInfo.sEnglishSubtitle = label.englishSubtitle.c_str();
	soundInfo.sLocalizedSubtitle = label.localizedSubtitle.c_str();
	soundInfo.sSoundEvent = label.soundEvent.c_str();
	soundInfo.sWho = label.characterName.c_str();
	soundInfo.fVolume = label.soundVolume;
	soundInfo.fDucking = label.soundDucking;
	soundInfo.fRadioRatio = label.soundRadioRatio;
	soundInfo.fRadioBackground = label.soundRadioBackground;
	soundInfo.fRadioSquelch = label.soundRadioSquelch;
	soundInfo.bUseSubtitle = label.useSubtitle;
}
