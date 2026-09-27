#include "Hooks/Hooks.h"
#include "IngredientData/StoredData.h"
#include "Settings/INI/INISettings.h"
#include "Settings/JSON/JSONSettings.h"

static void MessageEventCallback(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		SECTION_SEPARATOR;
		if (!IngredientData::Initialize()) {
			REX::FAIL("Failed to preload ingredient data."sv);
		}
		SECTION_SEPARATOR;
		REX::INFO("Finished startup tasks, enjoy your game!"sv);
		Settings::JSON::Holder::GetSingleton()->Release();
		break;
	default:
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []()
	{
		SKSE::PluginVersionData v{};

		v.PluginVersion(Plugin::VERSION);
		v.PluginName(Plugin::NAME);
		v.AuthorName("SeaSparrow"sv);
		v.UsesAddressLibrary();
		v.UsesUpdatedStructs();

		return v;
	}();
#endif

SKSE_PLUGIN_QUERY(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Plugin::NAME.data();
	a_info->version = Plugin::VERSION[0];

	if (a_skse->IsEditor()) {
		REX::CRITICAL("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	return true;
	}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface * a_skse)
{
	SKSE::InitInfo info;
	info.log = true;
	info.hook = true;
	info.trampoline = true;
	info.trampolineSize = 5 * 14u;
	
	SKSE::Init(a_skse, info);
	REX::INFO("Author: SeaSparrow"sv);
	SECTION_SEPARATOR;

	const auto ver = a_skse->RuntimeVersion();

#ifdef SKYRIM_GOG
	static constexpr std::array<REL::Version, 4> supported = 
	{
		SKSE::RUNTIME_SSE_1_6_1130,
		SKSE::RUNTIME_SSE_1_6_1170,
		SKSE::RUNTIME_SSE_1_6_1179,
		REL::Version(1, 6, 1179, 1) // no idea what this is still
	};
#else
	static constexpr std::array<REL::Version, 2> supported = 
	{
		SKSE::RUNTIME_SSE_1_7_104,
		SKSE::RUNTIME_SSE_1_7_99
	};	
#endif

	if ((ver < SKSE::RUNTIME_SSE_LATEST) && (!std::ranges::contains(supported, ver))) {
		REX::CRITICAL("Game Version: {}"sv, ver.string());
		REX::CRITICAL("Supported Versions:"sv);
		for (const auto& allowed : supported) {
			REX::CRITICAL("  - {}"sv, allowed.string());
		}
		REX::FAIL(
			fmt::format("You are using a version not supported by this plugin. Check the log at (Documents/My Games/Skyrim Special Edition/{}.log for more information."sv, Plugin::NAME)
		);
	}

	REX::INFO("Performing startup tasks..."sv);

	if (!Settings::INI::Read()) {
		REX::FAIL("Failed to load INI settings. Check the log for more information."sv);
	}
	SECTION_SEPARATOR;
	if (!Hooks::Install()) {
		REX::FAIL("Failed to install hooks. Check the log for more information."sv);
	}
	SECTION_SEPARATOR;

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(&MessageEventCallback);

	if (!Settings::JSON::Preload()) {
#ifdef NDEBUG
		REX::FAIL("Failed to preload JSON configs. Check the log for more information."sv);
#else
		REX::WARN("Potentially loaded with bad JSON configs due to debug mode."sv);
#endif
	}
	return true;
}