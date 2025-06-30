# ConfigUpdater

A Notepad++ Plugin that allows easy interface to be able to update the `langs.xml`, `stylers.xml`, and `themes\` files, based on the most recent Notepad++ release, so that your syntax highlighting will always keep up with improvements.

## Explanation

When Notepad++ updates, it tends to avoid updating important things like `langs.xml`, `stylers.xml`, and your themes; it does this because it doesn't want to overwrite your customizations, but that has the drawback that you end up missing new styles, new languages, and updated keyword lists.

This plugin uses the `langs.model.xml` and `stylers.model.xml` which ship with every copy of Notepad++, and looks for any information that's in the model files, but not in your active config files.  It will copy over that missing information; when you then restart Notepad++, the syntax highlighting and keyword lists will have been updated to include all the settings in your current Notepad++ version.

## Installation

You can install the plugin using Notepad++'s **Plugins Admin**, or by unzipping the appropriate archive from the [latest release](https://github.com/pryrt/NppPlugin-ConfigUpdater/releases/latest), so that `ConfigUpdater.dll` ends up in the `c:\Program Files\Notepad++\plugins\ConfigUpdater\` directory (or equivalent, for your installation's location).  (The files in the `docs\` folder from the zipfile could also be extracted into `c:\Program Files\Notepad++\plugins\ConfigUpdater\docs\` directory, but that's not required).

## Update Config Files to match model files

Run **Plugins > ConfigUpdater > Update Config Files** to run the config-file updater.  It will go through your AppData langs, stylers, and themes, and also go through any themes in your installation directory, and bring in the missing or updated syntax-highlighting settings from the new version, without losing any of your customizations.  If your "normal" user doesn't have write permission for your installation directory (which often happens), it will ask if you want to restart Notepad++ in "elevated (UAC)" mode (aka, "As Admin" or "As Administrator") -- if you say **yes**, Notepad++ will be closed, and it will attempt to restart As Admin (Windows may prompt you for elevated UAC permissions); if you say **no**, it won't try to restart for that file, but it will ask you again if another file doesn't have write permission; if you **cancel**, the plugin will stop asking you to restart.  If it restarts in elevated UAC mode, you will have to run **Plugins > ConfigUpdater > Update Config Files** again to restart the process.  After it is done running, if you are in elevated UAC mode, it will ask if you want to restart Notepad++ normally (because it's not a good idea to keep Notepad++ running As Admin if you don't need it, since you can accidentally overwrite important system config files in that mode).

After your config files have been updated, you can exit Notepad++ and run it again, and the updated Languages and Style Configurator settings will be in effect.

## [COMING SOON] Download Newest Model Files

[COMING SOON] Run **Plugins > ConfigUpdater > Download Newest Model Files** to grab the most recent `*.model.xml` from the Notepad++ repository.

The model files are the files that the ConfigUpdater uses to verify which languages and styles need to be updated in your config files, so having the most-recent model files is important.

When you upgrade Notepad++ using the installer, it will automatically update the installed `*.model.xml` files, so if you have a normal installation, you shouldn't need to use this action.  However, if you have a portable Notepad++, even if you have been manually grabbing the new executable and DLLs from a new portable unzip, you may have forgotten to also grab `*.model.xml` from the zipfile into your portable location -- so running this action in the ConfigUpdater plugin will remedy that.  

Further, whether in installed or portable, it can sometimes be useful to grab a newer copy of the model files than were available for your version: these might reveal hidden styles for languages you already have, or newly-updated keyword lists, so if you know there have been updates to the models since you last upgraded your Notepad++, you can get your stylers, themes, and language settings ahead of the curve.

## Validation

Historically, some of the themes have had XML problems, such as two styles in the same language with the same styleID value: Notepad++ silently ignores these, but then it is uncertain to the user which line of the XML actually gets used for that styleID.  Starting with v2.1 of the plugin, it will enable validation of the XML, so you can find (and solve) such problems: There are two ways of getting XML validation to occur on the XML:

1. When running **Update Config Files**, the plugin will run validation on each XML after it's been updated and saved.  After it is done updating, the restart-NPP popup will let you know if there was a validation error; if so, it is recommended that you say **No** to rebooting, then allow the next prompt to launch the Validation dialog for you.  It will prompt you with information about the validation failure, and ask if you want to edit the file:
    - ![](./.updater-validator-restart.png)
    - ![](./.updater-validator.png)

2. **Plugins > ConfigUpdater > Validate Config Files** will open a dialog:
    - You can choose one of the config files from the **Files** dropdown: `stylers.xml`, any of the themes, or `langs.xml`
    - Once a file is chosen, running **Validate** will validate the current file.
        - If the XML is good, you will see a message to that effect, and can then choose another file:
          ![](./.validator-passed.png)
        - If the XML has problems, each line of the listbox will give a linenumber in the file where the problem exists, along with the error message.  Double-clicking on this line will open the config file to that linenumber, so that you can make changes
          ![](./.validator-failed.png)
        - Running **Validate** again will re-validate the same file: if you have fixed all the problems, it will give you the SUCCESS message.
    - Using the **\*.model.xml** button (which will rename to **stylers.model.xml** or **langs.model.xml** depending on the Files dropdown value) will put the model file in the other view, and will attempt to scroll both files to approximately the same location, so that you can see what the model does for that particular language, so that you can investigate the difference between the language and its model, to try to fix the error.  (Starting with Notepad++ v8.8.2, the model files will have been validated in the source repo before being distributed, so they are "known good".)
    - **Done** will exit the dialog with no further interaction or editing.

## Notes

- This plugin can be run after each time you upgrade to a new version of Notepad++, to keep your settings in sync with updates to the model versions that ship with Notepad++.

- This plugin will work whether you are using a normal Notepad++ installation using AppData for you settings, or whether you have chosen Cloud Directory for your settings, or are using the `-settingsDir` command-line option, or whether you are running a portable (`doLocalConf.xml`) version of Notepad++, and should search and update the files in the correct directories in any of those circumstances.  (For it to do anything with a portable Notepad++, you have to make sure that you have copied the `*.model.xml` files from the new portable unzip into your active portable copy; if the `*.model.xml` aren't changed, ConfigUpdater won't know it has to update anything in your langs or stylers or themes.)
