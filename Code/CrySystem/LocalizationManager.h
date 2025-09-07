#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "CryCommon/CrySystem/ISystem.h"

class LocalizationManager final : public ILocalizationManager
{
public:
	struct Label
	{
		std::string name;

		std::string englishText;
		std::string englishSubtitle;
		std::wstring localizedText;
		std::wstring localizedSubtitle;
		std::wstring characterName;

		std::string soundEvent;
		float soundVolume = 1.0;
		float soundDucking = 1.0;
		float soundRadioRatio = 1.0;
		float soundRadioBackground = 0;
		float soundRadioSquelch = 0;

		bool useSubtitle = true;
	};

	struct Language
	{
		unsigned int id = 0;
		std::string name;
		std::string prettyName;
		std::vector<Label> labels;
	};

private:
	Language m_language;
	std::vector<std::string> m_filenames;

	static LocalizationManager s_globalInstance;

public:
	LocalizationManager();
	~LocalizationManager();

	// to be removed once we have our own CrySystem
	static LocalizationManager& GetInstance()
	{
		return s_globalInstance;
	}

	const Language& GetCurrentLanguage() const
	{
		return m_language;
	}

	bool Add(Label&& label, bool keepExisting = false);

	const Label* FindLabel(std::string_view name) const;
	const Label* FindLabel(std::wstring_view name) const;

	std::string Localize(std::string_view text) const;
	std::string LocalizeEnglish(std::string_view text) const;

	void LogInfo();

	static std::string_view GetLanguageFromSystem();

	////////////////////////////////////////////////////////////////////////////////
	// ILocalizationManager - legacy API
	////////////////////////////////////////////////////////////////////////////////

	bool SetLanguage(const char* name) override;
	const char* GetLanguage() override;

	bool LoadExcelXmlSpreadsheet(const char* filename, bool reload = false) override;

	void FreeData() override;

	bool LocalizeString(const char* text, wstring& result, bool english = false) override;
	bool LocalizeString(const string& text, wstring& result, bool english = false) override;
	bool LocalizeLabel(const char* name, wstring& result, bool english = false) override;

	bool GetLocalizedInfo(const char* name, SLocalizedInfo& info) override;
	int GetLocalizedStringCount() override;
	bool GetLocalizedInfoByIndex(int index, SLocalizedInfo& info) override;
	bool GetLocalizedSoundInfo(const char* name, SLocalizedSoundInfo& soundInfo) override;

	bool GetEnglishString(const char* name, string& result) override;

	bool GetSubtitle(const char* name, wstring& result, bool forceSubtitle = false) override;

	void FormatStringMessage(string& result, const string& format, const char** args, int count) override;
	void FormatStringMessage(string& result, const string& format, const char* arg1,
	                                                               const char* arg2 = nullptr,
	                                                               const char* arg3 = nullptr,
	                                                               const char* arg4 = nullptr) override;

	void FormatStringMessage(wstring& result, const wstring& format, const wchar_t** args, int count) override;
	void FormatStringMessage(wstring& result, const wstring& format, const wchar_t* arg1,
	                                                                 const wchar_t* arg2 = nullptr,
	                                                                 const wchar_t* arg3 = nullptr,
	                                                                 const wchar_t* arg4 = nullptr) override;

	wchar_t ToUpperCase(wchar_t ch) override;
	wchar_t ToLowerCase(wchar_t ch) override;

	void LocalizeTime(time_t time, bool isLocalTime, bool showSeconds, wstring& result) override;
	void LocalizeDate(time_t time, bool isLocalTime, bool useShort, bool showWeekday, wstring& result) override;
	void LocalizeDuration(int seconds, wstring& result) override;

	////////////////////////////////////////////////////////////////////////////////

private:
	const Label* FindLabelImpl(std::string_view loweredName) const;

	template<typename NameStringView, typename ResultString>
	bool LocalizeControlCode(NameStringView name, ResultString& result) const;

	template<typename NameStringView, typename ResultString>
	bool LocalizeLabelImpl(NameStringView name, ResultString& result, bool english = false) const;

	template<typename TextStringView, typename ResultString>
	bool LocalizeStringImpl(TextStringView text, ResultString& result, bool english = false) const;

	template<typename ResultString, typename T>
	void FormatStringMessageImpl(ResultString& result,
	                             std::basic_string_view<T> format, const T** args, int count) const;

	static void AssignLocalizedInfo(SLocalizedInfo& info, const Label& label);
	static void AssignLocalizedSoundInfo(SLocalizedSoundInfo& soundInfo, const Label& label);
};
