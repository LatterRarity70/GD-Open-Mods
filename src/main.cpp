#include <imgui-cocos-with-addons/include.hpp>
using namespace geode::prelude;

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
void Browser(bool reload = false) {

	if (reload) REPO_LIST = "";
	if (reload) LOADED_REPOS.clear();

	static std::map<std::string, bool> REPO_WAS_LOADED;
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
		auto req = web::WebRequest();
		listener->setFilter(req.get("https://raw.githubusercontent.com/user95401/GD-Open-Mods/refs/heads/main/_list.txt"));
		
		return;
	};

	for (auto repo_gdstr : string::split(REPO_LIST, "\n")) {
		auto repo = std::string(repo_gdstr.c_str()); //i never will trust gd::string
		if (repo.size() < 2) continue;

		if (not REPO_WAS_LOADED[repo]) {
			REPO_WAS_LOADED[repo] = true;

			auto listener = new EventListener<web::WebTask>();
			listener->bind(
				[=](web::WebTask::Event* e) mutable {
					if (web::WebResponse* res = e->getValue()) {
						LOADED_REPOS[repo] = res->json().unwrapOrDefault();
						if (listener) delete listener;
					}
				}
			);
			auto req = web::WebRequest();
			listener->setFilter(req.get("https://ungh.cc/repos/" + repo));

			return;
		};

		static std::map<std::string, bool> hovered;
		auto flags = ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY;
		if (hovered[repo]) {
			flags |= ImGuiChildFlags_Borders;
			flags |= ImGui::IsMouseDown(ImGuiMouseButton_Left) ? ImGuiChildFlags_None : ImGuiChildFlags_FrameStyle;
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				INFO_REPO = LOADED_REPOS[repo]["repo"];
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
				auto req = web::WebRequest();
				listener->setFilter(req.get("https://ungh.cc/repos/" + repo + "/readme"));
			}
		}
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImGui::GetStyle().WindowPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
		ImGui::BeginChild((repo + " Repo Item").c_str(), {0, 0}, flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		{
			ImGui::Markdown("**" + repo + "**\n" + LOADED_REPOS[repo]["repo"]["description"].asString().unwrapOr("Loading..."));
			hovered[repo] = ImGui::IsWindowHovered();
		}
		ImGui::EndChild();
	}
}

void Favorites(bool reload = false) {
	ImGui::Markdown("# Favorites...");
}

void Installed(bool reload = false) {
	ImGui::Markdown("# Installed...");
}

inline static auto LOADED_RELEASES = std::map<std::string, matjson::Value>();
void RELEASES(bool reload = false) {
	auto repo = INFO_REPO_KEY_DUMP("repo");
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
					LOADED_RELEASES[repo] = res->json().unwrapOrDefault()["releases"];
					if (listener) delete listener;
				}
			}
		);
		auto req = web::WebRequest();
		listener->setFilter(req.get("https://ungh.cc/repos/" + repo + "/releases"));

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

		ImGui::Markdown(
			+ "## " + RELEASE_KEY_DUMP("name")
			+ "\n**" + RELEASE_KEY_DUMP("author")
			+ "** published at " + IsoToReadable(RELEASE_KEY_DUMP("publishedAt"))
			+ " with " + RELEASE_KEY_DUMP("tag") + " tag."
		);
		ImGui::Separator();
		ImGui::Markdown(release["markdown"].asString().unwrapOrDefault());
		ImGui::Separator();

		for (auto asset : release["assets"]) {
#define ASSET_KEY_DUMP(key) string::replace(asset[key].dump(), "\"", "")
			auto filename = std::filesystem::path(ASSET_KEY_DUMP("downloadUrl")).filename();

			if (ImGui::Button(filename.string().c_str())) {
				showDownloadProgress = true;
				downloadProgress = 0.0f;
				statusText = "Downloading...";

				auto listener = new EventListener<web::WebTask>();
				listener->bind(
					[=](web::WebTask::Event* e) mutable {
						if (web::WebResponse* res = e->getValue()) {
							res->into(dirs::getModsDir() / filename);
							showDownloadProgress = false;
							if (listener) delete listener;
						}
						else if (web::WebProgress* p = e->getProgress()) {
							showDownloadProgress = true;
							downloadProgress = p->downloadProgress().value_or(0.f) / 100.f;
							statusText = fmt::format("Downloading {}: {:.1f}%", filename, downloadProgress * 100.f);
						}
					}
				);
				auto req = web::WebRequest();
				listener->setFilter(req.get(ASSET_KEY_DUMP("downloadUrl")));
			}

			ImGui::SameLine();

			if (ImGui::BeginChild(("##childf" + ASSET_KEY_DUMP("downloadUrl")).c_str(), {}, ImGuiChildFlags_AutoResizeY)) {
				ImGui::Markdown(
					"| " + FormatFileSize(asset["size"].asInt().unwrapOr(0)) +
					", " + ASSET_KEY_DUMP("downloadCount") +
					" downloads, updated at " + IsoToReadable(ASSET_KEY_DUMP("updatedAt"))
				);
			}
			ImGui::EndChild();
		}

		ImGui::EndChild();
	}

	if (showDownloadProgress) {
		if (ImGui::BeginTooltip()) {
			ImGui::Text("%s", statusText.c_str());
			ImGui::ProgressBar(downloadProgress);
		}
		ImGui::EndTooltip();
	}
}

