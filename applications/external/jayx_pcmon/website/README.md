# JAYX Project Website

This folder contains the source code for the JAYX static landing page and interactive simulator. It is designed to be hosted directly on GitHub Pages.

## Features Included
* **Interactive Flipper Zero Simulator**: Clicking the controls (Up/Down/Left/Right/OK/Back) cycles through real monochrome system screenshots of the JAYX app.
* **Agent Console Preview**: Terminal tabs showing build (`ufbt launch`) and runtime (`monitor.py`) console logs.
* **Responsive Layout**: Adapts smoothly to mobile, tablet, and desktop viewports.
* **Quickstart Commands**: Clean copyable terminal commands with click-to-clipboard actions.

---

## 1. Running Locally for Development

To test the website locally, run a simple local web server from inside the `website/` directory.

### Using Python (Native on most environments)
Open PowerShell or Command Prompt, move to this folder, and run:
```powershell
python -m http.server 8000
```
Then open your browser and navigate to `http://localhost:8000`.

### Using Node.js (Alternative)
```sh
npx serve
```
Then open `http://localhost:3000`.

---

## 2. Deploying to GitHub Pages

There are two primary methods to host this site on GitHub Pages under your repository:

### Method A: Deploy from a subfolder using GitHub Actions (Recommended)
You can configure a GitHub Action to automatically build and deploy only the `website/` folder whenever you push changes to the `main` branch.

1. In your repository root, create the directory structure `.github/workflows/` if it doesn't exist.
2. Create a new file named `deploy.yml` with the following configuration:

```yaml
name: Deploy website to Pages

on:
  push:
    branches: ["main"]
    paths:
      - 'website/**'
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Setup Pages
        uses: actions/configure-pages@v4

      - name: Upload Pages Artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: './website' # Deploys only the website folder

      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

3. Push the workflow file to GitHub.
4. On GitHub, go to your repository **Settings** → **Pages**.
5. Under **Build and deployment** → **Source**, select **GitHub Actions**.
6. The website will automatically deploy on the next push or when you trigger it manually from the Actions tab!

---

### Method B: Deploying manually via `gh-pages` branch
If you prefer not to use GitHub Actions, you can push the `website/` directory directly to a separate `gh-pages` branch:

Run the following commands in the root of your project:

```powershell
# Commit your changes first
git add .
git commit -m "Add website build"

# Push the website subfolder to the gh-pages branch
git subtree push --prefix website origin gh-pages
```

On GitHub, go to your repository **Settings** → **Pages**. Under **Build and deployment**, select **Deploy from a branch** and choose `gh-pages` (root folder `/`).
