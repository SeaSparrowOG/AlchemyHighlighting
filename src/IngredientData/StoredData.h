#pragma once

namespace IngredientData
{
	enum class MagnitudeClassification
	{
		Base,
		Stronger,
		Weaker
	};

	class StoredData : public REX::Singleton<StoredData>
	{
	public:
		[[nodiscard]] bool  WarmCache();
		[[nodiscard]] bool  IsIgnoredEffect(RE::EffectSetting* a_effect) const;
		[[nodiscard]] float GetBaseMagnitudeForEffect(RE::EffectSetting* a_effect) const;

	private:
		struct StoredEffectData
		{
			float                             _baseMagnitude = 0.0f;
			std::unordered_map<float, size_t> _counts = {};
		};

		inline std::string GetFieldType(const Json::Value& a_field);

		inline static constexpr int PLUGIN_INDEX = 0;
		inline static constexpr int FORMID_INDEX = 1;
		static_assert(PLUGIN_INDEX >= 0, "PLUGIN_INDEX must be greater than or equal to 0.");
		static_assert(FORMID_INDEX >= 0, "FORMID_INDEX must be greater than or equal to 0.");
		static_assert(FORMID_INDEX != PLUGIN_INDEX, "PLUGIN_INDEX must be different from FORMID_INDEX.");

		// Used for form validation. Certain form types do NOT need to be retrieved through PO3's tweaks.
		template <typename T>
		struct form_type { static constexpr RE::FormType value = RE::FormType::None; };

		template <>
		struct form_type<RE::BGSKeyword> { static constexpr RE::FormType value = RE::FormType::Keyword; };
		template <>
		struct form_type<RE::BGSLocationRefType> { static constexpr RE::FormType value = RE::FormType::LocationRefType; };
		template<>
		struct form_type<RE::TESGlobal> { static constexpr RE::FormType value = RE::FormType::Global; };
		template<>
		struct form_type<RE::TESRace> { static constexpr RE::FormType value = RE::FormType::Race; };
		template<>
		struct form_type<RE::TESSound> { static constexpr RE::FormType value = RE::FormType::Sound; };
		template<>
		struct form_type<RE::TESObjectCELL> { static constexpr RE::FormType value = RE::FormType::Cell; };
		template<>
		struct form_type<RE::TESWorldSpace> { static constexpr RE::FormType value = RE::FormType::WorldSpace; };
		template<>
		struct form_type<RE::TESQuest> { static constexpr RE::FormType value = RE::FormType::Quest; };
		template<>
		struct form_type<RE::TESIdleForm> { static constexpr RE::FormType value = RE::FormType::Idle; };
		template<>
		struct form_type<RE::TESObjectANIO> { static constexpr RE::FormType value = RE::FormType::AnimatedObject; };
		template<>
		struct form_type<RE::TESImageSpaceModifier> { static constexpr RE::FormType value = RE::FormType::ImageAdapter; };
		template<>
		struct form_type<RE::BGSVoiceType> { static constexpr RE::FormType value = RE::FormType::VoiceType; };
		template<>
		struct form_type<RE::BGSMusicType> { static constexpr RE::FormType value = RE::FormType::MusicType; };
		template<>
		struct form_type<RE::BGSSoundDescriptorForm> { static constexpr RE::FormType value = RE::FormType::SoundRecord; };

		constexpr bool SupportsEDIDWithoutTweaks(RE::FormType type)
		{
			switch (type) {
			case RE::FormType::Keyword:
			case RE::FormType::LocationRefType:
			case RE::FormType::Global:
			case RE::FormType::Race:
			case RE::FormType::Sound:
			case RE::FormType::Cell:
			case RE::FormType::WorldSpace:
			case RE::FormType::Quest:
			case RE::FormType::Idle:
			case RE::FormType::AnimatedObject:
			case RE::FormType::ImageAdapter:
			case RE::FormType::VoiceType:
			case RE::FormType::MusicType:
			case RE::FormType::SoundRecord:
				return true;
			default:
				return false;
			}
		}

		enum class QueryResult
		{
			Success,          // Success
			FormatError,      //String is in an invalid format (EditorID while PO3's Tweaks is not present, FormID not hex, etc)
			FileNotFound,     // ESP/ESM/ESL missing
			FormNotInFile,    // Master exists, but form is not present
			WrongFormtype,    // Form exists in given file, but type is wrong.

