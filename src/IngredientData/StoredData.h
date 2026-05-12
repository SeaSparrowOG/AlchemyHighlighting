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
		[[nodiscard]] float GetBaseMagnitudeForEffect(RE::EffectSetting* a_effect) const;

	private:
		struct StoredEffectData
		{
			float                             _baseMagnitude = 0.0f;
			std::unordered_map<float, size_t> _counts = {};
		};

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
}