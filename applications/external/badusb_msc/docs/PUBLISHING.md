# Publish BadUSB MSC v0.1 to GitHub

**The ZIP is source code to upload. It does not automatically create a GitHub repository or install the app.**

1. Open [GitHub → New repository](https://github.com/new). Suggested repository name: **`badusb-msc`**. Set visibility to **Public** if you want the source to be available to everyone. Do **not** pre-create a README, `.gitignore`, or license when creating the empty repository: they are already included here.
2. **Extract** `BadUSB_MSC_v0.1_GITHUB_RELEASE.zip` on your computer. GitHub does not extract ZIP contents when you upload a ZIP as one file.
3. Open the new GitHub repository and choose **Add file → Upload files**. Upload the **contents** of the extracted folder so `application.fam`, `README.md`, `LICENSE`, and `helpers/` appear directly in the repository root; preserve directory structure. Commit the files.
4. Confirm that GitHub displays the README, GPLv3 LICENSE, and the author name **ZombieDolphin**. Check that no local `drive.img`, personal files or private settings were uploaded.
5. In the extracted source folder, run `ufbt -j 1` with an SDK matching the target firmware. Install and test `dist/badusb_msc.fap` on your own Flipper and Windows PC. **Do not claim binary compatibility until tested.**
6. After testing, optionally choose **Releases → Draft a new release**, create a tag named **`v0.1`**, and attach the validated FAP for the exact firmware/SDK it targets. If testing is not complete, publish the source repository first and create a binary release later.

The core repo metadata is `application.fam`, which declares `appid=badusb_msc`, `fap_version=0.1` and `fap_author=ZombieDolphin`.

**Flipper Lab and Unleashed submissions are separate future steps.** No repository, release or submission was created by preparing this ZIP.