			MissingPo3Tweaks, // EditorID query that requires PO3's tweaks but PO3's tweaks is not present.
			GenericFailure    // Catchall (might be missing data handler, cosmic ray, etc)
		};

		inline std::string QueryResultToString(QueryResult a_flag) {
			switch (a_flag) {
			case QueryResult::FileNotFound: return "FileNotFound";
			case QueryResult::FormatError: return "FormatError";
			case QueryResult::FormNotInFile: return "FormNotInFile";
			case QueryResult::GenericFailure: return "GenericFailure";
			case QueryResult::MissingPo3Tweaks: return "MissingPo3Tweaks";
			case QueryResult::WrongFormtype: return "WrongFormtype";
			default: return "Success";
			}
		}

		template <typename T>
		struct QueryData
		{
			std::optional<T*> value{ std::nullopt };
			QueryResult status{ QueryResult::Success };
		};


		template <typename T>
		QueryData<T> GetFormFromString(const std::string& a_str) {
			constexpr bool supportsEDID = SupportsEDIDWithoutTweaks(form_type<T>::value);

			auto response = QueryData<T>{};

			auto* dh = RE::TESDataHandler::GetSingleton();
			if (!dh) {
				response.status = QueryResult::GenericFailure;
				return response;
			}

			static auto* tweaks = REX::W32::GetModuleHandleW(L"po3_Tweaks.dll");

			auto parts = clib_util::string::split(a_str, "|");
			const RE::TESFile* mod;
			RE::FormID formID = 0;
			RE::TESForm* form = nullptr;
			T* castForm = nullptr;

			switch (parts.size()) {
			case 1:
				// EDID
				if constexpr (!supportsEDID) {
					if (!tweaks) {
						response.status = QueryResult::MissingPo3Tweaks;
						return response;
					}
				}
				form = RE::TESForm::LookupByEditorID<RE::TESForm>(a_str);
				if (!form) {
					response.value = nullptr;
					return response;
				}
				castForm = form->As<T>();
				if (!castForm) {
					response.status = QueryResult::WrongFormtype;
					return response;
				}
				response.value = castForm;
				return response;
			case 2:
				// FormID
				if (!clib_util::string::is_only_hex(parts[FORMID_INDEX], true)) {
					if (clib_util::string::is_only_hex(parts[PLUGIN_INDEX], true)) {
						formID = clib_util::string::to_num<RE::FormID>(parts[PLUGIN_INDEX], true);
						mod = dh->LookupModByName(parts[FORMID_INDEX]);
					}
					else {
						response.status = QueryResult::FormatError;
						return response;
					}
				}
				else {
					formID = clib_util::string::to_num<RE::FormID>(parts[FORMID_INDEX], true);
					mod = dh->LookupModByName(parts[PLUGIN_INDEX]);
				}
				if (!mod) {
					response.status = QueryResult::FileNotFound;
					return response;
				}

				form = dh->LookupForm(formID, mod->GetFilename());
				if (!form) {
					response.status = QueryResult::FormNotInFile;
					return response;
				}

				castForm = form->As<T>();
				if (!castForm) {
					response.status = QueryResult::WrongFormtype;
					return response;
				}

				response.value = castForm;
				return response;
			default:
				response.status = QueryResult::FormatError;
				break;
			}
			return response;
		}

		std::string MEMBER_IGNORED_EFFECTS = "ignoredeffects";

		std::unordered_set<RE::FormID>                           _ignoredEffects = {};
		std::unordered_map<RE::EffectSetting*, StoredEffectData> _cache = {};
	};

	[[nodiscard]] inline bool Initialize() {
		static auto* data = StoredData::GetSingleton();
		if (!data) {
			logger::critical("  >Failed to get internal cache manager."sv);
			return false;
		}
		return data->WarmCache();
	}

	[[nodiscard]] inline float GetAverageEffectMagnitude(RE::EffectSetting* a_effect) {
		static const auto* data = StoredData::GetSingleton();
		return data->GetBaseMagnitudeForEffect(a_effect);
	}
	[[nodiscard]] inline bool IsEffectIgnored(RE::EffectSetting* a_effect) {
		static const auto* data = StoredData::GetSingleton();
		return data->IsIgnoredEffect(a_effect);
	}
}