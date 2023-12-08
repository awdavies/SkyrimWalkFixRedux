#include <polyhook2/Detour/x64Detour.hpp>
#include <polyhook2/Enums.hpp>
#include <polyhook2/Virtuals/VFuncSwapHook.hpp>
#include <polyhook2/ZydisDisassembler.hpp>

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= fmt::format("{}.log"sv, Plugin::NAME);
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);
	}
}

static std::unique_ptr<PLH::VFuncSwapHook> g_player_character_hooks;
static constexpr uint16_t g_speed_struct_offset = 8;
static PLH::VFuncMap g_player_originals;

#ifdef SKYRIM_POST_1_6_353
static constexpr uint64_t g_actor_state_vtable_offset = 0xc0;
#else
static constexpr uint64_t g_actor_state_vtable_offset = 0xb8;
#endif

const SKSE::MessagingInterface* g_messaging = nullptr;

[[nodiscard]] inline bool player_character_is_walking() noexcept {
	// This works better than checking the built-in function IsRunning() from the PC.
	// If we were to use that function it would recurse indefinitely in our use-case.
	return RE::PlayerCharacter::GetSingleton()->actorState1.walking != 0;
}

[[nodiscard]] inline float* ag_as_vec3(void* ag_struct) noexcept {
	return reinterpret_cast<float*>(reinterpret_cast<uint64_t>(ag_struct) + 16);
}

bool UpdateAnimationGraph(RE::PlayerCharacter* pc, void* animation_graph) {
	auto overridden_fn = PLH::FnCast(reinterpret_cast<void*>(g_player_originals[g_speed_struct_offset]), &UpdateAnimationGraph);
	auto result = overridden_fn(pc, animation_graph);
	if (result) {
		auto vec3 = ag_as_vec3(animation_graph);
		if (player_character_is_walking()) {
			vec3[1] = vec3[0] * 1.01f;
		} else {
			vec3[0] = vec3[1] / 1.01f;
		}
	}
	return result;
}

void AttachPlayerHooks() {
	auto player_character = RE::PlayerCharacter::GetSingleton();
	PLH::VFuncMap redirects;
	redirects.insert({ static_cast<uint16_t>(g_speed_struct_offset), reinterpret_cast<uint64_t>(&UpdateAnimationGraph)});

	g_player_character_hooks.reset(
		new PLH::VFuncSwapHook(
			reinterpret_cast<uint64_t>(player_character) + g_actor_state_vtable_offset,
			redirects,
			&g_player_originals
		)
	);
	if (g_player_character_hooks->hook()) {
		logger::info("Player character hooks established. :)");
	} else {
		logger::critical("Player character hooks could not be established. This mod will not run. :(");
	}
}

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message) {
	switch (message->type) {
	case SKSE::MessagingInterface::kNewGame:
	case SKSE::MessagingInterface::kPostLoadGame:
		{
			AttachPlayerHooks();
		}
	case SKSE::MessagingInterface::kPostLoad:
	default:
		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* q_skse, SKSE::PluginInfo* a_info) {
	logger::info("Querying...");
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = std::string(Plugin::NAME).c_str();
	a_info->version = 1;
	logger::info("Plugin query successful.");
	return true;
}

#ifdef SKYRIM_SUPPORT_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v{};
	v.PluginVersion(Plugin::VERSION);
	v.PluginName(Plugin::NAME);
#ifdef SKYRIM_POST_1_6_353
	v.CompatibleVersions({ SKSE::RUNTIME_1_6_640 });
	v.UsesAddressLibrary();
#else
	v.CompatibleVersions({ SKSE::RUNTIME_1_6_353 });
	v.UsesAddressLibrary(true);
#endif
	return v;
}();
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface * a_skse) {
	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());
	SKSE::Init(a_skse);
	g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, will not load");
		return false;
	}
	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);
	logger::info("Loading successful.");
	return true;
}
