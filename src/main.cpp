/**
 * Here we include the ImGui-Cocos library, which is a wrapper around the ImGui library for Cocos2d-x.
 */
#include <imgui-cocos.hpp>

/**
 * Custom Keybinds mod provides a way to bind custom key combinations to actions in the game.
 * We will use it to bind open/close button for out ImGui interface.
 */
#include <geode.custom-keybinds/include/Keybinds.hpp>

/**
 * Now, we should define our setup and draw callbacks for ImGui.
 * `setup` will be called once, when the mod is loaded and ImGui is ready to be used.
 * (It can also be called when you change game resolution or switch between windowed and fullscreen modes.)
 * `draw` will be called every frame, and it's where we should put our ImGui code.
 */
void setup() {
    /**
     * This function should be used for things like setting up ImGui style, loading fonts, etc.
     * For this example, we will set up a custom font (which is stored in our mod resources).
     */
    auto& io = ImGui::GetIO();
    auto fontPath = geode::Mod::get()->getResourcesDir() / "Roboto-Regular.ttf";
    io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f);
}

void draw() {
    /**
     * This function should be used for drawing ImGui widgets.
     * You can put any ImGui code here, and it will be rendered on the screen.
     */
    ImGui::Begin("Hello, ImGui!");

    ImGui::Text("This is a simple ImGui window.");
    ImGui::Text("You can put any ImGui widgets here.");

    if (ImGui::Button("Close")) {
        /* This will hide our ImGui interface. */
        ImGuiCocos::get().toggle();
    }

    ImGui::End();
}

/**
 * Now we need to initialize ImGuiCocos and set our setup and draw callbacks.
 * Ideally, we do this when Geometry Dash window is opened, but this should be the case unless you use "early-load" flag.
 *
 * In case you use "early-load" flag, you're probably better off hooking MenuLayer::init() and doing it there (make sure it's called only once).
 * Here's what you could make for an early-load mod:
 * ```
 * #include <Geode/modify/MenuLayer.hpp>
 * class $modify(MenuLayer) {
 *     bool init() {
 *         if (!MenuLayer::init()) return false;
 *         static bool initialized = false;
 *         if (!initialized) {
 *             ImGuiCocos::get()
 *                 .setup(setup).draw(draw)
 *                 .setVisible(false);
 *             initialized = true;
 *         }
 *         return true;
 *     }
 * };
 * ```
 */
$on_mod(Loaded) {
    ImGuiCocos::get()
        .setup(setup).draw(draw)
        .setVisible(false); /* We don't want our ImGui interface to be visible by default. */
}

/**
 * $execute is a special macro that allows us to execute code when our mod first loads.
 * Then we will use Custom Keybinds API to register a new keybind for opening/closing our ImGui interface.
 */
$execute {
    /**
     * Bringing some namespaces into scope for easier access to classes and functions.
     */
    using namespace geode::prelude;
    using namespace keybinds;

    BindManager::get()->registerBindable({
        "open-imgui"_spr, /* Keybind ID */
        "Open Interface", /* Keybind name */
        "Open or close the ImGui interface.", /* Keybind description */
        { Keybind::create(cocos2d::enumKeyCodes::KEY_P, Modifier::Alt) },
        "My ImGui Mod" /* Category name (usually the name of your mod) */
    });
    new EventListener([=](InvokeBindEvent* event) {
        if (event->isDown()) ImGuiCocos::get().toggle();
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "open-imgui"_spr));
}