inline void wLoaded() {
    ImGuiCocos::get().setup(
        [] {
            ImGuiIO& io = ImGui::GetIO();

            auto RobotoItalic       = geode::Mod::get()->getResourcesDir() / "Roboto-Italic.ttf";
            auto RobotoBold         = geode::Mod::get()->getResourcesDir() / "Roboto-Bold.ttf";
            auto RobotoBoldItalic   = geode::Mod::get()->getResourcesDir() / "Roboto-BoldItalic.ttf";
            auto RobotoRegular      = geode::Mod::get()->getResourcesDir() / "Roboto-Regular.ttf";

            auto fsize      = 32.0f;
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

            io.MouseSource = ImGuiMouseSource_TouchScreen;
            io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
            io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;

            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavNoCaptureKeyboard;
            io.ConfigNavMoveSetMousePos = true;
            io.NavActive = true;
            io.NavVisible = true;

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

            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos);
            ImGui::SetNextWindowSize(ImGui::GetMainViewport()->WorkSize);
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
            if (ImGui::Begin(geode::getMod()->getName().c_str(), nullptr, window_flags))
            {

				ImGui::BeginChild("Listbox",
					{ ImGui::GetContentRegionAvail().x * 0.35f, 0 },
					ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoDecoration
				); {
					if (ImGui::BeginTabBar("Tabs", ImGuiTabBarFlags_DrawSelectedOverline)) {
						if (ImGui::BeginTabItem("Browser")) {
							auto activated = ImGui::IsItemActivated();
							ImGui::BeginChild("Browser""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
							Browser(activated);
							ImGui::EndChild();
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Favorites")) {
							ImGui::BeginChild("Favorites""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
							Favorites();
							ImGui::EndChild();
							ImGui::EndTabItem();
						}
						if (ImGui::BeginTabItem("Installed")) {
							ImGui::BeginChild("Installed""box", { 0, 0 }, ImGuiChildFlags_FrameStyle);
							Installed();
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
						ImGui::Markdown(
							+ "## " + INFO_REPO["name"].asString().unwrapOr("...")
							+ "\n" + INFO_REPO["description"].asString().unwrapOr("...")
						);
						ImGui::Separator();
						ImGui::Markdown(
							+ "**Stars:** " + INFO_REPO_KEY_DUMP("stars")
							+ " | **Watchers:** " + INFO_REPO_KEY_DUMP("watchers")
							+ " |  **Forks:** " + INFO_REPO_KEY_DUMP("forks")
							+ " | " + IsoToReadable(INFO_REPO_KEY_DUMP("createdAt"))
							+ " (upd: " + IsoToReadable(INFO_REPO_KEY_DUMP("updatedAt")) + ")"
						);
					};
					ImGui::EndChild();

					//
					ImGui::BeginChild("midbox", 
						{ 0, ImGui::GetContentRegionAvail().y - ImGui::CalcItemSize({ 0, 52 }, 0, 52).y },
						ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoDecoration
					); {

						if (ImGui::BeginTabBar("midbox tabs", ImGuiTabBarFlags_DrawSelectedOverline)) {

							if (ImGui::BeginTabItem("README")) {
								ImGui::BeginChild("READMEbox", { 0, 0 }, ImGuiChildFlags_FrameStyle);

								ImGui::Markdown(INFO_MD_BODY);

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

					ImGui::BeginChild("Bottom Line", { 0, 52 - 4 }, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeY);
					{
						ImGui::Markdown(
							+"[" + INFO_REPO_KEY_DUMP("repo")
							+ " - branch: " + INFO_REPO_KEY_DUMP("defaultBranch")
							+ ", id: " + INFO_REPO_KEY_DUMP("id") + "](https://github.com/" + INFO_REPO_KEY_DUMP("repo") + ")"
						);
					};
					ImGui::EndChild();
                };
                ImGui::EndChild();

            }
            ImGui::End();

            ImGui::ShowMetricsWindow();
            //ImGui::ShowDemoWindow();
            //ImGui::ShowDebugLogWindow();
            //ImGui::ShowStyleEditor();
        }
    );
}
$on_mod(Loaded) { wLoaded(); }

inline void wDataSaved() {
    ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
}
$on_mod(DataSaved) { wDataSaved(); }