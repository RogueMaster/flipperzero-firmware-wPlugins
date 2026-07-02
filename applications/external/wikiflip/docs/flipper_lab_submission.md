# Publishing WikiFlip on Flipper Lab

Flipper Lab lists apps approved through the official Flipper Application Catalog. WikiFlip is published by opening a pull request to `flipperdevices/flipper-application-catalog` that points to this public GitHub repository.

## 1. Prepare the App Repository

The public app repository should contain:

```text
application.fam
wikiflip.c
wikiflip.h
README.md
LICENSE
docs/catalog_description.md
docs/changelog.md
images/wikiflip_10px.png
screenshots/*.png
```

The icon must be a 10x10 px, 1-bit PNG. Screenshots should be captured from qFlipper without changing their resolution or format.

## 2. Build Before Publishing

```powershell
ufbt
```

Verify the output:

```text
dist/wikiflip.fap
```

## 3. Create a Release Commit

```powershell
git add application.fam wikiflip.c wikiflip.h README.md LICENSE docs images screenshots
git commit -m "Release WikiFlip v1.3"
git tag -a v1.3 -m "WikiFlip v1.3"
git push origin main
git push origin v1.3
```

Record the source commit:

```powershell
git rev-parse HEAD
```

## 4. Prepare `manifest.yml`

Copy `docs/manifest.yml.example` and replace:

```text
https://github.com/YOUR_USERNAME/WikiFlip.git
REPLACE_WITH_SOURCE_COMMIT_SHA
```

The final file belongs in the catalog fork:

```text
applications/Tools/wikiflip/manifest.yml
```

## 5. Submit the Catalog Pull Request

```powershell
git clone https://github.com/YOUR_USERNAME/flipper-application-catalog.git
cd flipper-application-catalog
git checkout -b YOUR_USERNAME_wikiflip_1.3
mkdir applications\Tools\wikiflip
copy C:\path\manifest.yml applications\Tools\wikiflip\manifest.yml
git add applications\Tools\wikiflip\manifest.yml
git commit -m "Add WikiFlip v1.3"
git push origin YOUR_USERNAME_wikiflip_1.3
```

Open the pull request against:

```text
https://github.com/flipperdevices/flipper-application-catalog
```

After the pull request is approved and merged, WikiFlip will appear on:

```text
https://lab.flipper.net/apps
```
