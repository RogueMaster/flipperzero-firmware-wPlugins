// Offline cache for the documentation site.
// Bumping CACHE invalidates everything, which is what a rebuild wants.
const CACHE = "meshcore-docs-v1";
const FILES = [
  "./",
  "./index.html",
  "./app-guide.html",
  "./contacts-and-mesh.html",
  "./guide-sergey.html",
  "./guide-mark.html",
  "./meshlog.py",
  "./manifest.webmanifest"
];

self.addEventListener("install", (event) => {
  event.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)));
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

// Cache first: in the field there is no network, and a stale page beats no page.
self.addEventListener("fetch", (event) => {
  if (event.request.method !== "GET") return;
  event.respondWith(
    caches.match(event.request).then((hit) => hit || fetch(event.request))
  );
});
