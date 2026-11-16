// SPDX-License-Identifier: GPL-2.0-only

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr char panelHotkeyName[] = "obs-preview-toggle.panel";

QWidget *findPreviewCenter(QMainWindow *window)
{
	auto *center = window->centralWidget();
	if (!center || center->objectName() != QStringLiteral("centralwidget"))
		return nullptr;

	auto *preview = center->findChild<QWidget *>(QStringLiteral("preview"));
	auto *container = center->findChild<QWidget *>(QStringLiteral("previewContainer"));
	auto *placeholder = center->findChild<QWidget *>(QStringLiteral("previewDisabledWidget"));
	if (!preview || !container || !container->isAncestorOf(preview) || !placeholder)
		return nullptr;

	return center;
}

class PreviewToggle final : public QObject {
public:
	PreviewToggle(QMainWindow *window, QWidget *previewCenter, QAction *action)
		: center(previewCenter),
		  toolsAction(action),
		  menu(new QMenu(window))
	{
		toolsAction->setMenu(menu);
		auto *panel = menu->addAction(QString(), this, &PreviewToggle::togglePanel);
		auto *render = menu->addAction(QString(), this, &PreviewToggle::toggleRendering);
		auto *both = menu->addAction(QString(), this, &PreviewToggle::toggleBoth);
		connect(menu, &QMenu::aboutToShow, this, [this, panel, render, both] {
			panel->setText(obs_module_text(hidden ? "ShowPanel" : "HidePanel"));
			const bool rendering = obs_frontend_preview_enabled();
			render->setText(obs_module_text(rendering ? "DisableRendering" : "EnableRendering"));
			both->setText(obs_module_text("HideBoth"));
			const bool enabled = available();
			panel->setEnabled(enabled);
			render->setEnabled(enabled);
			both->setEnabled(enabled);
			both->setVisible(!hidden);
		});

		hotkey = obs_hotkey_register_frontend(
			panelHotkeyName, obs_module_text("ToggleHotkey"),
			[](void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
				if (pressed) {
					auto *controller = static_cast<PreviewToggle *>(data);
					QMetaObject::invokeMethod(controller, &PreviewToggle::togglePanel,
								  Qt::QueuedConnection);
				}
			},
			this);
		loadHotkey();
	}

	~PreviewToggle() override
	{
		if (hotkey != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(hotkey);
		if (center && hidden)
			center->show();
		delete toolsAction.data();
		delete menu.data();
	}

	void loadHotkey()
	{
		auto *profile = obs_frontend_get_profile_config();
		if (!profile || hotkey == OBS_INVALID_HOTKEY_ID)
			return;

		const char *json = config_get_string(profile, "Hotkeys", panelHotkeyName);
		auto *data = json ? obs_data_create_from_json(json) : nullptr;
		auto *bindings = data ? obs_data_get_array(data, "bindings") : nullptr;
		obs_hotkey_load(hotkey, bindings);
		obs_data_array_release(bindings);
		obs_data_release(data);
	}

	void enterStudioMode()
	{
		// 保留 Studio Mode 的 Preview、Program 與轉場控制。
		if (center && hidden)
			center->show();
		hidden = false;
	}

	void restoreRendering()
	{
		if (!restoreRender || obs_frontend_preview_program_mode_active())
			return;

		restoreRender = false;
		if (!obs_frontend_preview_enabled())
			obs_frontend_set_preview_enabled(true);
	}

private:
	bool available() const { return center && !obs_frontend_preview_program_mode_active(); }

	void togglePanel()
	{
		if (!available())
			return;

		hidden = !hidden;
		// 收起 QMainWindow 的中央容器，讓停駐視窗填補空間。
		center->setVisible(!hidden);
		if (!hidden)
			restoreRendering();
	}

	void toggleRendering()
	{
		if (!available())
			return;

		restoreRender = false;
		obs_frontend_set_preview_enabled(!obs_frontend_preview_enabled());
	}

	void toggleBoth()
	{
		if (!available() || hidden)
			return;

		if (obs_frontend_preview_enabled()) {
			restoreRender = true;
			obs_frontend_set_preview_enabled(false);
		}
		togglePanel();
	}

	QPointer<QWidget> center;
	QPointer<QAction> toolsAction;
	QPointer<QMenu> menu;
	obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;
	bool hidden = false;
	bool restoreRender = false;
};

PreviewToggle *controller = nullptr;

void onFrontendEvent(obs_frontend_event event, void *);

void unload()
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	if (controller)
		controller->restoreRendering();
	delete controller;
	controller = nullptr;
}

void onFrontendEvent(obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		unload();
		return;
	}

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING && !controller) {
		auto *window = qobject_cast<QMainWindow *>(static_cast<QWidget *>(obs_frontend_get_main_window()));
		auto *center = window ? findPreviewCenter(window) : nullptr;
		if (!center) {
			obs_log(LOG_WARNING, "Unsupported OBS preview layout; panel controls are unavailable.");
			return;
		}

		auto *action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(obs_module_text("ToolMenu")));
		if (action)
			controller = new PreviewToggle(window, center, action);
		return;
	}

	if (!controller)
		return;

	switch (event) {
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
		controller->enterStudioMode();
		break;
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		controller->restoreRendering();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		controller->loadHotkey();
		break;
	default:
		break;
	}
}

} // namespace

bool obs_module_load(void)
{
	// 較舊的 OBS SDK 仍可用於連結，但首版只支援 OBS 33 起的預覽控制。
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(33, 0, 0)) {
		obs_log(LOG_WARNING, "OBS Studio 33 or newer is required.");
		return false;
	}

	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	obs_log(LOG_INFO, "Loaded version %s", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	if (controller)
		unload();
}
