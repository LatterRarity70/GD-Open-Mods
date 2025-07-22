#include <imgui-cocos-with-addons/include.hpp>
#include <regex>
using namespace geode::prelude;

namespace fs = std::filesystem;
static std::error_code fs_err;

static auto INFO_MD_BODY = geode::getMod()->getDetails().value_or("xd");
static auto INFO_REPO = matjson::Value();

//helpers
#define INFO_REPO_KEY_DUMP(key) string::replace(INFO_REPO[key].dump(), "\"", "")
std::string IsoToReadable(const std::string& iso_time, const char* fmt = "%Y.%m.%d - %H:%M", bool local_time = true) {
	std::tm tm = {};
	std::istringstream ss(iso_time);
	ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
	if (ss.fail()) return "bad date";

	std::time_t time = std::mktime(&tm);
	tm = local_time ? *std::localtime(&time) : *std::gmtime(&time);

	return (std::ostringstream() << std::put_time(&tm, fmt)).str();
}
std::string FormatFileSize(uint64_t bytes) {
	const char* units[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
	double size = static_cast<double>(bytes);
	int magnitude = 0;

	while (size >= 1024 && magnitude < 6) {
		size /= 1024;
		magnitude++;
	}

	std::ostringstream oss;
	oss << std::fixed << std::setprecision(2) << size << " " << units[magnitude];

	std::string result = oss.str();
	size_t dot_pos = result.find(".00");
	if (dot_pos != std::string::npos) {
		result.erase(dot_pos, 3);
	}

	return result;
}

inline static auto REPO_LIST = std::string();
inline static auto LOADED_REPOS = std::map<std::string, matjson::Value>();
inline static auto REPO_WAS_LOADED = std::map<std::string, bool>();

void RenderRepoList(std::string list = REPO_LIST) {

	ImGui::BeginChild("search-REPO_LISTinp-box", { 0, 0 }, ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY);
	static std::string SEARCH_FILTER;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::InputTextWithHint("##search-inp", "Search filter...\t", &SEARCH_FILTER);
	ImGui::EndChild();

	static std::string SEARCH_NEEDLES;
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	ImGui::TextWrapped("%s", SEARCH_FILTER.size() ? SEARCH_NEEDLES.c_str() : "No filter keywords...");

	for (auto repo_gdstr : string::split(list, "\n")) {
		auto repo = std::string(repo_gdstr.c_str()); //i never will trust gd::string
		if (repo.size() < 2) continue;

		if (SEARCH_FILTER.size()) {
			auto q = string::toLower(SEARCH_FILTER);
			auto haystack = string::toLower(repo + LOADED_REPOS[repo].dump());
			auto needles = string::split(q, " "); //split q for space
			for (auto a : { ",", "+", "_", "-", "/" }) { //and more...
				auto splitten = string::split(q, a);
				if (string::contains(q, a)) needles.insert(needles.end(), splitten.begin(), splitten.end());
			}
			SEARCH_NEEDLES = fmt::format("Keywords: {}", string::join(needles, ", "));
			if (not string::containsAll(haystack, needles)) continue;
		}

		if (not REPO_WAS_LOADED[repo]) {
			REPO_WAS_LOADED[repo] = true;

			log::debug("{}:{}", __LINE__, repo);

			auto listener = new EventListener<web::WebTask>();
			listener->bind(
				[=](web::WebTask::Event* e) mutable {
					if (web::WebResponse* res = e->getValue()) {
						LOADED_REPOS[repo] = res->json().unwrapOrDefault();

						auto& description = LOADED_REPOS[repo]["description"];
						if (not description.isString()) description = "No description provided.";

						if (listener) delete listener;
					}
				}
			);
			auto req = web::WebRequest().certVerification(false);

			log::debug("{}:{}", __LINE__, repo);
			listener->setFilter(req.get("https://ungh-exp.vercel.app/repos/" + repo));

			return;
		};

		static std::map<std::string, bool> hovered;
		auto flags = ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY;
		if (hovered[repo]) {
			flags |= ImGuiChildFlags_Borders;
			flags |= ImGui::IsMouseDown(ImGuiMouseButton_Left) ? ImGuiChildFlags_None : ImGuiChildFlags_FrameStyle;
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				INFO_REPO = LOADED_REPOS[repo];
				INFO_MD_BODY = "Loading...";

				auto listener = new EventListener<web::WebTask>();
				listener->bind(
					[=](web::WebTask::Event* e) mutable {
						if (web::WebResponse* res = e->getValue()) {

							INFO_MD_BODY = res->json().unwrapOrDefault()["markdown"].asString(
							).unwrapOr("Failed to parse.. " + res->string().unwrapOr("Uh oh!"));

							if (listener) delete listener;
						}
					}
				);
				auto req = web::WebRequest().certVerification(false);
				listener->setFilter(req.get("https://ungh-exp.vercel.app/repos/" + repo + "/readme"));
			}
		}
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().WindowPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
		ImGui::BeginChild((repo + " Repo Item").c_str(), { 0, 0 }, flags);
		ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
		ImGui::PopStyleVar(2);
		{
			hovered[repo] = ImGui::IsWindowHovered();

			auto title = LOADED_REPOS[repo]["name"].asString().unwrapOr(repo);
			if (auto slash_spl = string::split(repo, "/"); slash_spl.size() > 1) {
				ImGui::Markdown("**" + string::replace(slash_spl[1].c_str(), "-", " ") + "**");
				ImGui::TextDisabled("by %s", slash_spl[0].c_str());
			}
			else ImGui::Markdown("**" + title + "**");

			ImGui::Markdown(LOADED_REPOS[repo]["description"].asString().unwrapOr("Loading..."));
		}
		ImGui::EndChild();
	}

}

