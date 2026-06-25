#!/usr/bin/env bash
set -euo pipefail

git init
git add .
git commit -m "Initial NFC QR Presenter FAP"
git branch -M main
git remote add origin https://github.com/RogerF5-Security/NFC_QR_Presenter.git
git push -u origin main
