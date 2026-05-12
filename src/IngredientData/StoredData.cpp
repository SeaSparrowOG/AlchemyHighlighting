#include "StoredData.h"

namespace IngredientData
{
	bool StoredData::WarmCache() {
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
				const auto mag = effect->GetMagnitude();
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
			(void)effect;
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
		return true;
	}

	float StoredData::GetBaseMagnitudeForEffect(RE::EffectSetting* a_effect) const {
		auto it = _cache.find(a_effect);
		if (it != _cache.end()) {
			return (*it).second._baseMagnitude;
		}
		return 0.0f;
	}
}