void Browser(bool reload = false) {

	if (reload) REPO_LIST = "";
	if (reload) LOADED_REPOS.clear();
	if (reload) REPO_WAS_LOADED.clear();
	
	static bool LIST_WAS_LOADED;
	if (reload) LIST_WAS_LOADED = false;
	if (not LIST_WAS_LOADED) {
		LIST_WAS_LOADED = true;

		auto listener = new EventListener<web::WebTask>();
		listener->bind(
			[=](web::WebTask::Event* e) mutable {
				if (web::WebResponse* res = e->getValue()) {
					REPO_LIST = res->string().unwrapOr("Uh oh!");
					if (listener) delete listener;
				}
			}
		);
		auto req = web::WebRequest().certVerification(false);
		listener->setFilter(req.get("https://raw.githubusercontent.com/user95401/GD-Open-Mods/refs/heads/main/_list.txt"));
		
		return;
	};

	RenderRepoList();

	ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ResizeGripActive));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ResizeGripHovered));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ResizeGrip));
	if (ImGui::Button("Add Repository", {-1, 0})) {
		web::openLinkInBrowser("https://github.com/user95401/GD-Open-Mods/issues/1");
	}
	ImGui::PopStyleColor(3);
}

void Favorites(bool reload = false) {
	RenderRepoList(getMod()->getSavedValue<std::string>("favorites-list"));
}

void Installed(bool reload = false) {
	RenderRepoList(getMod()->getSavedValue<std::string>("installed-list"));
}

