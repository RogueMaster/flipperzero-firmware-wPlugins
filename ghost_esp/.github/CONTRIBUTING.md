# Making Contributions 

Contributions to this repo can be made by initiating a pull request from your local fork or by opening an [issue](https://github.com/jaylikesbunda/ghost_esp_app/issues).

Before submitting a new pull request please ensure that your changes act as intended and that you have been able to successfully build and test your implementations **BEFORE** submitting a PR.

## Setting up the build environment:

The easiest way to setup a build environment for flipper apps is by using the flipper [ufbt tool](https://github.com/flipperdevices/flipperzero-ufbt).

You can follow the instructions linked in their repo or follow the instructions provided below.

### Prerequisites:

You must have **python3** installed to ensure the most streamlined experience.

You must also have [forked](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks/fork-a-repo#fork-an-example-repository) and cloned this repo locally.


### Installing Build Tools:

1. Enter the folder you cloned the repo to ```$ cd ghost_esp_app```
2. Initialize a new Python venv ```$ python3 -m venv .venv```
3. Activate the new venv:
    - Linux: ```$ source .venv/bin/activate```
    - Windows CMD: ```C:\<path_to_repo> .venv\Scripts\activate.bat```
    - Windows powershell: ```PS C:\<path_to_repo>>.venv\Scripts\activate.bat```
4. Install the tool:
    - Linux & macOS: ```python3 -m pip install --upgrade ufbt```  
    - Windows: ```py -m pip install --upgrade ufbt```


## Building the App:
Now that your build tools are installed you can build your custom .fap

Before beginning ensure your python venv has been activated and that you are in the root folder of the repo (youll see the .fam file)

Now all you need to do is run ```$ ufbt``` within this folder. At this point the build tools will be downloaded, and the .fap will be built and placed within the ```/dist``` folder!

If needed you can clean your environment by running ```$ ufbt -c``` and this will delete any artifacts created by the build process.

## Uploading the App to Your Flipper

Uploading your custom build to your flipper is relatively straight forward. You can either connect your device and upload the file to your SD card via qflipper, or by connecting your SD card to your computer directly.

Once you have access to your SD card file system drag and drop your shiney new ```ghost_esp.fap``` application file into ```SD Card/apps```. If youre running non-standard firmware and would like to keep the file structure consististent I would suggest uploading your ```ghost_esp.fap``` to ```SD Card/apps/GPIO/ESP/```.

## Final steps:
Before submitting your PR be sure to verify the following:
1. Have you incremented the Changelog.md file, and tagged your changes?
2. Have you updated the version in fap_version.fam to reflect your
3. Have you successfully built and tested your changes?
4. Does your contribution address an open issue in the repo? Be sure to link to it!

If you've done these things then go ahead and submit your PR!