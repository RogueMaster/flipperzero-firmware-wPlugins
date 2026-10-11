# Publishing to the Flipper Apps Catalog

This folder holds a ready `manifest.yml` for submission to
[flipperdevices/flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog).

To submit:
1. Fork that repo.
2. Copy `manifest.yml` to `applications/<Category>/<appid>/manifest.yml`
   (Category = the app's `fap_category`; appid = the app's `appid`).
3. Update `commit_sha` to the exact commit you want published.
4. Open a PR.

Note: the catalog builds against **Official Firmware (OFW)**. These apps were
built with the Unleashed SDK; verify they compile on the current OFW SDK
(`ufbt update` default channel) before submitting. All APIs used are standard
(gui, storage, loader, dialogs, notification, infrared low-level), so an OFW
build is expected to work, but confirm first.
