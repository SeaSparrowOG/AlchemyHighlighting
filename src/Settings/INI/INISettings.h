#pragma once

namespace Settings
{
	namespace INI
	{
		bool Read();

		class Holder :
			public REX::Singleton<Holder>
		{
		public:
			bool StoreSettings();
			void DumpSettings();

			template <typename T>
			std::optional<T> GetStoredSetting(const std::string& a_settingName) {
				if constexpr (std::is_same_v<T, float>) {
					auto it = floatSettings.find(a_settingName);
					if (it != floatSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, std::string>) {
					auto it = stringSettings.find(a_settingName);
					if (it != stringSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, long>) {
					auto it = longSettings.find(a_settingName);
					if (it != longSettings.end()) return it->second;
				}
				else if constexpr (std::is_same_v<T, bool>) {
					auto it = boolSettings.find(a_settingName);
					if (it != boolSettings.end()) return it->second;
				}
				else {
					static_assert(always_false<T>, "Called GetStoredSetting with unsupported type.");
				}
				return std::nullopt;
			}

		private:
			std::map<std::string, long>        longSettings;
			std::map<std::string, bool>        boolSettings;
			std::map<std::string, float>       floatSettings;
			std::map<std::string, std::string> stringSettings;

			bool OverrideSettings();
		};

		inline static constexpr const std::uint8_t EXPECTED_COUNT = 11u;
		inline static constexpr const std::string_view COLOR_BENEFICIAL_STRONG = "Colors|iBeneficialStrong"sv;
		inline static constexpr const std::string_view COLOR_BENEFICIAL_WEAK = "Colors|iBeneficialWeak"sv;
		inline static constexpr const std::string_view COLOR_HARMFUL_STRONG = "Colors|iHarmfullStrong"sv;
		inline static constexpr const std::string_view COLOR_HARMFUL_WEAK = "Colors|iHarmfullWeak"sv;

		inline static constexpr const std::string_view ENABLED_MENUS_ALCHEMY = "EnabledMenus|bEnableInAlchemy"sv;
		inline static constexpr const std::string_view ENABLED_MENUS_INVENTORY = "EnabledMenus|bEnableInInventory"sv;
		inline static constexpr const std::string_view ENABLED_MENUS_CONTAINER = "EnabledMenus|bEnableInContainer"sv;
		inline static constexpr const std::string_view ENABLED_MENUS_BARTER = "EnabledMenus|bEnableInBarter"sv;
		inline static constexpr const std::string_view ENABLED_MENUS_GIFT = "EnabledMenus|bEnableInGift"sv;

		inline static constexpr const std::string_view ENABLE_UNKNOWN = "General|bEnableForUnknownEffects"sv;
		inline static constexpr const std::string_view SIMPLE_INDICATORS = "General|bUseSimpleIndicators"sv;

		inline static constexpr const std::array<std::string_view, EXPECTED_COUNT> EXPECTED_SETTINGS = {
			COLOR_BENEFICIAL_STRONG,
			COLOR_BENEFICIAL_WEAK,
			COLOR_HARMFUL_STRONG,
			COLOR_HARMFUL_WEAK,

			ENABLED_MENUS_ALCHEMY,
			ENABLED_MENUS_INVENTORY,
			ENABLED_MENUS_CONTAINER,
			ENABLED_MENUS_BARTER,
			ENABLED_MENUS_GIFT,

			ENABLE_UNKNOWN,
			SIMPLE_INDICATORS
		};

		template <typename T>
		std::optional<T> GetSetting(const std::string& a_settingName) {
			static auto* holder = Holder::GetSingleton();
			return holder->GetStoredSetting<T>(a_settingName);
		}

		inline static bool ShouldInstallHook(const std::string& a_setting) {
			static auto* holder = Holder::GetSingleton();
			auto response = holder->GetStoredSetting<bool>(a_setting);
			return response.value_or(true);
		}
	}
}