inline static auto LOADED_RELEASES = std::map<std::string, matjson::Value>();
void RELEASES(bool reload = false) {
	auto repo = INFO_REPO_KEY_DUMP("full_name");
	if (!LOADED_RELEASES.contains(repo)) ImGui::Markdown("Loading...");

	static std::map<std::string, bool> RELEASES_WAS_LOADED;
	if (reload) RELEASES_WAS_LOADED.clear();
	if (reload) LOADED_RELEASES.erase(repo);
	if (not RELEASES_WAS_LOADED[repo]) {
		RELEASES_WAS_LOADED[repo] = true;

		auto listener = new EventListener<web::WebTask>();
		listener->bind(
			[=](web::WebTask::Event* e) mutable {
				if (web::WebResponse* res = e->getValue()) {
					LOADED_RELEASES[repo] = res->json().unwrapOrDefault();
					if (listener) delete listener;
				}
			}
		);
		auto req = web::WebRequest().certVerification(false);
		listener->setFilter(req.get("https://ungh-exp.vercel.app/repos/" + repo + "/releases"));

		return;
	};

	static bool showDownloadProgress;
	static float downloadProgress;
	static std::string statusText;

	for (auto release : LOADED_RELEASES[repo]) {
#define RELEASE_KEY_DUMP(key) string::replace(release[key].dump(), "\"", "")
		ImGui::BeginChild(
			(repo + " release" + RELEASE_KEY_DUMP("id")).c_str(), 
			{ 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding| ImGuiChildFlags_AutoResizeY
		);

		ImGui::MDText(
			+ "## " + RELEASE_KEY_DUMP("name")
			+ "\n**" + release["author"]["login"].asString().unwrapOr("was")
			+ "** published at " + IsoToReadable(RELEASE_KEY_DUMP("published_at"))
			+ " with " + RELEASE_KEY_DUMP("tag_name") + " tag."
		);
		ImGui::Separator();
		ImGui::MDText(release["body"].asString().unwrapOrDefault());
		ImGui::Separator();

		for (auto asset : release["assets"]) {
#define ASSET_KEY_DUMP(key) string::replace(asset[key].dump(), "\"", "")

			auto file = ASSET_KEY_DUMP("name");
			auto path = dirs::getModsDir() / file;

			auto ext = fs::path(file).extension();
			if (string::containsAny(ext.string(), {".zip", ".apk"})) {
				path = dirs::getModConfigDir() / "geode.texture-loader" / "packs" / file;
			}

			auto instl = getMod()->getSavedValue<std::string>("installed-list");
			auto installed = string::contains(instl, repo);

			ImGui::BeginChild(
				("##chILD-" + file).c_str(),
				{ 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY
			);

			auto exists = fs::file_size(path, fs_err) == asset["size"].asInt().unwrapOr(0);
			if (not exists) ImGui::BeginDisabled();
			if (ImGui::Checkbox(("##chkb" + file).c_str(), &exists)) {
				getMod()->setSavedValue(
					"installed-list",
					string::replace(instl, repo, "")
				);
				fs::remove_all(path, fs_err);
			};
			if (not exists) ImGui::EndDisabled();

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);

			if (ImGui::Button(file.c_str())) {
				showDownloadProgress = true;
				downloadProgress = 0.0f;
				statusText = "Downloading...";

				auto listener = new EventListener<web::WebTask>();
				listener->bind(
					[=](web::WebTask::Event* e) mutable {
						if (web::WebResponse* res = e->getValue()) {
							if (auto err = res->into(path).err()) log::error("{}", err);
							showDownloadProgress = false;

							if (not installed) getMod()->setSavedValue(
								"installed-list",
								(instl + "\n" + repo)
							);

							if (listener) delete listener;
						}
						else if (web::WebProgress* p = e->getProgress()) {
							showDownloadProgress = true;
							downloadProgress = p->downloadProgress().value_or(0.f) / 100.f;
							statusText = fmt::format("Downloading {}", ASSET_KEY_DUMP("name"));
						}
					}
				);
				auto req = web::WebRequest().certVerification(false);
				listener->setFilter(req.get(ASSET_KEY_DUMP("browser_download_url")));
			}

			ImGui::SameLine();
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() - ImGui::GetStyle().ItemSpacing.x);
			
			ImGui::BeginDisabled();

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::InputText(
				("##emptinp" + file).c_str(), "",
				ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_ElideLeft
			);

			auto astinf = FormatFileSize(asset["size"].asInt().unwrapOr(0)) +
				", " + ASSET_KEY_DUMP("download_count") + " downloads";

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			ImGui::InputText(
				"", &astinf,
				ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_ElideLeft
			);

			ImGui::EndDisabled();

			ImGui::EndChild();
		}

		ImGui::EndChild();
	}

	if (showDownloadProgress) {

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

		if (ImGui::BeginPopupModal(statusText.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::ProgressBar(downloadProgress, { ImGui::CalcTextSize(statusText.c_str()).x, 0.f});
			ImGui::EndPopup();
		}

		if (!ImGui::IsPopupOpen(statusText.c_str())) ImGui::OpenPopup(statusText.c_str());
	}
}

void MainView() {
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
	ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar;
	window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 18.f, 18.f });
	if (ImGui::Begin(GEODE_MOD_ID, nullptr, window_flags))
	{
		ImGui::PopStyleVar();

		ImGui::BeginChild("Listbox",
			{ ImGui::GetContentRegionAvail().x * 0.35f, 0 },
			ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoDecoration
		); {
			ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Fix wrapping after window resize
			if (ImGui::BeginTabBar("TABS", ImGuiTabBarFlags_DrawSelectedOverline | ImGuiTabBarFlags_Reorderable)) {
				if (ImGui::BeginTabItem("BROWSER")) {
					auto active = ImGui::IsItemActivated(); //reload
					ImGui::BeginChild("Browser""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
					ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Fix wrapping after window resize
					Browser(active);
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("FAVORITES")) {
					ImGui::BeginChild("Favorites""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
					ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Fix wrapping after window resize
					Favorites();
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("INSTALLED")) {
					ImGui::BeginChild("Installed""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
					ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Fix wrapping after window resize
					Installed();
					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("EXTRA", nullptr)) {
					ImGui::BeginChild("EXTRA""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
					ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x); // Fix wrapping after window resize

					auto debug_windows = getMod()->getSavedValue<bool>("debug-windows", false);
					ImGui::Checkbox("Render debug windows", &debug_windows) ?  getMod()->setSavedValue(
						"debug-windows", debug_windows
					) : debug_windows;

					static auto editoropen = false;
					if (ImGui::Checkbox("Style editor", &editoropen) or ImGui::IsAnyItemActive()) {
						ImGui::SaveStylesTo(string::pathToString(
							getMod()->getSaveDir() / ".styles"
						).c_str());
					};
					ImGui::SameLine();
					if (editoropen) {
						ImGui::TextDisabled(" <- Turn off to save changes!");
						ImGui::ShowStyleEditor();
					}
					else if (ImGui::TextLink(" [RESET STYLE]")) {
						file::writeString(getMod()->getSaveDir() / ".styles", "");
						game::restart(true);
					};

					ImGui::EndChild();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		};
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Info", {}, ImGuiChildFlags_FrameStyle);
		{

			ImGui::BeginChild("Top Line", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
			{

				auto title = INFO_REPO_KEY_DUMP("full_name");
				if (auto slash_spl = string::split(title, "/"); slash_spl.size() > 1) {
					ImGui::PushFont(ImGui::MDFont_H2);
					ImGui::Text("%s", string::replace(slash_spl[1].c_str(), "-", " ").c_str());

					ImGui::SameLine(0, 0);

					auto hh = ImGui::GetTextLineHeight();
					ImGui::PopFont();
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (hh - ImGui::GetTextLineHeight()) / 2);

					ImGui::TextDisabled(" by %s", slash_spl[0].c_str());
					ImGui::Separator();
				}
				else ImGui::Markdown("## " + title);

				ImGui::Markdown(INFO_REPO["description"].asString().unwrapOr("Open some repo first :3"));

				ImGui::Markdown(
					+"**Stars:** " + INFO_REPO_KEY_DUMP("stargazers_count")
					+ " | **Watchers:** " + INFO_REPO_KEY_DUMP("watchers_count")
					+ " |  **Forks:** " + INFO_REPO_KEY_DUMP("forks")
				);
			};
			ImGui::EndChild();

			auto blineh = 54.f;

			//
			ImGui::BeginChild("midbox",
				{ 0, ImGui::GetContentRegionAvail().y - ImGui::CalcItemSize({ 0, blineh }, 0, 69).y },
				ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoDecoration
			); {

				if (ImGui::BeginTabBar("midbox tabs", ImGuiTabBarFlags_DrawSelectedOverline)) {

					if (ImGui::BeginTabItem("README")) {
						ImGui::BeginChild("READMEbox", { 0, 0 }, ImGuiChildFlags_FrameStyle);

						ImGui::MDText(INFO_MD_BODY);

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("RELEASES")) {
						auto activated = ImGui::IsItemActivated();
						ImGui::BeginChild("RELEASESbox", { 0, 0 }, ImGuiChildFlags_FrameStyle);

						RELEASES(activated);

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}

			};
			ImGui::EndChild();

			ImGui::BeginChild("Bottom Line", { 0, blineh - 4 }, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
			{
				ImGui::Markdown(
					+"[" + INFO_REPO_KEY_DUMP("full_name") + "](https://github.com/" + INFO_REPO_KEY_DUMP("full_name") + ")"
				);
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Unfavorite It").x - ImGui::GetStyle().FramePadding.x * 2);

				auto fl = getMod()->getSavedValue<std::string>("favorites-list");
				auto infav = string::contains(fl, INFO_REPO_KEY_DUMP("full_name"));
				if (ImGui::SmallButton(infav
					? "Unfavorite It"
					: "  Favorite It  ")) {
					getMod()->setSavedValue(
						"favorites-list",
						infav
						? string::replace(fl, INFO_REPO_KEY_DUMP("full_name"), "")
						: (fl + "\n" + INFO_REPO_KEY_DUMP("full_name"))
					);
				}
			};
			ImGui::EndChild();
		};
		ImGui::EndChild();

	}
	ImGui::End();
}

inline void wLoaded() {
    ImGuiCocos::get().setup(
        [] {
            ImGuiIO& io = ImGui::GetIO();

            auto RobotoItalic       = geode::Mod::get()->getResourcesDir() / "Roboto-Italic.ttf";
            auto RobotoBold         = geode::Mod::get()->getResourcesDir() / "Roboto-Bold.ttf";
            auto RobotoBoldItalic   = geode::Mod::get()->getResourcesDir() / "Roboto-BoldItalic.ttf";
            auto RobotoRegular      = geode::Mod::get()->getResourcesDir() / "Roboto-Regular.ttf";

            auto fsize      = 36.0f;
            auto hsmltp     = 1.05f;
            auto hsappmltp  = 0.10f;
            ImGui::MDFont_H1 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 6)));
            ImGui::MDFont_H2 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 5)));
            ImGui::MDFont_H3 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 4)));
            ImGui::MDFont_H4 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 3)));
            ImGui::MDFont_H5 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 2)));
            ImGui::MDFont_H6 = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize * (hsmltp + (hsappmltp * 1)));

            ImGui::MDFont_Italic        = io.Fonts->AddFontFromFileTTF(RobotoItalic.string().c_str(), fsize);
            ImGui::MDFont_Bold          = io.Fonts->AddFontFromFileTTF(RobotoBold.string().c_str(), fsize);
            ImGui::MDFont_ItalicBold    = io.Fonts->AddFontFromFileTTF(RobotoBoldItalic.string().c_str(), fsize);

            ; io.FontDefault            = io.Fonts->AddFontFromFileTTF(RobotoRegular.string().c_str(), fsize);

            io.FontAllowUserScaling = true;

            io.MouseDoubleClickTime = 0.5f;
            io.ConfigScrollbarScrollByPage = true;

            io.ConfigWindowsResizeFromEdges = true;

            io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
            
			{
				// Discord (Dark) style by BttrDrgn from ImThemes
				ImGuiStyle& style = ImGui::GetStyle();

				style.Alpha = 1.0f;
				style.DisabledAlpha = 0.6000000238418579f;
				style.WindowPadding = ImVec2(8.0f, 8.0f);
				style.WindowRounding = 0.0f;
				style.WindowBorderSize = 1.0f;
				style.WindowMinSize = ImVec2(32.0f, 32.0f);
				style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
				style.WindowMenuButtonPosition = ImGuiDir_Left;
				style.ChildRounding = 0.0f;
				style.ChildBorderSize = 1.0f;
				style.PopupRounding = 0.0f;
				style.PopupBorderSize = 1.0f;
				style.FramePadding = ImVec2(4.0f, 3.0f);
				style.FrameRounding = 0.0f;
				style.FrameBorderSize = 0.0f;
				style.ItemSpacing = ImVec2(8.0f, 4.0f);
				style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
				style.CellPadding = ImVec2(4.0f, 2.0f);
				style.IndentSpacing = 21.0f;
				style.ColumnsMinSpacing = 6.0f;
				style.ScrollbarSize = 14.0f;
				style.ScrollbarRounding = 0.0f;
				style.GrabMinSize = 10.0f;
				style.GrabRounding = 0.0f;
				style.TabRounding = 0.0f;
				style.TabBorderSize = 0.0f;
				//style.TabMinWidthForCloseButton = 0.0f;
				style.ColorButtonPosition = ImGuiDir_Right;
				style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
				style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

				style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 1.0f);
				style.Colors[ImGuiCol_WindowBg] = ImVec4(0.2117647081613541f, 0.2235294133424759f, 0.2470588237047195f, 1.0f);
				style.Colors[ImGuiCol_ChildBg] = ImVec4(0.1843137294054031f, 0.1921568661928177f, 0.2117647081613541f, 1.0f);
				style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0784313753247261f, 0.0784313753247261f, 0.0784313753247261f, 0.9399999976158142f);
				style.Colors[ImGuiCol_Border] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
				style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				style.Colors[ImGuiCol_FrameBg] = ImVec4(0.3098039329051971f, 0.3294117748737335f, 0.3607843220233917f, 1.0f);
				style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3098039329051971f, 0.3294117748737335f, 0.3607843220233917f, 1.0f);
				style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3450980484485626f, 0.3960784375667572f, 0.9490196108818054f, 1.0f);
				style.Colors[ImGuiCol_TitleBg] = ImVec4(0.1843137294054031f, 0.1921568661928177f, 0.2117647081613541f, 1.0f);
				style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.125490203499794f, 0.1333333402872086f, 0.1450980454683304f, 1.0f);
				style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.125490203499794f, 0.1333333402872086f, 0.1450980454683304f, 1.0f);
				style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.125490203499794f, 0.1333333402872086f, 0.1450980454683304f, 1.0f);
				style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.01960784383118153f, 0.01960784383118153f, 0.01960784383118153f, 0.5299999713897705f);
				style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3098039329051971f, 1.0f);
				style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.407843142747879f, 0.407843142747879f, 0.407843142747879f, 1.0f);
				style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.5098039507865906f, 0.5098039507865906f, 0.5098039507865906f, 1.0f);
				style.Colors[ImGuiCol_CheckMark] = ImVec4(0.2313725501298904f, 0.6470588445663452f, 0.364705890417099f, 1.0f);
				style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
				style.Colors[ImGuiCol_Button] = ImVec4(0.3098039329051971f, 0.3294117748737335f, 0.3607843220233917f, 1.0f);
				style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.407843142747879f, 0.4274509847164154f, 0.4509803950786591f, 1.0f);
				style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.125490203499794f, 0.1333333402872086f, 0.1450980454683304f, 1.0f);
				style.Colors[ImGuiCol_Header] = ImVec4(0.3098039329051971f, 0.3294117748737335f, 0.3607843220233917f, 1.0f);
				style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.407843142747879f, 0.4274509847164154f, 0.4509803950786591f, 1.0f);
				style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.407843142747879f, 0.4274509847164154f, 0.4509803950786591f, 1.0f);
				style.Colors[ImGuiCol_Separator] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4980392158031464f, 0.5f);
				style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 0.7799999713897705f);
				style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.09803921729326248f, 0.4000000059604645f, 0.7490196228027344f, 1.0f);
				style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.2000000029802322f);
				style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.6700000166893005f);
				style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 0.949999988079071f);
				style.Colors[ImGuiCol_Tab] = ImVec4(0.1843137294054031f, 0.1921568661928177f, 0.2117647081613541f, 1.0f);
				style.Colors[ImGuiCol_TabHovered] = ImVec4(0.2352941185235977f, 0.2470588237047195f, 0.2705882489681244f, 1.0f);
				style.Colors[ImGuiCol_TabActive] = ImVec4(0.2588235437870026f, 0.2745098173618317f, 0.3019607961177826f, 1.0f);
				style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.06666667014360428f, 0.1019607856869698f, 0.1450980454683304f, 0.9724000096321106f);
				style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1333333402872086f, 0.2588235437870026f, 0.4235294163227081f, 1.0f);
				style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6078431606292725f, 0.6078431606292725f, 0.6078431606292725f, 1.0f);
				style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.3450980484485626f, 0.3960784375667572f, 0.9490196108818054f, 1.0f);
				style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.3450980484485626f, 0.3960784375667572f, 0.9490196108818054f, 1.0f);
				style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.3607843220233917f, 0.4000000059604645f, 0.4274509847164154f, 1.0f);
				style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.1882352977991104f, 0.1882352977991104f, 0.2000000029802322f, 1.0f);
				style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.3098039329051971f, 0.3098039329051971f, 0.3490196168422699f, 1.0f);
				style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2274509817361832f, 0.2274509817361832f, 0.2470588237047195f, 1.0f);
				style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
				style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.05999999865889549f);
				style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.05098039284348488f, 0.4196078479290009f, 0.8588235378265381f, 1.0f);
				style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.3450980484485626f, 0.3960784375667572f, 0.9490196108818054f, 1.0f);
				style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.2588235437870026f, 0.5882353186607361f, 0.9764705896377563f, 1.0f);
				style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.699999988079071f);
				style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.2000000029802322f);
				style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000011920929f, 0.800000011920929f, 0.800000011920929f, 0.3499999940395355f);
			
			}

			ImGui::GetStyle().TabBarBorderSize = 2.0f;
			ImGui::GetStyle().Colors[ImGuiCol_FrameBg] = ImVec4(0.36f, 0.38f, 0.40f, 0.25f);
			ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.35f);

			ImGui::LoadStyleFrom(
				string::pathToString(getMod()->getSaveDir() / ".styles").c_str()
			);

			ImGuiCocos::get().setVisible(false);
        }
    ).draw(
        [] {

            //ui was developed at this resolution...
            const ImVec2 target_size(1920.0f, 1080.0f);

            auto win_size = ImGui::GetMainViewport()->Size;
            float scale_x = win_size.x / target_size.x;
            float scale_y = win_size.y / target_size.y;
            float scale = std::min(scale_x, scale_y);

            ImGui::GetIO().FontGlobalScale = (scale);
			ImGui::GetIO().DisplayFramebufferScale = { scale_x, scale_y };

			ImGui::GetIO().MouseSource = ImGuiMouseSource_TouchScreen;

			//swipe scroll
			ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
			if (ImGui::GetIO().MouseDownDuration[0] > 0.1f) {
				auto scroll = ccp(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y);
				scroll = scroll / 100.f;
				ImGui::GetIO().AddMouseWheelEvent(scroll.x, scroll.y);
			}

			MainView();

			if (getMod()->getSavedValue<bool>("debug-windows", false)) {
				ImGui::ShowMetricsWindow();
			}

			// ime fuckery for mobile
			if (GEODE_DESKTOP(false and) true) if (ImGui::IsMouseReleased(0)) {
				static Ref<TextInput> inpNodeRef;
				if (!inpNodeRef) {
					inpNodeRef = TextInput::create(100.f, "xd", "geode.loader/mdFont.fnt");
					inpNodeRef->getInputNode()->m_allowedChars = " !\"#$ % &'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
					inpNodeRef->setCallback(
						[](const std::string& str) {
							ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
							ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);
							ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);
							ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
							ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, true);
							ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, false);
							ImGui::GetIO().AddInputCharactersUTF8(str.c_str());
							auto curPos = inpNodeRef->getInputNode()->m_textField->m_uCursorPos;
							if (curPos != -1) { //-1 is the cursor at the end
								for (auto a : str) {
									ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftArrow, true);
									ImGui::GetIO().AddKeyEvent(ImGuiKey_LeftArrow, false);
								}
								for (auto c = 0; c < curPos; ++c) {
									ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
									ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, false);
								}
							}
						}
					);
					log::info("Created text input node for ImGui: {}", inpNodeRef);
				}
				if (inpNodeRef) {
					if (ImGui::GetIO().WantTextInput) queueInMainThread([] {
						ImGuiInputTextState& State = GImGui->InputTextState;
						static int imguicurpos_onclick;
						imguicurpos_onclick = State.GetCursorPos();

						if (State.TextLen) {
							inpNodeRef->setString(std::string(State.TextA.Data, State.TextLen));

#ifndef GEODE_IS_IOS // CCIMEDispatcher::sharedDispatcher() = imac 0x4a89a0, m1 0x411d04;
							for (auto c : inpNodeRef->getString()) {
								CCIMEDispatcher::sharedDispatcher()->dispatchInsertText("a", 1, KEY_Left);
							}
							for (auto c = 0; c < imguicurpos_onclick; ++c) {
								CCIMEDispatcher::sharedDispatcher()->dispatchInsertText("a", 1, KEY_Right);
							}
#else
							inpNodeRef->setString("");
#endif

							inpNodeRef->focus();
							inpNodeRef->getInputNode()->onClickTrackNode(true);

							//inpNodeRef->setPosition(CCScene::get()->getContentSize() / 2.f);
							//inpNodeRef->removeFromParentAndCleanup(false);
							//CCScene::get()->addChild(inpNodeRef);
						}
						});
					else {
						inpNodeRef->defocus();
						inpNodeRef->getInputNode()->onClickTrackNode(false);
					}
				}
			}
        }
    );

	auto list = getMod()->getSavedValue<std::string>("installed-list");
	for (auto mod : Loader::get()->getAllMods()) {
		auto source_url = mod->getMetadata().getLinks().getSourceURL().value_or("");

		auto repo = source_url;

		std::regex pattern(
			"(?:https?:\\/\\/)?(?:www\\.)?github\\.com\\/"
			"([a-zA-Z0-9_-]+)"
			"\\/"
			"([a-zA-Z0-9_-]+)"
			"(?:\\.git|\\/)?",
			std::regex_constants::icase
		);
		std::smatch matches;
		if (std::regex_search(repo, matches, pattern)) {
			if (matches.size() >= 3) {
				repo = matches[1].str() + "/" + matches[2].str();
			}
		}

		if (!repo.empty() and list.find(repo) == std::string::npos) {
			list += "\n" + repo;
		};
	}
	getMod()->setSavedValue("installed-list", list);
}
$on_mod(Loaded) { wLoaded(); }

inline void wDataSaved() {
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
}
$on_mod(DataSaved) { wDataSaved(); }


#include <Geode/modify/CCMenuItemSpriteExtra.hpp>
class $modify(OpenModsMenuItemActivateHook, CCMenuItemSpriteExtra) {
	static void openUI() {
		createQuickPopup("epik closo listnerrr", "", "", "", [](void*, bool) {
			ImGuiCocos::get().setVisible(false);
			});
		ImGuiCocos::get().setVisible(true);
	}
	void activate() {
		if (this->getID() == "mods-add-button") createQuickPopup(
			"Adding Mods",
			"Are you want to...",
			" Install ", 
			"Download"
			, [__this = Ref(this)](void*, bool download) {
				if (download) openUI();
				else (__this->m_pListener->*__this->m_pfnSelector)(__this);
			}
		);
		else CCMenuItemSpriteExtra::activate();
	}
};