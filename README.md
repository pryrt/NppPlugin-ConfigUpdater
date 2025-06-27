# ConfigUpdater

A Notepad++ Plugin that allows easy interface to be able to update the `langs.xml`, `stylers.xml`, and `themes\` files, based on the most recent Notepad++ release, so that your syntax highlighting will always keep up with improvements.

## Explanation

When Notepad++ updates, it tends to avoid updating important things like `langs.xml`, `stylers.xml`, and your themes; it does this because it doesn't want to overwrite your customizations, but that has the drawback that you end up missing new styles, new languages, and updated keyword lists.

This plugin uses the `langs.model.xml` and `stylers.model.xml` which ship with every copy of Notepad++, and looks for any information that's in the model files, but not in your active config files.  It will copy over that missing information; when you then restart Notepad++, the syntax highlighting and keyword lists will have been updated to include all the settings in your current Notepad++ version.

## Installation

You can install the plugin using Notepad++'s **Plugins Admin**, or by unzipping the appropriate archive from the [latest release](https://github.com/pryrt/NppPlugin-ConfigUpdater/releases/latest), so that `ConfigUpdater.dll` ends up in the `c:\Program Files\Notepad++\plugins\ConfigUpdater\` directory (or equivalent, for your installation's location).  (The files in the `docs\` folder from the zipfile could also be extracted into `c:\Program Files\Notepad++\plugins\ConfigUpdater\docs\` directory, but that's not required).

## Usage

Run **Plugins > ConfigUpdater > Update Config Files** to run the config-file updater.  It will go through your AppData langs, stylers, and themes, and also go through any themes in your installation directory, and bring in the missing or updated syntax-highlighting settings from the new version, without losing any of your customizations.  If your "normal" user doesn't have write permission for your installation directory (which often happens), it will ask if you want to restart Notepad++ in "elevated (UAC)" mode (aka, "As Admin" or "As Administrator") -- if you say **yes**, Notepad++ will be closed, and it will attempt to restart As Admin (Windows may prompt you for elevated UAC permissions); if you say **no**, it won't try to restart for that file, but it will ask you again if another file doesn't have write permission; if you **cancel**, the plugin will stop asking you to restart.  If it restarts in elevated UAC mode, you will have to run **Plugins > ConfigUpdater > Update Config Files** again to restart the process.  After it is done running, if you are in elevated UAC mode, it will ask if you want to restart Notepad++ normally (because it's not a good idea to keep Notepad++ running As Admin if you don't need it, since you can accidentally overwrite important system config files in that mode).

After your config files have been updated, you can exit Notepad++ and run it again, and the updated Languages and Style Configurator settings will be in effect.

## Validation

Historically, some of the themes have had XML problems, such as two styles in the same language with the same styleID value: Notepad++ silently ignores these, but then it is uncertain to the user which line of the XML actually gets used for that styleID.  Starting with v2.1 of the plugin, it will enable validation of the XML, so you can find (and solve) such problems: There are two ways of getting XML validation to occur on the XML:

1. When running **Update Config Files**, the plugin will run validation on each XML after it's been updated and saved.  It will prompt you with information about the validation failure, and ask if you want to edit the file:
    - ![](./.updater-validator.png)
    - **Yes**: The loop through updating the config files will stop. It will open the failing config file, to the appropriate line when possible. It will append a message to the logfile.  (That last means that the logfile, instead of the config file, might keep focus in Notepad++).
    - **No**: The loop through updating the config files will continue.  It will _not_ open this config file, and it will not ask on future validation errors, either (Essentially, this is "No to All").  It will still append a message into the logfile for this config file, and any of the remaining files that have validation errors.
    - **Cancel**: The loop through updating the config files will stop, but the plugin will _not_ open this config file.

3. **Plugins > ConfigUpdater > Validate Config Files** will open a dialog:
    - You can choose one of the config files from the **Files** dropdown: `stylers.xml`, any of the themes, or `langs.xml`
    - Once a file is chosen, running **Validate** will validate the current file.
        - If the XML is good, you will see a message to that effect, and can then choose another file:
          ![](./.validator-passed.png)
        - If the XML has problems, each line of the listbox will give a linenumber in the file where the problem exists, along with the error message.  Double-clicking on this line will open the config file to that linenumber, so that you can make changes
          ![](./.validator-failed.png)
        - Running **Validate** again will re-validate the same file: if you have fixed all the problems, it will give you the SUCCESS message.
    - **Done** will exit the dialog with no further interaction or editing.

## Notes

- This plugin can be run after each time you upgrade to a new version of Notepad++, to keep your settings in sync with updates to the model versions that ship with Notepad++.

- This plugin will work whether you are using a normal Notepad++ installation using AppData for you settings, or whether you have chosen Cloud Directory for your settings, or are using the `-settingsDir` command-line option, or whether you are running a portable (`doLocalConf.xml`) version of Notepad++, and should search and update the files in the correct directories in any of those circumstances.  (For it to do anything with a portable Notepad++, you have to make sure that you have copied the `*.model.xml` files from the new portable unzip into your active portable copy; if the `*.model.xml` aren't changed, ConfigUpdater won't know it has to update anything in your langs or stylers or themes.)
