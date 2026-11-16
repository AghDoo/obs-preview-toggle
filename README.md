# OBS Preview Toggle

[繁體中文](README.zh-TW.md)

OBS Preview Toggle is a small OBS Studio plugin that hides the native preview area so docked panels can use the available space. It can also switch OBS's preview rendering independently.

## Use

Open **Tools → OBS Preview Toggle** in normal preview mode:

- **Hide / Show Preview Area** collapses or restores the center of the main window, including the source toolbar. The render setting is left alone.
- **Disable / Enable Preview Rendering** changes OBS's native preview setting without changing the layout.
- **Hide Preview Area and Disable Rendering** does both. Showing the area restores rendering if this action disabled it. If you change rendering separately, that choice takes precedence.

Bind **OBS Preview Toggle: Show/Hide Preview Area** under **Settings → Hotkeys**. OBS saves the binding with the current profile. The panel starts visible each time OBS opens. Entering Studio Mode restores the panel and pauses the plugin's controls so the Preview, Program, and transition controls stay accessible.

## Windows x64 prototype

This prototype targets OBS Studio 33 on Windows x64. The plugin recognizes the native preview widgets before changing the layout. If a later OBS version changes the widget structure, the plugin logs a warning and skips the layout change. Check the [繁體中文 version](README.zh-TW.md#驗收) for the manual validation steps, including the DeckLink chain.

GitHub Actions builds the plugin on a Windows runner. Download the `obs-preview-toggle-windows-x64` artifact from the workflow run, then extract the versioned ZIP inside it.

Copy the extracted `obs-preview-toggle` folder to `C:\ProgramData\obs-studio\plugins\`. For [portable OBS 33](https://obsproject.com/kb/legacy-plugin-developer-info), copy it to `<OBS folder>\plugins\`. The DLL and the `data` directory sit side by side within the plugin folder. Restart OBS after copying the files. To remove the plugin, close OBS and remove that plugin folder.

See [LICENSE](LICENSE) for licensing.
