extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
#ifndef DEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= Version::PROJECT;
	*path += ".log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef DEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}

bool is_ok([[maybe_unused]] RE::TESShout* shout) {
	auto player = RE::PlayerCharacter::GetSingleton();
	return player->HasPerk(RE::TESForm::LookupByID<RE::BGSPerk>(0x018E6B));
}

class SpendSoulHook
{
public:
	static void Hook()
	{
		_ProcessMessage = REL::Relocation<uintptr_t>(REL::ID(RE::VTABLE_MagicMenu[0])).write_vfunc(0x4, ProcessMessage);
	}

private:
	struct MagicItemData
	{
		void* vftable;
		RE::BSString text;
		RE::SpellItem* spel;
		RE::GFxValue gval;
		void* field_38;
		void* field_40;
	};
	static_assert(sizeof(MagicItemData) == 0x48);

	struct MagicMenu_struct30
	{
		RE::GFxValue::ObjectInterface* InventoryLists;
		RE::GFxValue::ObjectInterface* obj_inter;
		void* field_10;
		void* field_18;
		RE::GFxValue::ObjectInterface* obj_inter_1;
		void* field_28;
		void* field_30;
		RE::BSTArray<MagicItemData*> array;
		char field_50;
	};
	static_assert(sizeof(MagicMenu_struct30) == 0x58);

	static RE::UI_MESSAGE_RESULTS ProcessMessage(RE::MagicMenu* menu, RE::UIMessage& a_message)
	{
		if (a_message.type.all(RE::UI_MESSAGE_TYPE::kUserEvent)) {
			if (auto data = static_cast<RE::BSUIMessageData*>(a_message.data)) {
				if (data->fixedStr == RE::UserEvents::GetSingleton()->xButton) {
					if (auto selected_MagicItemData = _generic_foo_<51211, MagicItemData*(void* a)>::eval(menu->unk30)) {
						if (auto shout_ = selected_MagicItemData->spel; shout_ && shout_->formType == RE::FormType::Shout) {
							if (auto first_unknown_word = _generic_foo_<22900, int(void* a1)>::eval(shout_);
								first_unknown_word >= 0) {
								auto shout = shout_->As<RE::TESShout>();
								auto it_words = shout->variations;
								auto i = first_unknown_word + 1;
								bool v13 = 0;
								do {
									auto Word_0 = it_words->word;
									if (it_words->word->GetKnown() && (Word_0->formFlags & 0x10000) == 0)
										v13 = true;
									++it_words;
									--i;
								} while (i);

								if (v13 && RE::PlayerCharacter::GetSingleton()->GetActorValue(RE::ActorValue::kDragonSouls) > 0) {
									if (!is_ok(shout)) {
										FenixUtils::notification("Lox.");
										return RE::UI_MESSAGE_RESULTS::kHandled;
									}
								}
							}
						}
					}
				}
			}
		}

		return _ProcessMessage(menu, a_message);
	}

	static inline REL::Relocation<decltype(ProcessMessage)> _ProcessMessage;
};

static void SKSEMessageHandler(SKSE::MessagingInterface::Message* message)
{
	switch (message->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		SpendSoulHook::Hook();

		break;
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	auto g_messaging = reinterpret_cast<SKSE::MessagingInterface*>(a_skse->QueryInterface(SKSE::LoadInterface::kMessaging));
	if (!g_messaging) {
		logger::critical("Failed to load messaging interface! This error is fatal, plugin will not load.");
		return false;
	}

	logger::info("loaded");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(1 << 10);

	g_messaging->RegisterListener("SKSE", SKSEMessageHandler);

	return true;
}
