#include "StoredData.h"

#include "Settings/JSON/JSONSettings.h"

namespace IngredientData
{
	bool StoredData::WarmCache() {
		using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;

		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			logger::critical("  >Failed to get the game's Data Handler."sv);
			return false;
		}

		auto& ingredients = dh->GetFormArray<RE::IngredientItem>();
		if (ingredients.empty()) {
			logger::warn("  >Ingredient array appears empty - likely an error."sv);
			return true;
		}

		for (auto* ingredient : ingredients) {
			if (!ingredient) {
				continue;
			}

			auto& effects = ingredient->effects;
			if (effects.empty()) {
				continue;
			}

			for (auto* effect : effects) {
				auto* base = effect ? effect->baseEffect : nullptr;
				if (!base) {
					continue;
				}

				auto found = _cache.find(base);
				auto mag = 1.0f;
				auto& flags = base->data.flags;
				if (flags.any(EffectFlag::kNoDuration) && flags.any(EffectFlag::kNoMagnitude)) {
					continue;
				}
				else if (flags.any(EffectFlag::kNoMagnitude)) {
					mag = static_cast<float>(effect->GetDuration());
				}
				else {
					mag = effect->GetMagnitude();
				}

				if (found != _cache.end()) {
					auto& currentFrequencies = found->second._counts;
					auto currentCounts = currentFrequencies.find(mag);
					if (currentCounts != currentFrequencies.end()) {
						currentCounts->second += 1u;
					}
					else {
						currentFrequencies.emplace(mag , 1u);
					}
				}
				else {
					StoredEffectData newData;
					newData._counts.emplace(mag, 1u);
					_cache.emplace(base, newData);
				}
			}
		}

		for (auto& [effect, data] : _cache) {
#ifdef NDEBUG
			(void)effect;
#else
			LOG_DEBUG("Effect: {} -> {}|{:06X}", clib_util::editorID::get_editorID(effect), (*effect->sourceFiles.array->begin())->GetFilename(), effect->GetFormID());
#endif
			auto& counts = data._counts;
			auto& baseMag = data._baseMagnitude;
			size_t lastLargest = 0u;
			for (const auto& [mag, frequency] : counts) {
				if (frequency > lastLargest) {
					lastLargest = frequency;
					baseMag = mag;
				}
			}
			data._counts.clear();
		}

		auto& jsonSettings = Settings::JSON::Holder::GetSingleton()->GetConfigs();
		if (jsonSettings.empty()) {
			logger::info("  >No runtime settings found."sv);
			return true;
		}

		logger::info("  >Parsing runtime settings..."sv);
		for (const auto& [name, config] : jsonSettings) {
			logger::info("    - {}"sv, name);
			if (!config.isObject()) {
				logger::critical("      Configuration is not an object - aborting load."sv);
				return false;
			}

			auto& ignoredEffects = config[MEMBER_IGNORED_EFFECTS];
			if (ignoredEffects) {
				if (ignoredEffects.isString()) {
					auto stringForm = ignoredEffects.asString();
					auto rawForm = GetFormFromString<RE::EffectSetting>(stringForm);
					if (rawForm.status == QueryResult::Success) {
						_ignoredEffects.insert(rawForm.value.value()->GetFormID());
					}
					else {
						switch (rawForm.status) {
						case QueryResult::FormatError:
						case QueryResult::FormNotInFile:
						case QueryResult::GenericFailure:
						case QueryResult::MissingPo3Tweaks:
						case QueryResult::WrongFormtype:
							logger::critical("      Failed to parse form {} with error: {}"sv, stringForm, QueryResultToString(rawForm.status));
							return false;
						default:
							break;
						}
					}
				}
				else if (ignoredEffects.isArray()) {
					for (const auto& arrayVal : ignoredEffects) {
						auto stringForm = arrayVal.asString();
						auto rawForm = GetFormFromString<RE::EffectSetting>(stringForm);
						if (rawForm.status == QueryResult::Success) {
							_ignoredEffects.insert(rawForm.value.value()->GetFormID());
						}
						else {
							switch (rawForm.status) {
							case QueryResult::FormatError:
							case QueryResult::FormNotInFile:
							case QueryResult::GenericFailure:
							case QueryResult::MissingPo3Tweaks:
							case QueryResult::WrongFormtype:
								logger::critical("      Failed to parse form {} with error: {}"sv, stringForm, QueryResultToString(rawForm.status));
								return false;
							default:
								break;
							}
						}
					}
				}
				else {
					logger::critical("      IgnoredEffects is neither an array nor a string."sv);
					return false;
				}
			}
		}
		return true;
	}

	bool StoredData::IsIgnoredEffect(RE::EffectSetting* a_effect) const {
		return _ignoredEffects.contains(a_effect->GetFormID());
	}

	float StoredData::GetBaseMagnitudeForEffect(RE::EffectSetting* a_effect) const {
		auto it = _cache.find(a_effect);
		if (it != _cache.end()) {
			return (*it).second._baseMagnitude;
		}
		return 0.0f;
	}
}